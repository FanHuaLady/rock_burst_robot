#include "vehicle_light.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint32_t gpio_clk;
    uint16_t pin;
} VehicleLight_Config;

static const VehicleLight_Config vehicle_light_configs[VEHICLE_LIGHT_COUNT] =
{
    { GPIOD, RCC_AHB1Periph_GPIOD, GPIO_Pin_2 },
    { GPIOD, RCC_AHB1Periph_GPIOD, GPIO_Pin_3 }
};

static uint8_t VehicleLight_IsValid(VehicleLight_Id light)
{
    return (light < VEHICLE_LIGHT_COUNT) ? 1U : 0U;
}

void VehicleLight_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_ResetBits(GPIOD, GPIO_Pin_2 | GPIO_Pin_3);

    gpio_init.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOD, &gpio_init);
}

void VehicleLight_On(VehicleLight_Id light)
{
    VehicleLight_Set(light, ENABLE);
}

void VehicleLight_Off(VehicleLight_Id light)
{
    VehicleLight_Set(light, DISABLE);
}

void VehicleLight_Set(VehicleLight_Id light, FunctionalState state)
{
    const VehicleLight_Config *config;

    if (VehicleLight_IsValid(light) == 0U)
    {
        return;
    }

    config = &vehicle_light_configs[light];
    if (state != DISABLE)
    {
        GPIO_SetBits(config->port, config->pin);
    }
    else
    {
        GPIO_ResetBits(config->port, config->pin);
    }
}

void VehicleLight_Toggle(VehicleLight_Id light)
{
    const VehicleLight_Config *config;

    if (VehicleLight_IsValid(light) == 0U)
    {
        return;
    }

    config = &vehicle_light_configs[light];
    if ((config->port->ODR & config->pin) != 0U)
    {
        GPIO_ResetBits(config->port, config->pin);
    }
    else
    {
        GPIO_SetBits(config->port, config->pin);
    }
}

void VehicleLight_AllOff(void)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_2 | GPIO_Pin_3);
}

FunctionalState VehicleLight_GetState(VehicleLight_Id light)
{
    const VehicleLight_Config *config;

    if (VehicleLight_IsValid(light) == 0U)
    {
        return DISABLE;
    }

    config = &vehicle_light_configs[light];
    return ((config->port->ODR & config->pin) != 0U) ? ENABLE : DISABLE;
}
