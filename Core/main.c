/**
  *********************************************************************
  * @file    main.c
  * @author  Vikebt
  * @version V1.0
  * @date    2026-04-20
  * @brief   FreeRTOS v9.0.0 + STM32 智能送药小车
  *********************************************************************
  */ 
 
/*
*************************************************************************
*                             包含的头文件
*************************************************************************
*/ 
/* FreeRTOS头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
/* 小车硬件连接传感器头文件 */
#include "motor.h"
#include "gray_sensor.h"
#include "hx711.h"
#include "oled.h"
#include "pid.h"
#include "delay.h"
#include "usart.h"

/**************************** 任务句柄 ********************************/
/* 
 * 任务句柄是一个指针，用于指向一个任务，当任务创建好之后，它就具有了一个任务句柄
 * 以后我们要想操作这个任务都需要通过这个任务句柄，如果是自身的任务操作自己，那么
 * 这个句柄可以为NULL。
 */
/* 创建任务句柄 */
TaskHandle_t g_ControlTask_Handle = NULL;
TaskHandle_t g_UILogicTask_Handle = NULL;
TaskHandle_t g_SensorTask_Handle = NULL;

/********************************** 内核对象句柄 *********************************/
/*
 * 信号量，消息队列，事件标志组，软件定时器这些都属于内核的对象，要想使用这些内核
 * 对象，必须先创建，创建成功之后会返回一个相应的句柄。实际上就是一个指针，后续我
 * 们就可以通过这个句柄操作这些内核对象。
 *
 * 内核对象说白了就是一种全局的数据结构，通过这些数据结构我们可以实现任务间的通信，
 * 任务间的事件同步等各种功能。至于这些功能的实现我们是通过调用这些内核对象的函数
 * 来完成的
 * 
 */
/* 创建队列句柄 */
QueueHandle_t g_UartQueue = NULL;
QueueHandle_t g_WeightQueue = NULL;

/********************************* PID 对象 *************************************/
PID_HandleTypeDef g_pid;

/******************************* 状态机定义 ************************************/
typedef enum {
    STATE_WAIT_LOAD = 0,
    STATE_LINE_FOLLOW,
    STATE_AT_INTERSECTION,
    STATE_TURN_RIGHT,
		STATE_TURN_LEFT,
    STATE_DELIVER,
    STATE_RETURN_HOME,
		STATE_RETURN_AT_INTERSECTION,   // 返程到达路口
    STATE_RETURN_TURN_LEFT,         // 返程左转
    STATE_RETURN_TURN_RIGHT         // 返程右转
} SystemState_t;

SystemState_t g_sysState = STATE_WAIT_LOAD;

typedef enum {
    TURN_NONE = 0,
    TURN_LEFT,
    TURN_RIGHT
} TurnDirection_t;

volatile TurnDirection_t g_turn_direction = TURN_NONE;

typedef enum {
    DIRECTION_TO_WARD = 0,
    DIRECTION_RETURN_HOME
} JourneyDirection_t;

JourneyDirection_t g_journey_dir = DIRECTION_TO_WARD;

/******************************* 外部中断标志位 ************************************/
volatile uint8_t g_intersection_flag = 0;

/******************************* 全局变量声明 ************************************
 * 当我们在写应用程序的时候，可能需要用到一些全局变量。
 ********************************************************************************/
uint8_t g_targetWard = 4;                       // 目标病房号（可写死或由OpenMV设定）
uint8_t g_currentWard = 0;
uint16_t g_baseSpeed = 400;                     // 基础速度 (0~1000)

volatile TurnDirection_t g_enter_turn_direction = TURN_NONE;  // 记录去程转弯方向
int8_t g_intersection_count = 0;                // 当前路口序号（去程递增）
int8_t g_target_intersection_index = -1;        // 目标病房所在路口序号

/*********************************函数声明*************************************/
void vControlTask(void *pvParameters);
void vUILogicTask(void *pvParameters);
void vSensorTask(void *pvParameters);
uint8_t Is_Home(void);

/*****************************************************************
  * @brief  主函数
  * @param  无
  * @retval 无
  * @note   第一步：开发板硬件初始化 
            第二步：创建APP应用任务
            第三步：启动FreeRTOS，开始多任务调度
  ****************************************************************/
int main(void)
{
    /* 1. NVIC 优先级分组（必须放在所有中断配置之前） */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	
		/* 2. 基础外设初始化（时钟已在 SystemInit 中完成） */
	  delay_init();       // 初始化微秒延时
    Motor_Init();
    GraySensor_Init();
    HX711_Init();
    OLED_Init();
	  USART_Config();
	
		/* 3. HX711 标定 */
    HX711_Set_Scale(430.0f);  // 示例值
    HX711_Tare();             // 去皮
	
		/* 4. PID 参数初始化 */
    PID_Init(&g_pid, 2.5f, 0.02f, 1.8f);
    g_pid.setpoint = 0.0f;
    
    /* 5. 创建队列 */
    g_UartQueue = xQueueCreate(10, sizeof(uint8_t));
    g_WeightQueue = xQueueCreate(5, sizeof(float));
    
    /* 6. 创建任务 */
    xTaskCreate(vUILogicTask, "UI_Logic", 256, NULL, 3, &g_UILogicTask_Handle);
    xTaskCreate(vControlTask, "Control", 512, NULL, 4, &g_ControlTask_Handle);
    xTaskCreate(vSensorTask, "Sensor", 128, NULL, 1, &g_SensorTask_Handle);
    
    /* 7. 启动调度器（内部自动配置 SysTick） */
    vTaskStartScheduler();
    
    /* 正常情况下不会运行到这里 */
    while(1);
}

