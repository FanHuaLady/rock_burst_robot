#include "GP8403.h"

#define GP8403_DWT_CTRL          (*(volatile uint32_t *)0xE0001000UL)
#define GP8403_CYCCNT            (*(volatile uint32_t *)0xE0001004UL)
#define GP8403_LINE_TIMEOUT_US   2000U
#define GP8403_OUTPUT_MAX_MV     5000U
#define GP8403_DAC_MAX_CODE      4095U

#define GP8403_SCL               GP8403_SCL_PIN
#define GP8403_SDA               GP8403_SDA_PIN

#if GP8403_DEFAULT_ADDRESS > 7U
#error GP8403_DEFAULT_ADDRESS must match the three hardware address pins (0..7)
#endif

volatile GP8403_Debug_t GP8403_Debug;
static uint32_t gp8403_cycles_per_us;

static void GP8403_LineHigh(uint16_t pin)
{
    GPIO_SetBits(GP8403_SCL_GPIO, pin);
}

static void GP8403_LineLow(uint16_t pin)
{
    GPIO_ResetBits(GP8403_SCL_GPIO, pin);
}

static uint8_t GP8403_LineRead(uint16_t pin)
{
    return ((GP8403_SCL_GPIO->IDR & pin) != 0U) ? 1U : 0U;
}

/* DWT avoids sharing the application's SysTick delay timer. */
static void GP8403_DelayUS(uint32_t us)
{
    uint32_t start = GP8403_CYCCNT;
    uint32_t cycles = us * gp8403_cycles_per_us;

    while ((uint32_t)(GP8403_CYCCNT - start) < cycles)
    {
    }
}

static void GP8403_Delay(void)
{
    GP8403_DelayUS(GP8403_I2C_DELAY_US);
}

static uint8_t GP8403_SCLHigh(void)
{
    uint32_t start;

    GP8403_LineHigh(GP8403_SCL);
    start = GP8403_CYCCNT;
    while (GP8403_LineRead(GP8403_SCL) == 0U)
    {
        if ((uint32_t)(GP8403_CYCCNT - start) >=
            GP8403_LINE_TIMEOUT_US * gp8403_cycles_per_us)
        {
            GP8403_Debug.last_error = GP8403_DEBUG_SCL_TIMEOUT;
            return 0U;
        }
    }
    GP8403_Delay();
    return 1U;
}

static uint8_t GP8403_Stop(void)
{
    uint8_t clock_ok;

    GP8403_LineLow(GP8403_SCL);
    GP8403_Delay();
    GP8403_LineLow(GP8403_SDA);
    GP8403_Delay();
    clock_ok = GP8403_SCLHigh();
    GP8403_LineHigh(GP8403_SDA);
    GP8403_Delay();
    return (uint8_t)(clock_ok && (GP8403_LineRead(GP8403_SDA) != 0U));
}

static uint8_t GP8403_RecoverBus(void)
{
    uint8_t pulse;

    GP8403_LineHigh(GP8403_SDA);
    if (GP8403_SCLHigh() == 0U)
    {
        return 0U;
    }

    /* Complete a slave's interrupted byte, then generate STOP. */
    for (pulse = 0U; (pulse < 9U) && (GP8403_LineRead(GP8403_SDA) == 0U); pulse++)
    {
        GP8403_LineLow(GP8403_SCL);
        GP8403_Delay();
        if (GP8403_SCLHigh() == 0U)
        {
            return 0U;
        }
    }
    return GP8403_Stop();
}

static uint8_t GP8403_WriteByte(uint8_t value)
{
    uint8_t bit;
    uint8_t acknowledged;

    GP8403_Debug.last_byte = value;
    for (bit = 0U; bit < 8U; bit++)
    {
        if ((value & 0x80U) != 0U)
        {
            GP8403_LineHigh(GP8403_SDA);
        }
        else
        {
            GP8403_LineLow(GP8403_SDA);
        }
        GP8403_Delay();
        if (GP8403_SCLHigh() == 0U)
        {
            return 0U;
        }
        GP8403_LineLow(GP8403_SCL);
        GP8403_Delay();
        value <<= 1;
    }

    /* Release SDA for the slave's ninth-clock ACK. */
    GP8403_LineHigh(GP8403_SDA);
    GP8403_Delay();
    if (GP8403_SCLHigh() == 0U)
    {
        return 0U;
    }
    acknowledged = (GP8403_LineRead(GP8403_SDA) == 0U) ? 1U : 0U;
    GP8403_LineLow(GP8403_SCL);
    GP8403_Delay();
    if (acknowledged != 0U)
    {
        GP8403_Debug.ack_count++;
    }
    else
    {
        GP8403_Debug.last_error = GP8403_DEBUG_NACK;
    }
    return acknowledged;
}

