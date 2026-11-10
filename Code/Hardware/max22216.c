#include "max22216.h"

#define MAX22216_SPI_TIMEOUT         100000UL
#define MAX22216_FAULT_CLEAR_DELAY   200U
#define MAX22216_WRITE_BIT           0x80
#define MAX22216_KCDR_UA             1017U  /* MAX22216A typical CDR current LSB */
#define MAX22216_MASTER_PWM_25KHZ    0x0060U
#define MAX22216_CTRL_MODE_CDR       0x4000U
#define MAX22216_RAMP_ENABLE         0x0700U
#define MAX22216_CTRL0_LOW_SIDE_CDR  (MAX22216_CTRL_MODE_CDR | \
                                      MAX22216_RAMP_ENABLE)
#define MAX22216_CTRL1_LOW_SIDE      0x0000U
#define MAX22216_FAULT_BLOCK_MASK    (MAX22216_STATUS_OVT | \
                                      MAX22216_STATUS_OCP | \
                                      MAX22216_STATUS_COMER | \
                                      MAX22216_STATUS_UVM)

typedef enum
{
    MAX22216_SPI_APB1 = 0,
    MAX22216_SPI_APB2
} MAX22216_SpiBus;

typedef struct
{
    SPI_TypeDef *spi;
    uint32_t spi_clk;
    MAX22216_SpiBus spi_bus;
    uint8_t gpio_af;

    GPIO_TypeDef *sck_port;
    uint32_t sck_gpio_clk;
    uint16_t sck_pin;
    uint8_t sck_source;

    GPIO_TypeDef *miso_port;
    uint32_t miso_gpio_clk;
    uint16_t miso_pin;
    uint8_t miso_source;

    GPIO_TypeDef *mosi_port;
    uint32_t mosi_gpio_clk;
    uint16_t mosi_pin;
    uint8_t mosi_source;

    GPIO_TypeDef *cs_port;
    uint32_t cs_gpio_clk;
    uint16_t cs_pin;

    GPIO_TypeDef *enable_port;
    uint32_t enable_gpio_clk;
    uint16_t enable_pin;

    GPIO_TypeDef *dir_port[MAX22216_MAX_CHANNELS];
    uint32_t dir_gpio_clk[MAX22216_MAX_CHANNELS];
    uint16_t dir_pin[MAX22216_MAX_CHANNELS];
} MAX22216_DeviceConfig;

