#include "app.h"

#include "board_led.h"
#include "stm32f4xx.h"

#define LED_BLINK_TEST_INTERVAL_MS 500U

static uint8_t led_blink_test_initialized;
static volatile uint32_t led_blink_test_ms;
static uint32_t led_blink_test_next_toggle_ms;

void App_Init(void)
{
    if (led_blink_test_initialized != 0U)
    {
        return;
    }

    BoardLed_Init();
    BoardLed_AllOff();

    led_blink_test_ms = 0U;
    led_blink_test_next_toggle_ms = LED_BLINK_TEST_INTERVAL_MS;
    led_blink_test_initialized = 1U;

    if (SysTick_Config(SystemCoreClock / 1000U) != 0U)
    {
        led_blink_test_initialized = 0U;
    }
}

void App_Poll(void)
{
    if (led_blink_test_initialized == 0U)
    {
        return;
    }

    if ((int32_t)(led_blink_test_ms - led_blink_test_next_toggle_ms) >= 0)
    {
        BoardLed_Toggle(BOARD_LED_1);
        BoardLed_Toggle(BOARD_LED_2);
        led_blink_test_next_toggle_ms += LED_BLINK_TEST_INTERVAL_MS;
    }
}

void App_Tick1ms(void)
{
    led_blink_test_ms++;
}
