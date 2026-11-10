#include "board_led.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} BoardLed_Config;

static const BoardLed_Config board_led_configs[BOARD_LED_COUNT] =
{
    { GPIOD, GPIO_Pin_4 },
    { GPIOD, GPIO_Pin_5 }
};

static uint8_t BoardLed_IsValid(BoardLed_Id led)
{
    return (led < BOARD_LED_COUNT) ? 1U : 0U;
}

void BoardLed_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_ResetBits(GPIOD, GPIO_Pin_4 | GPIO_Pin_5);

    gpio_init.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOD, &gpio_init);
}

void BoardLed_On(BoardLed_Id led)
{
    BoardLed_Set(led, ENABLE);
}

void BoardLed_Off(BoardLed_Id led)
{
    BoardLed_Set(led, DISABLE);
}

void BoardLed_Set(BoardLed_Id led, FunctionalState state)
{
    const BoardLed_Config *config;

    if (BoardLed_IsValid(led) == 0U)
    {
        return;
    }

    config = &board_led_configs[led];
    if (state != DISABLE)
    {
        GPIO_SetBits(config->port, config->pin);
    }
    else
    {
        GPIO_ResetBits(config->port, config->pin);
    }
}

void BoardLed_Toggle(BoardLed_Id led)
{
    const BoardLed_Config *config;

    if (BoardLed_IsValid(led) == 0U)
    {
        return;
    }

    config = &board_led_configs[led];
    if ((config->port->ODR & config->pin) != 0U)
    {
        GPIO_ResetBits(config->port, config->pin);
    }
    else
    {
        GPIO_SetBits(config->port, config->pin);
    }
}

void BoardLed_AllOff(void)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_4 | GPIO_Pin_5);
}

FunctionalState BoardLed_GetState(BoardLed_Id led)
{
    const BoardLed_Config *config;

    if (BoardLed_IsValid(led) == 0U)
    {
        return DISABLE;
    }

    config = &board_led_configs[led];
    return ((config->port->ODR & config->pin) != 0U) ? ENABLE : DISABLE;
}
