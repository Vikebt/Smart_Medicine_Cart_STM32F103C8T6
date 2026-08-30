#include "delay.h"

static uint32_t g_fac_us = 0;

void delay_init(void)
{
    g_fac_us = SystemCoreClock / 8000000;
}

void delay_us(uint32_t nus)
{
    uint32_t ticks = nus * g_fac_us;
    uint32_t told = SysTick->VAL;
    uint32_t tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;
    while(1) {
        tnow = SysTick->VAL;
        if(tnow != told) {
            if(tnow < told) tcnt += told - tnow;
            else tcnt += reload - tnow + told;
            told = tnow;
            if(tcnt >= ticks) break;
        }
    }
}

void delay_ms(uint32_t nms)
{
    for(uint32_t i = 0; i < nms; i++)
        delay_us(1000);
}