static const MAX22216_DeviceConfig max22216_devices[MAX22216_DEV_COUNT] =
{
    {
        SPI2, RCC_APB1Periph_SPI2, MAX22216_SPI_APB1, GPIO_AF_SPI2,
        GPIOB, RCC_AHB1Periph_GPIOB, GPIO_Pin_10, GPIO_PinSource10,
        GPIOC, RCC_AHB1Periph_GPIOC, GPIO_Pin_2, GPIO_PinSource2,
        GPIOC, RCC_AHB1Periph_GPIOC, GPIO_Pin_3, GPIO_PinSource3,
        GPIOB, RCC_AHB1Periph_GPIOB, GPIO_Pin_14,
        GPIOB, RCC_AHB1Periph_GPIOB, GPIO_Pin_15,
        {GPIOE, GPIOB, GPIOB, GPIOB},
        {RCC_AHB1Periph_GPIOE, RCC_AHB1Periph_GPIOB, RCC_AHB1Periph_GPIOB, RCC_AHB1Periph_GPIOB},
        {GPIO_Pin_7, GPIO_Pin_2, GPIO_Pin_1, GPIO_Pin_0}
    },
    {
        SPI1, RCC_APB2Periph_SPI1, MAX22216_SPI_APB2, GPIO_AF_SPI1,
        GPIOB, RCC_AHB1Periph_GPIOB, GPIO_Pin_3, GPIO_PinSource3,
        GPIOB, RCC_AHB1Periph_GPIOB, GPIO_Pin_4, GPIO_PinSource4,
        GPIOB, RCC_AHB1Periph_GPIOB, GPIO_Pin_5, GPIO_PinSource5,
        GPIOB, RCC_AHB1Periph_GPIOB, GPIO_Pin_6,
        GPIOB, RCC_AHB1Periph_GPIOB, GPIO_Pin_7,
        {GPIOE, GPIOE, GPIOE, GPIOE},
        {RCC_AHB1Periph_GPIOE, RCC_AHB1Periph_GPIOE, RCC_AHB1Periph_GPIOE, RCC_AHB1Periph_GPIOE},
        {GPIO_Pin_3, GPIO_Pin_2, GPIO_Pin_1, GPIO_Pin_0}
    },
    {
        SPI3, RCC_APB1Periph_SPI3, MAX22216_SPI_APB1, GPIO_AF_SPI3,
        GPIOC, RCC_AHB1Periph_GPIOC, GPIO_Pin_10, GPIO_PinSource10,
        GPIOC, RCC_AHB1Periph_GPIOC, GPIO_Pin_11, GPIO_PinSource11,
        GPIOC, RCC_AHB1Periph_GPIOC, GPIO_Pin_12, GPIO_PinSource12,
        GPIOD, RCC_AHB1Periph_GPIOD, GPIO_Pin_0,
        GPIOD, RCC_AHB1Periph_GPIOD, GPIO_Pin_1,
        {GPIOD, GPIOD, GPIOA, GPIOA},
        {RCC_AHB1Periph_GPIOD, RCC_AHB1Periph_GPIOD, RCC_AHB1Periph_GPIOA, RCC_AHB1Periph_GPIOA},
        {GPIO_Pin_7, GPIO_Pin_6, GPIO_Pin_12, GPIO_Pin_11}
    }
};

static uint8_t max22216_last_spi_status[MAX22216_DEV_COUNT];
static FunctionalState max22216_enable_state[MAX22216_DEV_COUNT];
static uint8_t max22216_direction_state[MAX22216_DEV_COUNT][MAX22216_MAX_CHANNELS];
static uint8_t max22216_channel_enable_mask[MAX22216_DEV_COUNT];

static const uint8_t max22216_reg_dc_l2h[MAX22216_MAX_CHANNELS] = {0x09, 0x17, 0x25, 0x33};
static const uint8_t max22216_reg_dc_h[MAX22216_MAX_CHANNELS] = {0x0A, 0x18, 0x26, 0x34};
static const uint8_t max22216_reg_dc_l[MAX22216_MAX_CHANNELS] = {0x0B, 0x19, 0x27, 0x35};
static const uint8_t max22216_reg_l2h_time[MAX22216_MAX_CHANNELS] = {0x0C, 0x1A, 0x28, 0x36};
static const uint8_t max22216_reg_ctrl0[MAX22216_MAX_CHANNELS] = {0x0D, 0x1B, 0x29, 0x37};
static const uint8_t max22216_reg_ctrl1[MAX22216_MAX_CHANNELS] = {0x0E, 0x1C, 0x2A, 0x38};
static const uint8_t max22216_reg_p[MAX22216_MAX_CHANNELS] = {0x15, 0x23, 0x31, 0x3F};
static const uint8_t max22216_reg_i[MAX22216_MAX_CHANNELS] = {0x16, 0x24, 0x32, 0x40};
static const uint8_t max22216_reg_i_monitor[MAX22216_MAX_CHANNELS] = {0x45, 0x4E, 0x57, 0x60};

static uint8_t MAX22216_IsValidDevice(MAX22216_DeviceId dev)
{
    return (dev < MAX22216_DEV_COUNT) ? 1U : 0U;
}