/*-----------------------------------------------------------*/
/* PID Control 任务 - 最高优先级 (4) */
void vControlTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(5);	// 5ms 周期 = 200Hz
    float error, output;
    int16_t leftSpeed, rightSpeed;
    
    while(1) {
				// 1. 检查系统状态：只有在循迹状态下才执行控制
        if(g_sysState == STATE_LINE_FOLLOW || g_sysState == STATE_RETURN_HOME) 
					{
						// 2. 获取灰度偏差
            error = GraySensor_Read_Error();
						
						// 3. PID计算，得到转向补偿量
            output = PID_Calculate(&g_pid, error);
            
						// 4. 输出限幅（防止过大的转向导致翻车）
            if(output > 300.0f) output = 300.0f;
            if(output < -300.0f) output = -300.0f;
						
            // 5. 差速计算：左轮减速、右轮加速（或反之）
            leftSpeed = g_baseSpeed - (int16_t)output;
            rightSpeed = g_baseSpeed + (int16_t)output;
            
						// 6. 速度限幅（0~1000对应PWM占空比）
            if(leftSpeed > 1000) leftSpeed = 1000;
            if(leftSpeed < -1000) leftSpeed = -1000;
            if(rightSpeed > 1000) rightSpeed = 1000;
            if(rightSpeed < -1000) rightSpeed = -1000;
						
            // 7. 驱动电机
            Motor_Set_Speed(leftSpeed, rightSpeed);
        } 
				else 
				{
						// 非循迹状态，主动让出CPU给其他任务
            taskYIELD();
        }
				// 8. 精确延时，保证5ms的稳定周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


/*-----------------------------------------------------------*/
/* UI & Logic 任务 - 中等优先级 (3) */
void vUILogicTask(void *pvParameters)
{
    uint8_t rxData;
    float currentWeight = 0.0f;
	
		// 假设目标病房号已通过某种方式获得，例如预先写死为4
    g_targetWard = 4;
    
    for(;;) 
    {
        // --- 第一部分：数据接收 ---
        // 非阻塞地从队列中取出OpenMV发送的病房号
        while(xQueueReceive(g_UartQueue, &rxData, 0) == pdTRUE) 
				{
            if(rxData >= 1 && rxData <= 8) 
						{
                g_targetWard = rxData;
                OLED_ShowString(0, 2, "Target:", 8);
                OLED_ShowNum(56, 2, g_targetWard, 1, 8);
            }
        }
        
        // 从重量队列获取当前重量（非阻塞）
        if(xQueueReceive(g_WeightQueue, &currentWeight, 0) == pdTRUE) 
				{
            // 重量值已在Sensor任务中显示，此处仅用于状态判断
        }
        
        // --- 第二部分：主状态机 ---
        switch(g_sysState) 
				{
            case STATE_WAIT_LOAD:
								// 等待药品装载（重量≥200g）
                if(currentWeight >= 200.0f) 
								{		
										// 重置路口计数
										g_intersection_count = 0;		
										g_target_intersection_index = -1;
										g_journey_dir = DIRECTION_TO_WARD;
										// 切换到循迹状态
                    g_sysState = STATE_LINE_FOLLOW;
                    OLED_ShowString(0, 4, "Going...", 8);
									  // Control任务检测到状态变化后将开始驱动电机
                }
                break;
                
            case STATE_LINE_FOLLOW:
								// 每20ms检查一次是否到达路口
								if(g_journey_dir == DIRECTION_TO_WARD && Is_Intersection())   // 判断七路是否全黑
								{
										g_intersection_count++;   // 去程每过一个路口加1
										g_sysState = STATE_AT_INTERSECTION;
										Motor_Set_Speed(0, 0);   // 立即停车
										OLED_ShowString(0, 4, "Intersect", 8);
										
										// 向OpenMV发送命令，请求识别左右两侧的数字
										Usart_SendByte(DEBUG_USARTx, 0xA5);
										// 等待OpenMV返回结果（假设OpenMV在100ms内完成识别并返回）
										vTaskDelay(pdMS_TO_TICKS(100));
								}
								break;
                
            case STATE_AT_INTERSECTION:
                // 在进入此状态前，已发送0xA5命令并延时等待
                /* 假设 OpenMV 返回的病房号已存入 g_currentWard */
                if(g_turn_direction == TURN_RIGHT)
								{
										g_target_intersection_index = g_intersection_count;  // 记录
										g_enter_turn_direction = TURN_RIGHT;
										g_sysState = STATE_TURN_RIGHT;
								}
								else if(g_turn_direction == TURN_LEFT)
								{
										g_target_intersection_index = g_intersection_count;
										g_enter_turn_direction = TURN_LEFT;
										g_sysState = STATE_TURN_LEFT;
								}
								else   // 直行通过
								{
										g_sysState = STATE_LINE_FOLLOW;
								}
                break;
								
						case STATE_TURN_RIGHT:
								// 右转：左轮正转，右轮反转（或差速右转）
								Motor_Set_Speed(300, -300);
								vTaskDelay(pdMS_TO_TICKS(500));   // 根据实际转角调整时间
								Motor_Set_Speed(0, 0);
								g_sysState = STATE_DELIVER;
								break;

						case STATE_TURN_LEFT:
								// 左转：左轮反转，右轮正转
								Motor_Set_Speed(-300, 300);
								vTaskDelay(pdMS_TO_TICKS(500));
								Motor_Set_Speed(0, 0);
								g_sysState = STATE_DELIVER;
								break;
                
            case STATE_DELIVER:
                GPIO_SetBits(GPIOC, GPIO_Pin_8);   // 红灯亮（表示到达）
								// 等待药品被取走（重量下降到50g以下）
                while(currentWeight > 50.0f) 
								{
                    xQueueReceive(g_WeightQueue, &currentWeight, portMAX_DELAY);
                }
                GPIO_ResetBits(GPIOC, GPIO_Pin_8);	// 红灯灭
                GPIO_SetBits(GPIOC, GPIO_Pin_9);    // 绿灯亮（表示完成）
                vTaskDelay(pdMS_TO_TICKS(1000));
                GPIO_ResetBits(GPIOC, GPIO_Pin_9);
                // 掉头（原地旋转约180度）
                Motor_Set_Speed(300, -300);
                vTaskDelay(pdMS_TO_TICKS(1000));
                Motor_Set_Speed(0, 0);
								
								// 设置返程标志
								g_journey_dir = DIRECTION_RETURN_HOME;
								// 注意：g_intersection_count 保持当前值（即目标路口序号）
								// 返程时从当前值开始递减
                g_sysState = STATE_RETURN_HOME;
                break;
                
            case STATE_RETURN_HOME:
								// 返程循迹中，检测路口
								if(Is_Intersection())
								{
										// 先判断是否为目标路口，再决定是否递减计数
										if(g_intersection_count == g_target_intersection_index)
										{
												// 到达来时的路口，需要反向转弯
												g_sysState = STATE_RETURN_AT_INTERSECTION;
												Motor_Set_Speed(0, 0);
										}
										else
                    {
                        // 非目标路口，直接通过，但计数递减
                        g_intersection_count--;
                    }
								}
								else if(Is_Home())
								{
										// 回到药房
										g_sysState = STATE_WAIT_LOAD;
										Motor_Set_Speed(0, 0);
										OLED_ShowString(0, 4, "Home", 8);
								}
								break;

						case STATE_RETURN_AT_INTERSECTION:
								// 执行反向转弯
								if(g_enter_turn_direction == TURN_RIGHT)
								{
										g_sysState = STATE_RETURN_TURN_LEFT;   // 去程右转，返程左转
								}
								else if(g_enter_turn_direction == TURN_LEFT)
								{
										g_sysState = STATE_RETURN_TURN_RIGHT;  // 去程左转，返程右转
								}
								else
								{
										// 异常情况，直接继续返程
										g_sysState = STATE_RETURN_HOME;
								}
								break;

						case STATE_RETURN_TURN_LEFT:
								Motor_Set_Speed(-300, 300);   // 左转
								vTaskDelay(pdMS_TO_TICKS(500));
								Motor_Set_Speed(0, 0);
								// 转弯完成后，计数减1（因为已经通过了这个路口）
                g_intersection_count--;
								g_sysState = STATE_RETURN_HOME;
								break;

						case STATE_RETURN_TURN_RIGHT:
								Motor_Set_Speed(300, -300);   // 右转
								vTaskDelay(pdMS_TO_TICKS(500));
								Motor_Set_Speed(0, 0);
								// 转弯完成后，计数减1（因为已经通过了这个路口）
                g_intersection_count--;
								g_sysState = STATE_RETURN_HOME;
								break;
        }
				// 状态机每20ms运行一次
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
/*-----------------------------------------------------------*/
/* Sensor & Display 任务 - 最低优先级 (1) */
void vSensorTask(void *pvParameters)
{
    float weight;
    for(;;) {
				// 读取重量（内含多次平均）
        weight = HX711_Read_Weight();
			
        OLED_ShowString(0, 0, "W:", 8);
        OLED_ShowNum(16, 0, (uint16_t)weight, 4, 8);
			
				// 发送给UI任务
        xQueueSend(g_WeightQueue, &weight, 0);
			
				// 每100ms执行一次
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* 判断是否回到药房（示例：检测特定灰度组合） */
uint8_t Is_Home(void)
{
    // 实际可根据传感器数据判断
    return ((GPIOB->IDR & 0x007F) == 0x3E);  // 假设中间三路为高
}

/********************************END OF FILE****************************/
