#include "delay.h"
#include "stm32f10x_rcc.h"

/* TIM2 is the independent microsecond clock; FreeRTOS owns SysTick. */
void delay_init(void)
{
    RCC_ClocksTypeDef clocks;
    uint32_t timer_hz;

    RCC_GetClocksFreq(&clocks);
    timer_hz = clocks.PCLK1_Frequency;
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency)
    {
        timer_hz *= 2U;
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM2->CR1 = 0U;
    TIM2->PSC = (timer_hz / 1000000U) - 1U;
    TIM2->ARR = 0xFFFFU;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CNT = 0U;
    TIM2->CR1 = TIM_CR1_CEN;
}

void delay_us(uint32_t microseconds)
{
    while (microseconds != 0U)
    {
        uint16_t start = (uint16_t)TIM2->CNT;
        uint16_t interval = (microseconds > 60000U) ? 60000U : (uint16_t)microseconds;

        while ((uint16_t)((uint16_t)TIM2->CNT - start) < interval)
        {
        }
        microseconds -= interval;
    }
}

void delay_ms(uint32_t milliseconds)
{
    while (milliseconds-- != 0U)
    {
        delay_us(1000U);
    }
}
