#ifndef __MAX22216_H
#define __MAX22216_H

#include "stm32f4xx.h"

#define MAX22216_REG_GLOBAL_CTRL     0x00
#define MAX22216_REG_GLOBAL_CFG      0x01
#define MAX22216_REG_STATUS          0x02
#define MAX22216_REG_STATUS_CFG      0x03
#define MAX22216_REG_VM_MONITOR      0x05
#define MAX22216_REG_FAULT0          0x65
#define MAX22216_REG_FAULT1          0x66

#define MAX22216_MAX_CHANNELS        4U
#define MAX22216_MAX_HOLD_CURRENT_MA 3000U

#define MAX22216_GLOBAL_CFG_ACTIVE   (1U << 15)

#define MAX22216_STATUS_OVT          (1U << 7)
#define MAX22216_STATUS_OCP          (1U << 6)
#define MAX22216_STATUS_OLF          (1U << 5)
#define MAX22216_STATUS_HHF          (1U << 4)
#define MAX22216_STATUS_DPM          (1U << 3)
#define MAX22216_STATUS_COMER        (1U << 2)
#define MAX22216_STATUS_UVM          (1U << 1)

typedef enum
{
    MAX22216_DEV_A = 0,
    MAX22216_DEV_B,
    MAX22216_DEV_C,
    MAX22216_DEV_COUNT
} MAX22216_DeviceId;

typedef struct
{
    uint8_t spi_status;
    uint16_t status_reg;
    uint16_t vm_monitor_reg;
    uint16_t rw_test_value;
    uint8_t rw_ok;
} MAX22216_TestResult;

typedef struct
{
    uint8_t spi_status;
    uint16_t status;
    uint16_t status_cfg;
    uint16_t fault0;
    uint16_t fault1;
    uint16_t vm_monitor;
    uint16_t global_cfg;
    uint16_t global_ctrl;
    uint16_t ctrl0;
    uint16_t ctrl1;
    uint16_t dc_l2h;
    uint16_t dc_h;
    uint16_t dc_l;
    uint16_t l2h_time;
    uint16_t p;
    uint16_t i;
    uint16_t i_monitor_raw;
    uint16_t current_ma;
} MAX22216_ChannelState;

void MAX22216_Init(void);
void MAX22216_InitDevice(MAX22216_DeviceId dev);
void MAX22216_SetEnable(MAX22216_DeviceId dev, FunctionalState state);
FunctionalState MAX22216_GetEnable(MAX22216_DeviceId dev);
ErrorStatus MAX22216_SetChannelDirection(MAX22216_DeviceId dev, uint8_t channel, uint8_t reverse);
uint8_t MAX22216_GetChannelDirection(MAX22216_DeviceId dev, uint8_t channel);
uint16_t MAX22216_WriteReg(MAX22216_DeviceId dev, uint8_t addr, uint16_t data);
uint16_t MAX22216_ReadReg(MAX22216_DeviceId dev, uint8_t addr);
uint16_t MAX22216_ReadStatusReg(MAX22216_DeviceId dev);
uint8_t MAX22216_BasicTest(MAX22216_DeviceId dev, MAX22216_TestResult *result);
ErrorStatus MAX22216_ClearFaults(MAX22216_DeviceId dev);
ErrorStatus MAX22216_EnterActiveMode(MAX22216_DeviceId dev);
ErrorStatus MAX22216_AllOff(MAX22216_DeviceId dev);
ErrorStatus MAX22216_ConfigLowSideCDR(MAX22216_DeviceId dev, uint8_t channel, uint16_t hold_current_ma);
ErrorStatus MAX22216_ConfigLowSideCDRImmediate(MAX22216_DeviceId dev,
                                                uint8_t channel,
                                                uint16_t hold_current_ma);
ErrorStatus MAX22216_SetHoldCurrent_mA(MAX22216_DeviceId dev, uint8_t channel, uint16_t hold_current_ma);
ErrorStatus MAX22216_ChannelOn(MAX22216_DeviceId dev, uint8_t channel);
ErrorStatus MAX22216_ChannelOff(MAX22216_DeviceId dev, uint8_t channel);
uint16_t MAX22216_ReadChannelCurrent_mA(MAX22216_DeviceId dev, uint8_t channel);
ErrorStatus MAX22216_GetChannelState(MAX22216_DeviceId dev, uint8_t channel, MAX22216_ChannelState *state);

#endif
