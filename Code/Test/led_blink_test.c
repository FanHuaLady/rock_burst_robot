#include "led_blink_test.h"

#include "board_led.h"
#include "delay.h"

#define LED_BLINK_TEST_INTERVAL_MS 500U

void LedBlinkTest_Run(void)
{
    BoardLed_Init();
    BoardLed_AllOff();

    while (1)
    {
        BoardLed_Toggle(BOARD_LED_1);
        BoardLed_Toggle(BOARD_LED_2);
        Delay_ms(LED_BLINK_TEST_INTERVAL_MS);
    }
}
