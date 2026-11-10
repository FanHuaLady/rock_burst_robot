#include "rs485.h"

#define RS485_USART                  USART1
#define RS485_USART_RCC              RCC_APB2Periph_USART1

#define RS485_GPIO_RCC               RCC_AHB1Periph_GPIOA
#define RS485_TX_GPIO_PORT           GPIOA
#define RS485_TX_GPIO_PIN            GPIO_Pin_9
#define RS485_TX_GPIO_SOURCE         GPIO_PinSource9
#define RS485_RX_GPIO_PORT           GPIOA
#define RS485_RX_GPIO_PIN            GPIO_Pin_10
#define RS485_RX_GPIO_SOURCE         GPIO_PinSource10
#define RS485_DIR_GPIO_PORT          GPIOA
#define RS485_DIR_GPIO_PIN           GPIO_Pin_8

static void RS485_GPIO_Config(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RS485_GPIO_RCC, ENABLE);

    GPIO_PinAFConfig(RS485_TX_GPIO_PORT, RS485_TX_GPIO_SOURCE, GPIO_AF_USART1);
    GPIO_PinAFConfig(RS485_RX_GPIO_PORT, RS485_RX_GPIO_SOURCE, GPIO_AF_USART1);

    gpio_init.GPIO_Pin = RS485_TX_GPIO_PIN | RS485_RX_GPIO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &gpio_init);

    gpio_init.GPIO_Pin = RS485_DIR_GPIO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(RS485_DIR_GPIO_PORT, &gpio_init);
}

void RS485_Init(uint32_t baudrate)
{
    USART_InitTypeDef usart_init;

    RS485_GPIO_Config();
    RCC_APB2PeriphClockCmd(RS485_USART_RCC, ENABLE);

    USART_DeInit(RS485_USART);

    usart_init.USART_BaudRate = baudrate;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_WordLength = USART_WordLength_8b;
    USART_Init(RS485_USART, &usart_init);

    USART_Cmd(RS485_USART, ENABLE);
    RS485_SetRxMode();
}

void RS485_SetTxMode(void)
{
    GPIO_SetBits(RS485_DIR_GPIO_PORT, RS485_DIR_GPIO_PIN);
}

void RS485_SetRxMode(void)
{
    GPIO_ResetBits(RS485_DIR_GPIO_PORT, RS485_DIR_GPIO_PIN);
}

void RS485_SendByte(uint8_t data)
{
    RS485_SetTxMode();

    USART_SendData(RS485_USART, data);
    while (USART_GetFlagStatus(RS485_USART, USART_FLAG_TXE) == RESET)
    {
    }
    while (USART_GetFlagStatus(RS485_USART, USART_FLAG_TC) == RESET)
    {
    }

    RS485_SetRxMode();
}

void RS485_SendBuffer(const uint8_t *buffer, uint16_t length)
{
    uint16_t index;

    if ((buffer == 0) || (length == 0U))
    {
        return;
    }

    RS485_SetTxMode();

    for (index = 0U; index < length; index++)
    {
        USART_SendData(RS485_USART, buffer[index]);
        while (USART_GetFlagStatus(RS485_USART, USART_FLAG_TXE) == RESET)
        {
        }
    }

    while (USART_GetFlagStatus(RS485_USART, USART_FLAG_TC) == RESET)
    {
    }

    RS485_SetRxMode();
}

void RS485_SendString(const char *string)
{
    if (string == 0)
    {
        return;
    }

    RS485_SetTxMode();

    while (*string != '\0')
    {
        USART_SendData(RS485_USART, (uint8_t)(*string));
        while (USART_GetFlagStatus(RS485_USART, USART_FLAG_TXE) == RESET)
        {
        }
        string++;
    }

    while (USART_GetFlagStatus(RS485_USART, USART_FLAG_TC) == RESET)
    {
    }

    RS485_SetRxMode();
}

uint8_t RS485_ReadByte(uint8_t *data)
{
    if (data == 0)
    {
        return 0U;
    }

    if (USART_GetFlagStatus(RS485_USART, USART_FLAG_RXNE) == SET)
    {
        *data = (uint8_t)USART_ReceiveData(RS485_USART);
        return 1U;
    }

    return 0U;
}

void RS485_Printf_RedirectEnable(void)
{
}
