#ifndef __HALL_SENSOR_H
#define __HALL_SENSOR_H

#include "stm32f4xx.h"

void HallSensor_Init(void);
FunctionalState HallSensor_IsTriggered(void);
BitAction HallSensor_ReadRawLevel(void);

#endif
