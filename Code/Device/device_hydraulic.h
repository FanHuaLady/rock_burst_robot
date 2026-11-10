#ifndef __DEVICE_HYDRAULIC_H
#define __DEVICE_HYDRAULIC_H

#include "stm32f4xx.h"
#include "max22216.h"

typedef struct
{
    MAX22216_DeviceId device;
    uint8_t channel;
} DeviceHydraulic_PhysicalChannel;

void DeviceHydraulic_Init(void);
void DeviceHydraulic_Tick1ms(void);
void DeviceHydraulic_ResetAll(void);
ErrorStatus DeviceHydraulic_SetDirection(DeviceHydraulic_PhysicalChannel physical,
                                         FunctionalState reverse);
ErrorStatus DeviceHydraulic_ConfigCurrent(DeviceHydraulic_PhysicalChannel physical,
                                          uint16_t current_ma);
ErrorStatus DeviceHydraulic_ChannelOn(DeviceHydraulic_PhysicalChannel physical);
ErrorStatus DeviceHydraulic_ChannelOff(DeviceHydraulic_PhysicalChannel physical);
ErrorStatus DeviceHydraulic_GetState(DeviceHydraulic_PhysicalChannel physical,
                                     MAX22216_ChannelState *state);

#endif
