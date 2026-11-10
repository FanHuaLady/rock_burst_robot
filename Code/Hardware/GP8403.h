#ifndef __GP8403_H
#define __GP8403_H

#include "stm32f4xx.h"

/* A2:A1:A0 hardware straps: 0..7. Default 000 gives address 0x58. */
#define GP8403_I2C_ADDRESS       0x58U
#define GP8403_DEFAULT_ADDRESS   0x00U

/* Software I2C timing. Three delay slots per clock are used by each byte. */
#define GP8403_I2C_DELAY_US      100U
/* Use the STM32 GPIO pull-ups in addition to the external bus pull-ups. */
#define GP8403_INTERNAL_PULLUPS  1U

/* GP8403 software-I2C bus connection. */
#define GP8403_SCL_GPIO          GPIOB
#define GP8403_SCL_GPIO_CLK      RCC_AHB1Periph_GPIOB
#define GP8403_SCL_PIN           GPIO_Pin_8
#define GP8403_SDA_GPIO          GPIOB
#define GP8403_SDA_GPIO_CLK      RCC_AHB1Periph_GPIOB
#define GP8403_SDA_PIN           GPIO_Pin_9

typedef enum
{
    GP8403_CHANNEL_0 = 0x02U,
    GP8403_CHANNEL_1 = 0x04U,
    GP8403_CHANNEL_BOTH = 0xFFU
} GP8403_Channel_t;

typedef enum
{
    GP8403_OK = 0,
    GP8403_ERROR = 1
} GP8403_Status_t;

typedef enum
{
    GP8403_STAGE_IDLE,
    GP8403_STAGE_START,
    GP8403_STAGE_ADDRESS,
    GP8403_STAGE_COMMAND,
    GP8403_STAGE_DATA,
    GP8403_STAGE_STOP
} GP8403_Stage_t;

typedef enum
{
    GP8403_DEBUG_NONE,
    GP8403_DEBUG_NACK,
    GP8403_DEBUG_SCL_TIMEOUT,
    GP8403_DEBUG_SDA_STUCK,
    GP8403_DEBUG_BUS_RECOVERY
} GP8403_DebugError_t;

/* Live transfer diagnostics; counters include all transfers since reset. */
typedef struct
{
    uint32_t success_count;
    uint32_t error_count;
    uint32_t ack_count;
    uint32_t retry_count;
    uint32_t failed_write_count;
    GP8403_Stage_t stage;
    GP8403_DebugError_t last_error;
    uint8_t command;
    uint8_t last_byte;
    uint8_t data_index;
} GP8403_Debug_t;

extern volatile GP8403_Debug_t GP8403_Debug;

GP8403_Status_t GP8403_Init(void);
/* 0..5000 mV, nominal full scale. BOTH writes both output registers. */
GP8403_Status_t GP8403_SetVoltageMV(GP8403_Channel_t channel, uint16_t millivolts);

#endif