static void MAX22216_EnableSpiClock(const MAX22216_DeviceConfig *cfg)
{
    if (cfg->spi_bus == MAX22216_SPI_APB2)
    {
        RCC_APB2PeriphClockCmd(cfg->spi_clk, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(cfg->spi_clk, ENABLE);
    }
}

static void MAX22216_CS_Low(MAX22216_DeviceId dev)
{
    const MAX22216_DeviceConfig *cfg = &max22216_devices[dev];
    GPIO_ResetBits(cfg->cs_port, cfg->cs_pin);
}

static void MAX22216_CS_High(MAX22216_DeviceId dev)
{
    const MAX22216_DeviceConfig *cfg = &max22216_devices[dev];
    GPIO_SetBits(cfg->cs_port, cfg->cs_pin);
}

static void MAX22216_Delay(volatile uint32_t count)
{
    while (count-- != 0U)
    {
    }
}

static uint8_t MAX22216_IsValidChannel(uint8_t channel)
{
    return (channel < MAX22216_MAX_CHANNELS) ? 1U : 0U;
}

static ErrorStatus MAX22216_ConfigDirectionPin(MAX22216_DeviceId dev, uint8_t channel)
{
    const MAX22216_DeviceConfig *cfg = &max22216_devices[dev];
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = cfg->dir_pin[channel];
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(cfg->dir_port[channel], &gpio);

    return SUCCESS;
}

static uint16_t MAX22216_CurrentToReg(uint16_t current_ma)
{
    return (uint16_t)(((uint32_t)current_ma * 1000U + (MAX22216_KCDR_UA / 2U)) /
                      MAX22216_KCDR_UA);
}

static uint16_t MAX22216_RegToCurrent(uint16_t reg_value)
{
    return (uint16_t)(((uint32_t)reg_value * MAX22216_KCDR_UA + 500U) / 1000U);
}

static uint8_t MAX22216_SPI_TxRxByte(MAX22216_DeviceId dev, uint8_t tx)
{
    const MAX22216_DeviceConfig *cfg = &max22216_devices[dev];
    uint32_t timeout = MAX22216_SPI_TIMEOUT;

    while (SPI_I2S_GetFlagStatus(cfg->spi, SPI_I2S_FLAG_TXE) == RESET)
    {
        if (timeout-- == 0U)
        {
            return 0;
        }
    }

    SPI_I2S_SendData(cfg->spi, tx);

    timeout = MAX22216_SPI_TIMEOUT;
    while (SPI_I2S_GetFlagStatus(cfg->spi, SPI_I2S_FLAG_RXNE) == RESET)
    {
        if (timeout-- == 0U)
        {
            return 0;
        }
    }

    return (uint8_t)SPI_I2S_ReceiveData(cfg->spi);
}

static uint16_t MAX22216_Transfer24(MAX22216_DeviceId dev, uint8_t cmd, uint16_t data, uint8_t *spi_status)
{
    const MAX22216_DeviceConfig *cfg = &max22216_devices[dev];
    uint8_t rx0;
    uint8_t rx1;
    uint8_t rx2;
    uint32_t timeout;

    MAX22216_CS_Low(dev);
    rx0 = MAX22216_SPI_TxRxByte(dev, cmd);
    rx1 = MAX22216_SPI_TxRxByte(dev, (uint8_t)(data >> 8));
    rx2 = MAX22216_SPI_TxRxByte(dev, (uint8_t)data);

    timeout = MAX22216_SPI_TIMEOUT;
    while (SPI_I2S_GetFlagStatus(cfg->spi, SPI_I2S_FLAG_BSY) == SET)
    {
        if (timeout-- == 0U)
        {
            break;
        }
    }

    MAX22216_CS_High(dev);

    if (spi_status != 0)
    {
        *spi_status = rx0;
    }
    max22216_last_spi_status[dev] = rx0;

    return (uint16_t)(((uint16_t)rx1 << 8) | rx2);
}

void MAX22216_Init(void)
{
    MAX22216_InitDevice(MAX22216_DEV_A);
    MAX22216_InitDevice(MAX22216_DEV_B);
    MAX22216_InitDevice(MAX22216_DEV_C);
}

void MAX22216_InitDevice(MAX22216_DeviceId dev)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;
    const MAX22216_DeviceConfig *cfg;
    uint32_t dir_gpio_clk;
    uint8_t channel;

    if (MAX22216_IsValidDevice(dev) == 0U)
    {
        return;
    }

    cfg = &max22216_devices[dev];
    max22216_channel_enable_mask[dev] = 0U;

    RCC_AHB1PeriphClockCmd(cfg->sck_gpio_clk |
                           cfg->miso_gpio_clk |
                           cfg->mosi_gpio_clk |
                           cfg->cs_gpio_clk |
                           cfg->enable_gpio_clk, ENABLE);
    MAX22216_EnableSpiClock(cfg);

    GPIO_SetBits(cfg->cs_port, cfg->cs_pin);
    GPIO_ResetBits(cfg->enable_port, cfg->enable_pin);

    dir_gpio_clk = 0U;
    for (channel = 0U; channel < MAX22216_MAX_CHANNELS; channel++)
    {
        dir_gpio_clk |= cfg->dir_gpio_clk[channel];
    }
    RCC_AHB1PeriphClockCmd(dir_gpio_clk, ENABLE);

    GPIO_PinAFConfig(cfg->sck_port, cfg->sck_source, cfg->gpio_af);
    GPIO_PinAFConfig(cfg->miso_port, cfg->miso_source, cfg->gpio_af);
    GPIO_PinAFConfig(cfg->mosi_port, cfg->mosi_source, cfg->gpio_af);

    gpio.GPIO_Pin = cfg->sck_pin;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(cfg->sck_port, &gpio);

    gpio.GPIO_Pin = cfg->miso_pin;
    GPIO_Init(cfg->miso_port, &gpio);

    gpio.GPIO_Pin = cfg->mosi_pin;
    GPIO_Init(cfg->mosi_port, &gpio);

    gpio.GPIO_Pin = cfg->cs_pin;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(cfg->cs_port, &gpio);
    MAX22216_CS_High(dev);

    gpio.GPIO_Pin = cfg->enable_pin;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(cfg->enable_port, &gpio);
    GPIO_SetBits(cfg->enable_port, cfg->enable_pin);
    max22216_enable_state[dev] = ENABLE;

    for (channel = 0U; channel < MAX22216_MAX_CHANNELS; channel++)
    {
        (void)MAX22216_ConfigDirectionPin(dev, channel);
        max22216_direction_state[dev][channel] = 0U;
        GPIO_ResetBits(cfg->dir_port[channel], cfg->dir_pin[channel]);
    }

    SPI_I2S_DeInit(cfg->spi);

    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7;
    SPI_Init(cfg->spi, &spi);
    SPI_Cmd(cfg->spi, ENABLE);

    MAX22216_Delay(200000U);
}

