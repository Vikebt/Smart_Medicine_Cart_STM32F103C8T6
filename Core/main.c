/**
 * @file main.c
 * @brief FreeRTOS medicine cart with a non-blocking state machine.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "motor.h"
#include "gray_sensor.h"
#include "hx711.h"
#include "oled.h"
#include "pid.h"
#include "delay.h"
#include "usart.h"
#include "cart_state.h"
#include "app_protocol.h"

#define CONTROL_PERIOD_MS       5U
#define LOGIC_PERIOD_MS        20U
#define SENSOR_PERIOD_MS      100U
#define BASE_SPEED            400

TaskHandle_t g_ControlTask_Handle = NULL;
TaskHandle_t g_UILogicTask_Handle = NULL;
TaskHandle_t g_SensorTask_Handle = NULL;

QueueHandle_t g_UartQueue = NULL;
QueueHandle_t g_WeightQueue = NULL;
volatile uint8_t g_intersection_flag = 0U;

static PID_HandleTypeDef g_xPID;
static uint8_t g_ucTargetWard = 4U;

typedef struct
{
    int16_t iLeftSpeed;
    int16_t iRightSpeed;
    uint8_t ucUseLineController;
} MotorCommand_t;

typedef struct
{
    float fWeightGram;
    uint8_t ucValid;
} WeightSample_t;

static MotorCommand_t g_xMotorCommand;

static void vControlTask(void *pvParameters);
static void vUILogicTask(void *pvParameters);
static void vSensorTask(void *pvParameters);

static uint8_t prvIsHome(void)
{
    return ((GPIOB->IDR & 0x007FU) == 0x3EU) ? 1U : 0U;
}

static void prvIndicatorInit(void)
{
    GPIO_InitTypeDef xGPIO;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    xGPIO.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    xGPIO.GPIO_Mode = GPIO_Mode_Out_PP;
    xGPIO.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &xGPIO);
    GPIO_ResetBits(GPIOC, GPIO_Pin_8 | GPIO_Pin_9);
}

static void prvPublishMotorCommand(const CartOutput_t *pOutput)
{
    taskENTER_CRITICAL();
    g_xMotorCommand.iLeftSpeed = pOutput->iLeftSpeed;
    g_xMotorCommand.iRightSpeed = pOutput->iRightSpeed;
    g_xMotorCommand.ucUseLineController = pOutput->ucUseLineController;
    taskEXIT_CRITICAL();
}

int main(void)
{
    BaseType_t xTasksOK = pdPASS;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init();
    Motor_Init();
    GraySensor_Init();
    HX711_Init();
    OLED_Init();
    USART_Config();
    prvIndicatorInit();

    HX711_Set_Scale(430.0f);
    if (HX711_Tare() == 0U)
    {
        OLED_ShowString(0, 0, "HX711 timeout", 8);
    }

    PID_Init(&g_xPID, 2.5f, 0.02f, 1.8f);
    g_xPID.setpoint = 0.0f;

    /* Length-one queues model latest-value mailboxes. */
    g_UartQueue = xQueueCreate(1U, sizeof(VisionFrame_t));
    g_WeightQueue = xQueueCreate(1U, sizeof(WeightSample_t));
    configASSERT(g_UartQueue != NULL);
    configASSERT(g_WeightQueue != NULL);

    xTasksOK &= xTaskCreate(vUILogicTask, "UI_Logic", 256U, NULL, 3U,
                            &g_UILogicTask_Handle);
    xTasksOK &= xTaskCreate(vControlTask, "Control", 512U, NULL, 4U,
                            &g_ControlTask_Handle);
    xTasksOK &= xTaskCreate(vSensorTask, "Sensor", 160U, NULL, 1U,
                            &g_SensorTask_Handle);
    configASSERT(xTasksOK == pdPASS);

    vTaskStartScheduler();
    for (;;) { }
}