static GP8403_Status_t GP8403_WriteOnce(uint8_t command,
                                         const uint8_t *data,
                                         uint8_t length)
{
    uint8_t i;

    GP8403_Debug.last_error = GP8403_DEBUG_NONE;
    GP8403_Debug.stage = GP8403_STAGE_START;
    GP8403_Debug.command = command;
    GP8403_LineHigh(GP8403_SDA);
    GP8403_Delay();
    if (GP8403_SCLHigh() == 0U)
    {
        goto transfer_error;
    }
    if (GP8403_LineRead(GP8403_SDA) == 0U)
    {
        GP8403_Debug.last_error = GP8403_DEBUG_SDA_STUCK;
        goto transfer_error;
    }
    GP8403_LineLow(GP8403_SDA);
    GP8403_Delay();
    GP8403_LineLow(GP8403_SCL);
    GP8403_Delay();

    GP8403_Debug.stage = GP8403_STAGE_ADDRESS;
    if (GP8403_WriteByte((uint8_t)((GP8403_I2C_ADDRESS |
                                    GP8403_DEFAULT_ADDRESS) << 1)) == 0U)
    {
        goto transfer_error;
    }
    GP8403_Debug.stage = GP8403_STAGE_COMMAND;
    if (GP8403_WriteByte(command) == 0U)
    {
        goto transfer_error;
    }
    GP8403_Debug.stage = GP8403_STAGE_DATA;
    for (i = 0U; i < length; i++)
    {
        GP8403_Debug.data_index = i;
        if (GP8403_WriteByte(data[i]) == 0U)
        {
            goto transfer_error;
        }
    }
    GP8403_Debug.stage = GP8403_STAGE_STOP;
    if (GP8403_Stop() == 0U)
    {
        if (GP8403_Debug.last_error == GP8403_DEBUG_NONE)
        {
            GP8403_Debug.last_error = GP8403_DEBUG_SDA_STUCK;
        }
        goto transfer_error;
    }
    GP8403_Debug.stage = GP8403_STAGE_IDLE;
    GP8403_Debug.success_count++;
    return GP8403_OK;

transfer_error:
    GP8403_Debug.error_count++;
    (void)GP8403_Stop();
    return GP8403_ERROR;
}

static GP8403_Status_t GP8403_WriteCommand(uint8_t command,
                                            const uint8_t *data,
                                            uint8_t length)
{
    uint8_t attempt;

    /* Register writes are idempotent; retry the whole transaction on NACK. */
    for (attempt = 0U; attempt < 3U; attempt++)
    {
        if (GP8403_WriteOnce(command, data, length) == GP8403_OK)
        {
            return GP8403_OK;
        }
        if (attempt < 2U)
        {
            GP8403_Debug.retry_count++;
            if (GP8403_RecoverBus() == 0U)
            {
                break;
            }
            GP8403_DelayUS(1000U);
        }
    }
    GP8403_Debug.failed_write_count++;
    return GP8403_ERROR;
}

GP8403_Status_t GP8403_Init(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t mode = 0x00U;

    SystemCoreClockUpdate();
    gp8403_cycles_per_us = SystemCoreClock / 1000000U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    GP8403_DWT_CTRL |= 1U;

    RCC_AHB1PeriphClockCmd(GP8403_SCL_GPIO_CLK | GP8403_SDA_GPIO_CLK, ENABLE);
    /* Keep hardware I2C1 disabled while GPIO implements the bus. */
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, DISABLE);

    GP8403_LineHigh(GP8403_SCL);
    GP8403_LineHigh(GP8403_SDA);
    gpio.GPIO_Pin = GP8403_SCL | GP8403_SDA;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_OType = GPIO_OType_OD;
#if GP8403_INTERNAL_PULLUPS
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
#else
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
#endif
    GPIO_Init(GP8403_SCL_GPIO, &gpio);

    GP8403_DelayUS(3000U);
    if (GP8403_RecoverBus() == 0U)
    {
        GP8403_Debug.last_error = GP8403_DEBUG_BUS_RECOVERY;
        GP8403_Debug.error_count++;
        return GP8403_ERROR;
    }

    /* Register 0x01, data 0x00 selects 0-5 V on both channels. */
    if (GP8403_WriteCommand(0x01U, &mode, 1U) != GP8403_OK)
    {
        return GP8403_ERROR;
    }
    return GP8403_SetVoltageMV(GP8403_CHANNEL_BOTH, 0U);
}

GP8403_Status_t GP8403_SetVoltageMV(GP8403_Channel_t channel, uint16_t millivolts)
{
    uint16_t code;
    uint16_t packed_code;
    uint8_t data[4];
    uint8_t command;
    uint8_t length = 2U;

    if ((millivolts > GP8403_OUTPUT_MAX_MV) ||
        ((channel != GP8403_CHANNEL_0) &&
         (channel != GP8403_CHANNEL_1) &&
         (channel != GP8403_CHANNEL_BOTH)))
    {
        return GP8403_ERROR;
    }

    code = (uint16_t)(((uint32_t)millivolts * GP8403_DAC_MAX_CODE +
                       (GP8403_OUTPUT_MAX_MV / 2U)) /
                      GP8403_OUTPUT_MAX_MV);
    /* Bits 15:4 hold the DAC code; transmit the low byte first. */
    packed_code = (uint16_t)(code << 4);
    data[0] = (uint8_t)packed_code;
    data[1] = (uint8_t)(packed_code >> 8);
    command = (uint8_t)channel;
    if (channel == GP8403_CHANNEL_BOTH)
    {
        command = (uint8_t)GP8403_CHANNEL_0;
        data[2] = data[0];
        data[3] = data[1];
        length = 4U;
    }
    return GP8403_WriteCommand(command, data, length);
}
