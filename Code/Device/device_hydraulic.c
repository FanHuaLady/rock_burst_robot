#include "device_hydraulic.h"

#include <string.h>

#define DEVICE_HYDRAULIC_DIRECTION_SETTLE_MS 15U

static uint8_t device_hydraulic_configured[MAX22216_DEV_COUNT][MAX22216_MAX_CHANNELS];
static volatile uint32_t device_hydraulic_ms;
static uint8_t DeviceHydraulic_IsValid(DeviceHydraulic_PhysicalChannel physical);

void DeviceHydraulic_Init(void)
{
    MAX22216_DeviceId device;
    uint8_t channel;

    (void)memset(device_hydraulic_configured, 0, sizeof(device_hydraulic_configured));
    device_hydraulic_ms = 0U;
    for (device = MAX22216_DEV_A; device < MAX22216_DEV_COUNT; device++)
    {
        (void)MAX22216_AllOff(device);
        (void)MAX22216_EnterActiveMode(device);
        (void)MAX22216_ClearFaults(device);
        for (channel = 0U; channel < MAX22216_MAX_CHANNELS; channel++)
        {
            if (MAX22216_ConfigLowSideCDR(device, channel, 100U) == SUCCESS)
            {
                device_hydraulic_configured[device][channel] = 1U;
            }
        }
        (void)MAX22216_AllOff(device);
    }
}

void DeviceHydraulic_Tick1ms(void)
{
    device_hydraulic_ms++;
}

void DeviceHydraulic_ResetAll(void)
{
    MAX22216_DeviceId device;

    for (device = MAX22216_DEV_A; device < MAX22216_DEV_COUNT; device++)
    {
        (void)MAX22216_AllOff(device);
    }
}

ErrorStatus DeviceHydraulic_SetDirection(DeviceHydraulic_PhysicalChannel physical,
                                         FunctionalState reverse)
{
    uint8_t requested_reverse;
    uint32_t ready_ms;

    if (DeviceHydraulic_IsValid(physical) == 0U)
    {
        return ERROR;
    }

    requested_reverse = (reverse == ENABLE) ? 1U : 0U;
    if (MAX22216_GetChannelDirection(physical.device, physical.channel) == requested_reverse)
    {
        return SUCCESS;
    }

    if (MAX22216_ChannelOff(physical.device, physical.channel) == ERROR)
    {
        return ERROR;
    }
    if (MAX22216_SetChannelDirection(physical.device,
                                     physical.channel,
                                     requested_reverse) == ERROR)
    {
        return ERROR;
    }

    ready_ms = device_hydraulic_ms + DEVICE_HYDRAULIC_DIRECTION_SETTLE_MS;
    while ((int32_t)(device_hydraulic_ms - ready_ms) < 0)
    {
    }
    return SUCCESS;
}

ErrorStatus DeviceHydraulic_ConfigCurrent(DeviceHydraulic_PhysicalChannel physical,
                                          uint16_t current_ma)
{
    if (DeviceHydraulic_IsValid(physical) == 0U)
    {
        return ERROR;
    }

    if (device_hydraulic_configured[physical.device][physical.channel] == 0U)
    {
        if (MAX22216_ChannelOff(physical.device, physical.channel) == ERROR)
        {
            return ERROR;
        }
        if (MAX22216_ConfigLowSideCDR(physical.device,
                                      physical.channel,
                                      current_ma) == ERROR)
        {
            return ERROR;
        }
        device_hydraulic_configured[physical.device][physical.channel] = 1U;
        return SUCCESS;
    }

    return MAX22216_SetHoldCurrent_mA(physical.device,
                                      physical.channel,
                                      current_ma);
}

ErrorStatus DeviceHydraulic_ChannelOn(DeviceHydraulic_PhysicalChannel physical)
{
    if (DeviceHydraulic_IsValid(physical) == 0U)
    {
        return ERROR;
    }

    if (device_hydraulic_configured[physical.device][physical.channel] == 0U)
    {
        return ERROR;
    }

    return MAX22216_ChannelOn(physical.device, physical.channel);
}

ErrorStatus DeviceHydraulic_ChannelOff(DeviceHydraulic_PhysicalChannel physical)
{
    if (DeviceHydraulic_IsValid(physical) == 0U)
    {
        return ERROR;
    }

    return MAX22216_ChannelOff(physical.device, physical.channel);
}

ErrorStatus DeviceHydraulic_GetState(DeviceHydraulic_PhysicalChannel physical,
                                     MAX22216_ChannelState *state)
{
    if ((state == 0) || (DeviceHydraulic_IsValid(physical) == 0U))
    {
        return ERROR;
    }

    return MAX22216_GetChannelState(physical.device, physical.channel, state);
}

static uint8_t DeviceHydraulic_IsValid(DeviceHydraulic_PhysicalChannel physical)
{
    return ((physical.device < MAX22216_DEV_COUNT) &&
            (physical.channel < MAX22216_MAX_CHANNELS)) ? 1U : 0U;
}