void MAX22216_SetEnable(MAX22216_DeviceId dev, FunctionalState state)
{
    const MAX22216_DeviceConfig *cfg;

    if (MAX22216_IsValidDevice(dev) == 0U)
    {
        return;
    }

    cfg = &max22216_devices[dev];
    if (state == ENABLE)
    {
        GPIO_SetBits(cfg->enable_port, cfg->enable_pin);
        max22216_enable_state[dev] = ENABLE;
    }
    else
    {
        GPIO_ResetBits(cfg->enable_port, cfg->enable_pin);
        max22216_enable_state[dev] = DISABLE;
    }
}

FunctionalState MAX22216_GetEnable(MAX22216_DeviceId dev)
{
    if (MAX22216_IsValidDevice(dev) == 0U)
    {
        return DISABLE;
    }

    return max22216_enable_state[dev];
}

ErrorStatus MAX22216_SetChannelDirection(MAX22216_DeviceId dev, uint8_t channel, uint8_t reverse)
{
    const MAX22216_DeviceConfig *cfg;

    if ((MAX22216_IsValidDevice(dev) == 0U) || (MAX22216_IsValidChannel(channel) == 0U))
    {
        return ERROR;
    }

    cfg = &max22216_devices[dev];
    if (reverse != 0U)
    {
        GPIO_SetBits(cfg->dir_port[channel], cfg->dir_pin[channel]);
        max22216_direction_state[dev][channel] = 1U;
    }
    else
    {
        GPIO_ResetBits(cfg->dir_port[channel], cfg->dir_pin[channel]);
        max22216_direction_state[dev][channel] = 0U;
    }

    return SUCCESS;
}

