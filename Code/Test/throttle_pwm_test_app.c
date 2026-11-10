#include "app.h"

#include "board_led.h"
#include "throttle_pwm.h"

#define THROTTLE_PWM_TEST_DUTY_PERCENT 50U

static uint8_t throttle_pwm_test_initialized;

void App_Init(void)
{
    if (throttle_pwm_test_initialized != 0U)
    {
        return;
    }

    BoardLed_Init();
    BoardLed_AllOff();

    if (ThrottlePwm_InitDefault() != SUCCESS)
    {
        return;
    }

    (void)ThrottlePwm_SetDutyPercent(THROTTLE_PWM_TEST_DUTY_PERCENT);
    BoardLed_On(BOARD_LED_1);
    throttle_pwm_test_initialized = 1U;
}

void App_Poll(void)
{
}

void App_Tick1ms(void)
{
}
