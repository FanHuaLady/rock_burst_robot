#include "button.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint32_t gpio_clk;
    uint16_t pin;
} Button_Config;

static const Button_Config button_configs[BUTTON_COUNT] =
{
    { GPIOD, RCC_AHB1Periph_GPIOD, GPIO_Pin_13 },
    { GPIOD, RCC_AHB1Periph_GPIOD, GPIO_Pin_14 }
};

static uint8_t Button_IsValid(Button_Id button)
{
    return (button < BUTTON_COUNT) ? 1U : 0U;
}

void Button_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    gpio_init.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
    gpio_init.GPIO_Mode = GPIO_Mode_IN;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &gpio_init);
}

FunctionalState Button_IsPressed(Button_Id button)
{
    if (Button_IsValid(button) == 0U)
    {
        return DISABLE;
    }

    return (Button_ReadRawLevel(button) == Bit_RESET) ? ENABLE : DISABLE;
}

BitAction Button_ReadRawLevel(Button_Id button)
{
    const Button_Config *config;

    if (Button_IsValid(button) == 0U)
    {
        return Bit_RESET;
    }

    config = &button_configs[button];
    return GPIO_ReadInputDataBit(config->port, config->pin);
}
