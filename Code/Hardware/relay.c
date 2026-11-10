#include "relay.h"

void Relay_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RELAY_GPIO_CLK, ENABLE);
    /* Preload low before enabling the output to start with the coil off. */
    GPIO_ResetBits(RELAY_GPIO, RELAY_PIN);
    gpio.GPIO_Pin = RELAY_PIN;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(RELAY_GPIO, &gpio);
}

void Relay_On(void)
{
    GPIO_SetBits(RELAY_GPIO, RELAY_PIN);
}

void Relay_Off(void)
{
    GPIO_ResetBits(RELAY_GPIO, RELAY_PIN);
}

uint8_t Relay_IsOn(void)
{
    return (uint8_t)(GPIO_ReadOutputDataBit(RELAY_GPIO, RELAY_PIN) != Bit_RESET);
}

void Relay_Toggle(void)
{
    if (Relay_IsOn() != 0U)
    {
        Relay_Off();
    }
    else
    {
        Relay_On();
    }
}
