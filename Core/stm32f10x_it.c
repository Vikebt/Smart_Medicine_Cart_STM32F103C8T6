/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTI
  
  AL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"

#include "FreeRTOS.h"					//FreeRTOS使用		  
#include "task.h" 
#include "queue.h" 

extern QueueHandle_t g_UartQueue;
extern volatile uint8_t g_intersection_flag;
//extern volatile TurnDirection_t g_turn_direction;
extern volatile enum { TURN_NONE = 0, TURN_LEFT, TURN_RIGHT } g_turn_direction;
extern uint8_t g_targetWard;
extern uint8_t g_currentWard;

#define RX_BUF_SIZE 2
static uint8_t rx_buffer[RX_BUF_SIZE];
static uint8_t rx_index = 0;


/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
//void SVC_Handler(void)
//{
//}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
//void PendSV_Handler(void)
//{
//}

///**
//  * @brief  This function handles SysTick Handler.
//  * @param  None
//  * @retval None
//  */

void USART3_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) 
		{
        uint8_t data = USART_ReceiveData(USART3);
			
				if(rx_index < RX_BUF_SIZE)
        {
            rx_buffer[rx_index++] = data;
            if(rx_index == RX_BUF_SIZE)   // 收满两个字节
            {
                uint8_t right_ward = rx_buffer[0];
                uint8_t left_ward  = rx_buffer[1];
                
                // 根据目标病房号判断是否需要转向，以及转向方向
                if(right_ward == g_targetWard && right_ward != 0)
                {
                    g_turn_direction = TURN_RIGHT;
                    g_currentWard = right_ward;
                }
                else if(left_ward == g_targetWard && left_ward != 0)
                {
                    g_turn_direction = TURN_LEFT;
                    g_currentWard = left_ward;
                }
                else
                {
                    g_turn_direction = TURN_NONE;
                }
                
                // 将数据放入队列（或直接用全局变量通知UI任务）
                xQueueSendFromISR(g_UartQueue, &right_ward, &xHigherPriorityTaskWoken);
                xQueueSendFromISR(g_UartQueue, &left_ward, &xHigherPriorityTaskWoken);
                
                rx_index = 0;   // 重置缓冲区
            }
        }
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
		// 如果发送队列导致更高优先级任务就绪，则立即切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* 如果需要使用外部中断检测路口，可在此添加 */
void EXTI9_5_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line7) != RESET) 
		{
        g_intersection_flag = 1;
        EXTI_ClearITPendingBit(EXTI_Line7);
    }
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
