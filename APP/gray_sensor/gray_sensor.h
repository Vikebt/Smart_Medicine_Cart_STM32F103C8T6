#ifndef __GRAY_SENSOR_H
#define __GRAY_SENSOR_H

#include "stm32f10x.h"

void GraySensor_Init(void);
float GraySensor_Read_Error(void);
uint8_t Is_Intersection(void);

#endif