uint8_t MAX22216_GetChannelDirection(MAX22216_DeviceId dev, uint8_t channel)
{
    if ((MAX22216_IsValidDevice(dev) == 0U) || (MAX22216_IsValidChannel(channel) == 0U))
    {
        return 0U;
    }

    return max22216_direction_state[dev][channel];
}

uint16_t MAX22216_WriteReg(MAX22216_DeviceId dev, uint8_t addr, uint16_t data)
{
    if (MAX22216_IsValidDevice(dev) == 0U)
    {
        return 0U;
    }

    return MAX22216_Transfer24(dev, (uint8_t)(MAX22216_WRITE_BIT | (addr & 0x7FU)), data, 0);
}

uint16_t MAX22216_ReadReg(MAX22216_DeviceId dev, uint8_t addr)
{
    uint8_t cmd = (uint8_t)(addr & 0x7FU);

    if (MAX22216_IsValidDevice(dev) == 0U)
    {
        return 0U;
    }

    (void)MAX22216_Transfer24(dev, cmd, 0x0000, 0);
    return MAX22216_Transfer24(dev, cmd, 0x0000, 0);
}

uint16_t MAX22216_ReadStatusReg(MAX22216_DeviceId dev)
{
    return MAX22216_ReadReg(dev, MAX22216_REG_STATUS);
}

uint8_t MAX22216_BasicTest(MAX22216_DeviceId dev, MAX22216_TestResult *result)
{
    uint16_t readback;

    if ((MAX22216_IsValidDevice(dev) == 0U) || (result == 0))
    {
        return 0;
    }

    result->status_reg = MAX22216_ReadReg(dev, MAX22216_REG_STATUS);
    result->spi_status = max22216_last_spi_status[dev];
    result->vm_monitor_reg = MAX22216_ReadReg(dev, MAX22216_REG_VM_MONITOR);

    (void)MAX22216_WriteReg(dev, MAX22216_REG_GLOBAL_CTRL, 0x00A0);
    readback = MAX22216_ReadReg(dev, MAX22216_REG_GLOBAL_CTRL);
    result->rw_test_value = readback;
    result->rw_ok = (readback == 0x00A0U) ? 1U : 0U;

    (void)MAX22216_WriteReg(dev, MAX22216_REG_GLOBAL_CTRL, 0x0000);

    return result->rw_ok;
}

ErrorStatus MAX22216_ClearFaults(MAX22216_DeviceId dev)
{
    if (MAX22216_IsValidDevice(dev) == 0U)
    {
        return ERROR;
    }

    (void)MAX22216_WriteReg(dev, MAX22216_REG_FAULT0, 0xFFFFU);
    MAX22216_Delay(MAX22216_FAULT_CLEAR_DELAY);
    (void)MAX22216_WriteReg(dev, MAX22216_REG_FAULT1, 0xFFFFU);
    MAX22216_Delay(MAX22216_FAULT_CLEAR_DELAY);
    return SUCCESS;
}

ErrorStatus MAX22216_EnterActiveMode(MAX22216_DeviceId dev)
{
    if (MAX22216_IsValidDevice(dev) == 0U)
    {
        return ERROR;
    }

    (void)MAX22216_WriteReg(dev, MAX22216_REG_GLOBAL_CFG, MAX22216_GLOBAL_CFG_ACTIVE);
    return SUCCESS;
}

