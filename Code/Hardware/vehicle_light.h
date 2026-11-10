#ifndef __VEHICLE_LIGHT_H
#define __VEHICLE_LIGHT_H

#include "stm32f4xx.h"

typedef enum
{
    VEHICLE_LIGHT_1 = 0,
    VEHICLE_LIGHT_2,
    VEHICLE_LIGHT_COUNT
} VehicleLight_Id;

void VehicleLight_Init(void);
void VehicleLight_On(VehicleLight_Id light);
void VehicleLight_Off(VehicleLight_Id light);
void VehicleLight_Set(VehicleLight_Id light, FunctionalState state);
void VehicleLight_Toggle(VehicleLight_Id light);
void VehicleLight_AllOff(void);
FunctionalState VehicleLight_GetState(VehicleLight_Id light);

#endif
