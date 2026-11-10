#include "hall_sensor.h"

#define HALL_SENSOR_GPIO_PORT    GPIOD
#define HALL_SENSOR_GPIO_CLK     RCC_AHB1Periph_GPIOD
#define HALL_SENSOR_GPIO_PIN     GPIO_Pin_8

void HallSensor_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(HALL_SENSOR_GPIO_CLK, ENABLE);

    gpio_init.GPIO_Pin = HALL_SENSOR_GPIO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_IN;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(HALL_SENSOR_GPIO_PORT, &gpio_init);
}

FunctionalState HallSensor_IsTriggered(void)
{
    return (HallSensor_ReadRawLevel() == Bit_RESET) ? ENABLE : DISABLE;
}

BitAction HallSensor_ReadRawLevel(void)
{
    return GPIO_ReadInputDataBit(HALL_SENSOR_GPIO_PORT, HALL_SENSOR_GPIO_PIN);
}