ErrorStatus MAX22216_AllOff(MAX22216_DeviceId dev)
{
    if (MAX22216_IsValidDevice(dev) == 0U)
    {
        return ERROR;
    }

    max22216_channel_enable_mask[dev] = 0U;
    (void)MAX22216_WriteReg(dev, MAX22216_REG_GLOBAL_CTRL, MAX22216_MASTER_PWM_25KHZ);
    return SUCCESS;
}

static ErrorStatus MAX22216_ConfigLowSideCDRWithCtrl0(MAX22216_DeviceId dev,
                                                       uint8_t channel,
                                                       uint16_t hold_current_ma,
                                                       uint16_t ctrl0)
{
    uint16_t target_reg;

    if ((MAX22216_IsValidDevice(dev) == 0U) ||
        (MAX22216_IsValidChannel(channel) == 0U) ||
        (hold_current_ma == 0U) ||
        (hold_current_ma > MAX22216_MAX_HOLD_CURRENT_MA))
    {
        return ERROR;
    }

    target_reg = MAX22216_CurrentToReg(hold_current_ma);

    (void)MAX22216_EnterActiveMode(dev);
    (void)MAX22216_WriteReg(dev, max22216_reg_dc_l2h[channel], target_reg);
    (void)MAX22216_WriteReg(dev, max22216_reg_dc_h[channel], target_reg);
    (void)MAX22216_WriteReg(dev, max22216_reg_dc_l[channel], 0x0000U);
    (void)MAX22216_WriteReg(dev, max22216_reg_l2h_time[channel], 0x0000U);
    (void)MAX22216_WriteReg(dev, max22216_reg_ctrl0[channel], ctrl0);
    (void)MAX22216_WriteReg(dev, max22216_reg_ctrl1[channel], MAX22216_CTRL1_LOW_SIDE);
    (void)MAX22216_WriteReg(dev, max22216_reg_p[channel], 0x0100U);
    (void)MAX22216_WriteReg(dev, max22216_reg_i[channel], 0x0010U);

    return SUCCESS;
}

ErrorStatus MAX22216_ConfigLowSideCDR(MAX22216_DeviceId dev, uint8_t channel, uint16_t hold_current_ma)
{
    return MAX22216_ConfigLowSideCDRWithCtrl0(dev,
                                              channel,
                                              hold_current_ma,
                                              MAX22216_CTRL0_LOW_SIDE_CDR);
}

ErrorStatus MAX22216_ConfigLowSideCDRImmediate(MAX22216_DeviceId dev,
                                                uint8_t channel,
                                                uint16_t hold_current_ma)
{
    return MAX22216_ConfigLowSideCDRWithCtrl0(dev,
                                              channel,
                                              hold_current_ma,
                                              MAX22216_CTRL_MODE_CDR);
}

ErrorStatus MAX22216_SetHoldCurrent_mA(MAX22216_DeviceId dev, uint8_t channel, uint16_t hold_current_ma)
{
    uint16_t target_reg;

    if ((MAX22216_IsValidDevice(dev) == 0U) ||
        (MAX22216_IsValidChannel(channel) == 0U) ||
        (hold_current_ma == 0U) ||
        (hold_current_ma > MAX22216_MAX_HOLD_CURRENT_MA))
    {
        return ERROR;
    }

    target_reg = MAX22216_CurrentToReg(hold_current_ma);
    (void)MAX22216_WriteReg(dev, max22216_reg_dc_l2h[channel], target_reg);
    (void)MAX22216_WriteReg(dev, max22216_reg_dc_h[channel], target_reg);

    return SUCCESS;
}