static void vControlTask(void *pvParameters)
{
    TickType_t xLastWake = xTaskGetTickCount();
    MotorCommand_t xCommand;
    float fOutput;
    int16_t iLeft;
    int16_t iRight;

    (void)pvParameters;
    for (;;)
    {
        taskENTER_CRITICAL();
        xCommand = g_xMotorCommand;
        taskEXIT_CRITICAL();

        if (xCommand.ucUseLineController)
        {
            fOutput = PID_Calculate(&g_xPID, GraySensor_Read_Error());
            if (fOutput > 300.0f) fOutput = 300.0f;
            if (fOutput < -300.0f) fOutput = -300.0f;
            iLeft = BASE_SPEED - (int16_t)fOutput;
            iRight = BASE_SPEED + (int16_t)fOutput;
        }
        else
        {
            iLeft = xCommand.iLeftSpeed;
            iRight = xCommand.iRightSpeed;
        }

        if (iLeft > 1000) iLeft = 1000;
        if (iLeft < -1000) iLeft = -1000;
        if (iRight > 1000) iRight = 1000;
        if (iRight < -1000) iRight = -1000;

        /* This task is the sole owner of the motor peripheral. */
        Motor_Set_Speed(iLeft, iRight);
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

static void vUILogicTask(void *pvParameters)
{
    TickType_t xLastWake = xTaskGetTickCount();
    CartStateMachine_t xStateMachine;
    CartInput_t xInput;
    CartOutput_t xOutput;
    VisionFrame_t xFrame;
    WeightSample_t xWeightSample;
    float fLatestWeight = 0.0f;
    uint8_t ucWeightValid = 0U;

    (void)pvParameters;
    CartSM_Init(&xStateMachine, 0U);

    for (;;)
    {
        xInput.eVisionTurn = CART_TURN_NONE;
        if (xQueueReceive(g_UartQueue, &xFrame, 0U) == pdTRUE)
        {
            if ((xFrame.ucRightWard != 0U) &&
                (xFrame.ucRightWard == g_ucTargetWard))
            {
                xInput.eVisionTurn = CART_TURN_RIGHT;
            }
            else if ((xFrame.ucLeftWard != 0U) &&
                     (xFrame.ucLeftWard == g_ucTargetWard))
            {
                xInput.eVisionTurn = CART_TURN_LEFT;
            }
        }

        if (xQueueReceive(g_WeightQueue, &xWeightSample, 0U) == pdTRUE)
        {
            ucWeightValid = xWeightSample.ucValid;
            if (ucWeightValid)
            {
                fLatestWeight = xWeightSample.fWeightGram;
            }
        }
        xInput.ulNowMs = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        xInput.fWeightGram = fLatestWeight;
        xInput.ucAtIntersection = Is_Intersection();
        xInput.ucAtHome = prvIsHome();

        CartSM_Step(&xStateMachine, &xInput, &xOutput);
        prvPublishMotorCommand(&xOutput);

        if (xOutput.ucRequestVision)
        {
            Usart_SendByte(DEBUG_USARTx, 0xA5U);
        }
        if (xOutput.ucRedLamp) GPIO_SetBits(GPIOC, GPIO_Pin_8);
        else GPIO_ResetBits(GPIOC, GPIO_Pin_8);
        if (xOutput.ucGreenLamp) GPIO_SetBits(GPIOC, GPIO_Pin_9);
        else GPIO_ResetBits(GPIOC, GPIO_Pin_9);

        OLED_ShowString(0, 2, "Target:", 8);
        OLED_ShowNum(56, 2, g_ucTargetWard, 1, 8);
        OLED_ShowString(0, 4, "State:", 8);
        OLED_ShowNum(48, 4, (uint32_t)xOutput.eState, 2, 8);
        if (ucWeightValid)
        {
            OLED_ShowString(0, 0, "W:", 8);
            OLED_ShowNum(16, 0, (uint16_t)fLatestWeight, 4, 8);
        }
        else
        {
            OLED_ShowString(0, 0, "HX711 fault", 8);
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(LOGIC_PERIOD_MS));
    }
}

static void vSensorTask(void *pvParameters)
{
    TickType_t xLastWake = xTaskGetTickCount();
    WeightSample_t xSample;

    (void)pvParameters;
    for (;;)
    {
        xSample.ucValid = HX711_Read_Weight(&xSample.fWeightGram);
        xQueueOverwrite(g_WeightQueue, &xSample);
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void vAssertCalled(const char *pcFile, int iLine)
{
    (void)pcFile;
    (void)iLine;
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
