#include "gray_sensor.h"

void GraySensor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
                                  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

float GraySensor_Read_Error(void)
{
		// 读取PB0~PB6状态
    uint8_t val = (uint8_t)(GPIOB->IDR & 0x007F);
    int32_t sum = 0;
    int8_t cnt = 0;
		// 位置权重
    int8_t weights[7] = {-30, -20, -10, 0, 10, 20, 30};
    
    for(int i = 0; i < 7; i++) 
		{
        if(val & (1 << i)) {
            sum += weights[i];
            cnt++;
        }
    }
		// 全白，保持上次偏差
    if(cnt == 0) return 0.0f;
		// 返回加权平均偏差
    return (float)sum / cnt;
}

uint8_t Is_Intersection(void)
{
    uint8_t val = (uint8_t)(GPIOB->IDR & 0x007F);
    return (val == 0x7F);   // 假设全黑为路口
}