ErrorStatus MAX22216_ChannelOn(MAX22216_DeviceId dev, uint8_t channel)
{
    uint16_t status;

    if ((MAX22216_IsValidDevice(dev) == 0U) || (MAX22216_IsValidChannel(channel) == 0U))
    {
        return ERROR;
    }

    status = MAX22216_ReadStatusReg(dev);
    if ((status & MAX22216_FAULT_BLOCK_MASK) != 0U)
    {
        return ERROR;
    }

    max22216_channel_enable_mask[dev] |= (uint8_t)(1U << channel);
    (void)MAX22216_WriteReg(dev,
                            MAX22216_REG_GLOBAL_CTRL,
                            (uint16_t)(MAX22216_MASTER_PWM_25KHZ |
                                       max22216_channel_enable_mask[dev]));
    return SUCCESS;
}

ErrorStatus MAX22216_ChannelOff(MAX22216_DeviceId dev, uint8_t channel)
{
    if ((MAX22216_IsValidDevice(dev) == 0U) || (MAX22216_IsValidChannel(channel) == 0U))
    {
        return ERROR;
    }

    max22216_channel_enable_mask[dev] &= (uint8_t)~(1U << channel);
    (void)MAX22216_WriteReg(dev,
                            MAX22216_REG_GLOBAL_CTRL,
                            (uint16_t)(MAX22216_MASTER_PWM_25KHZ |
                                       max22216_channel_enable_mask[dev]));

    return SUCCESS;
}

uint16_t MAX22216_ReadChannelCurrent_mA(MAX22216_DeviceId dev, uint8_t channel)
{
    if ((MAX22216_IsValidDevice(dev) == 0U) || (MAX22216_IsValidChannel(channel) == 0U))
    {
        return 0U;
    }

    return MAX22216_RegToCurrent(MAX22216_ReadReg(dev, max22216_reg_i_monitor[channel]));
}

ErrorStatus MAX22216_GetChannelState(MAX22216_DeviceId dev, uint8_t channel, MAX22216_ChannelState *state)
{
    if ((MAX22216_IsValidDevice(dev) == 0U) ||
        (MAX22216_IsValidChannel(channel) == 0U) ||
        (state == 0))
    {
        return ERROR;
    }

    state->status = MAX22216_ReadStatusReg(dev);
    state->spi_status = max22216_last_spi_status[dev];
    state->status_cfg = MAX22216_ReadReg(dev, MAX22216_REG_STATUS_CFG);
    state->fault0 = MAX22216_ReadReg(dev, MAX22216_REG_FAULT0);
    state->fault1 = MAX22216_ReadReg(dev, MAX22216_REG_FAULT1);
    state->vm_monitor = MAX22216_ReadReg(dev, MAX22216_REG_VM_MONITOR);
    state->global_cfg = MAX22216_ReadReg(dev, MAX22216_REG_GLOBAL_CFG);
    state->global_ctrl = MAX22216_ReadReg(dev, MAX22216_REG_GLOBAL_CTRL);
    state->ctrl0 = MAX22216_ReadReg(dev, max22216_reg_ctrl0[channel]);
    state->ctrl1 = MAX22216_ReadReg(dev, max22216_reg_ctrl1[channel]);
    state->dc_l2h = MAX22216_ReadReg(dev, max22216_reg_dc_l2h[channel]);
    state->dc_h = MAX22216_ReadReg(dev, max22216_reg_dc_h[channel]);
    state->dc_l = MAX22216_ReadReg(dev, max22216_reg_dc_l[channel]);
    state->l2h_time = MAX22216_ReadReg(dev, max22216_reg_l2h_time[channel]);
    state->p = MAX22216_ReadReg(dev, max22216_reg_p[channel]);
    state->i = MAX22216_ReadReg(dev, max22216_reg_i[channel]);
    state->i_monitor_raw = MAX22216_ReadReg(dev, max22216_reg_i_monitor[channel]);
    state->current_ma = MAX22216_RegToCurrent(state->i_monitor_raw);

    return SUCCESS;
}
