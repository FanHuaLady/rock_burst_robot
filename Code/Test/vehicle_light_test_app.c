#include "app.h"

#include "stm32f4xx.h"

#include "board_led.h"
#include "vehicle_light.h"

#define VEHICLE_LIGHT_TEST_INTERVAL_MS 8000U

static uint8_t vehicle_light_test_initialized;
static volatile uint32_t vehicle_light_test_ms;
static uint32_t vehicle_light_test_next_toggle_ms;

void App_Init(void)
{
    if (vehicle_light_test_initialized != 0U)
    {
        return;
    }

    BoardLed_Init();
    VehicleLight_Init();
    BoardLed_AllOff();
    VehicleLight_AllOff();

    vehicle_light_test_ms = 0U;
    vehicle_light_test_next_toggle_ms = VEHICLE_LIGHT_TEST_INTERVAL_MS;
    vehicle_light_test_initialized = 1U;

    /* This test app owns the system tick because it does not initialize Network. */
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U)
    {
        vehicle_light_test_initialized = 0U;
    }
}

void App_Poll(void)
{
    if (vehicle_light_test_initialized == 0U)
    {
        return;
    }

    if ((int32_t)(vehicle_light_test_ms - vehicle_light_test_next_toggle_ms) >= 0)
    {
        VehicleLight_Toggle(VEHICLE_LIGHT_1);
        VehicleLight_Toggle(VEHICLE_LIGHT_2);
        BoardLed_Toggle(BOARD_LED_1);
        BoardLed_Toggle(BOARD_LED_2);
        vehicle_light_test_next_toggle_ms += VEHICLE_LIGHT_TEST_INTERVAL_MS;
    }
}

void App_Tick1ms(void)
{
    vehicle_light_test_ms++;
}
