#ifndef __HX711_H
#define __HX711_H

#include "stm32f10x.h"

// 引脚定义（可根据实际修改）
#define HX711_SCK_PIN    GPIO_Pin_2
#define HX711_SCK_PORT   GPIOC
#define HX711_DOUT_PIN   GPIO_Pin_3
#define HX711_DOUT_PORT  GPIOC

// 宏定义：操作引脚
#define HX711_SCK_HIGH()  GPIO_SetBits(HX711_SCK_PORT, HX711_SCK_PIN)
#define HX711_SCK_LOW()   GPIO_ResetBits(HX711_SCK_PORT, HX711_SCK_PIN)
#define HX711_DOUT_READ() GPIO_ReadInputDataBit(HX711_DOUT_PORT, HX711_DOUT_PIN)

// 函数声明
void HX711_Init(void);
uint32_t HX711_Read(void);
float HX711_Read_Weight(void);
void HX711_Set_Offset(long offset);
void HX711_Set_Scale(float scale);
void HX711_Tare(void);

#endif
