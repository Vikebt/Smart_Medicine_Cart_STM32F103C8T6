#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

// I2C 引脚定义（使用 PB8-SCL, PB9-SDA）
#define OLED_SCL_PIN    GPIO_Pin_8
#define OLED_SDA_PIN    GPIO_Pin_9
#define OLED_I2C_PORT   GPIOB

// 函数声明
void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);
void OLED_Refresh(void);  // 如果使用缓冲区

#endif
