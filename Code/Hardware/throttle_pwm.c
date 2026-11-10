#include "throttle_pwm.h"

#define THROTTLE_PWM_TIMER             TIM4
#define THROTTLE_PWM_TIMER_CLK         RCC_APB1Periph_TIM4
#define THROTTLE_PWM_GPIO_PORT         GPIOD
#define THROTTLE_PWM_GPIO_CLK          RCC_AHB1Periph_GPIOD
#define THROTTLE_PWM_GPIO_PIN          GPIO_Pin_12
#define THROTTLE_PWM_GPIO_SOURCE       GPIO_PinSource12
#define THROTTLE_PWM_GPIO_AF           GPIO_AF_TIM4

static uint32_t throttle_pwm_frequency_hz;
static uint16_t throttle_pwm_duty_permille;
static uint16_t throttle_pwm_arr;
static uint8_t throttle_pwm_enabled;

static uint32_t ThrottlePwm_GetTimerClockHz(void)
{
    RCC_ClocksTypeDef clocks;
    uint32_t timer_clock;

    RCC_GetClocksFreq(&clocks);
    timer_clock = clocks.PCLK1_Frequency;
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U)
    {
        timer_clock *= 2U;
    }

    return timer_clock;
}

static uint16_t ThrottlePwm_DutyToCompare(uint16_t duty_permille)
{
    uint32_t period_counts;
    uint32_t compare;

    if (duty_permille > THROTTLE_PWM_DUTY_MAX)
    {
        duty_permille = THROTTLE_PWM_DUTY_MAX;
    }

    period_counts = (uint32_t)throttle_pwm_arr + 1U;
    compare = (period_counts * duty_permille) / THROTTLE_PWM_DUTY_MAX;
    if (compare > 0xFFFFU)
    {
        compare = 0xFFFFU;
    }

    return (uint16_t)compare;
}

static uint16_t ThrottlePwm_PercentToPermille(uint8_t duty_percent)
{
    if (duty_percent > THROTTLE_PWM_DUTY_PERCENT_MAX)
    {
        duty_percent = THROTTLE_PWM_DUTY_PERCENT_MAX;
    }

    return (uint16_t)duty_percent * 10U;
}

static ErrorStatus ThrottlePwm_ConfigureTimer(uint32_t frequency_hz)
{
    TIM_TimeBaseInitTypeDef tim_base;
    TIM_OCInitTypeDef tim_oc;
    uint32_t timer_clock;
    uint32_t prescaler;
    uint32_t period_ticks;
    uint64_t denominator;
    uint8_t was_enabled;

    if (frequency_hz == 0U)
    {
        return ERROR;
    }

    was_enabled = throttle_pwm_enabled;
    TIM_Cmd(THROTTLE_PWM_TIMER, DISABLE);

    timer_clock = ThrottlePwm_GetTimerClockHz();
    if (timer_clock == 0U)
    {
        return ERROR;
    }

    period_ticks = timer_clock / frequency_hz;
    if (period_ticks == 0U)
    {
        return ERROR;
    }

    prescaler = 0U;
    if (period_ticks > 65536U)
    {
        prescaler = ((period_ticks + 65535U) / 65536U) - 1U;
        if (prescaler > 0xFFFFU)
        {
            return ERROR;
        }

        denominator = (uint64_t)(prescaler + 1U) * frequency_hz;
        period_ticks = (uint32_t)(timer_clock / denominator);
        if ((period_ticks == 0U) || (period_ticks > 65536U))
        {
            return ERROR;
        }
    }

    throttle_pwm_arr = (uint16_t)(period_ticks - 1U);

    TIM_TimeBaseStructInit(&tim_base);
    tim_base.TIM_Prescaler = (uint16_t)prescaler;
    tim_base.TIM_CounterMode = TIM_CounterMode_Up;
    tim_base.TIM_Period = throttle_pwm_arr;
    tim_base.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(THROTTLE_PWM_TIMER, &tim_base);

    TIM_OCStructInit(&tim_oc);
    tim_oc.TIM_OCMode = TIM_OCMode_PWM1;
    tim_oc.TIM_OutputState = TIM_OutputState_Enable;
    tim_oc.TIM_Pulse = ThrottlePwm_DutyToCompare(throttle_pwm_duty_permille);
    tim_oc.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(THROTTLE_PWM_TIMER, &tim_oc);
    TIM_OC1PreloadConfig(THROTTLE_PWM_TIMER, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(THROTTLE_PWM_TIMER, ENABLE);

    throttle_pwm_frequency_hz = frequency_hz;
    throttle_pwm_enabled = was_enabled;
    if (was_enabled != 0U)
    {
        TIM_Cmd(THROTTLE_PWM_TIMER, ENABLE);
    }
    return SUCCESS;
}

static void ThrottlePwm_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_AHB1PeriphClockCmd(THROTTLE_PWM_GPIO_CLK, ENABLE);
    GPIO_ResetBits(THROTTLE_PWM_GPIO_PORT, THROTTLE_PWM_GPIO_PIN);

    GPIO_PinAFConfig(THROTTLE_PWM_GPIO_PORT, THROTTLE_PWM_GPIO_SOURCE, THROTTLE_PWM_GPIO_AF);

    gpio_init.GPIO_Pin = THROTTLE_PWM_GPIO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_AF;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(THROTTLE_PWM_GPIO_PORT, &gpio_init);
}

ErrorStatus ThrottlePwm_Init(uint32_t frequency_hz)
{
    RCC_APB1PeriphClockCmd(THROTTLE_PWM_TIMER_CLK, ENABLE);
    ThrottlePwm_GPIO_Init();

    throttle_pwm_duty_permille = 0U;
    throttle_pwm_enabled = 0U;
    if (ThrottlePwm_ConfigureTimer(frequency_hz) == ERROR)
    {
        return ERROR;
    }

    TIM_SetCompare1(THROTTLE_PWM_TIMER, 0U);
    throttle_pwm_enabled = 1U;
    TIM_Cmd(THROTTLE_PWM_TIMER, ENABLE);
    return SUCCESS;
}

ErrorStatus ThrottlePwm_InitDefault(void)
{
    return ThrottlePwm_Init(THROTTLE_PWM_DEFAULT_FREQUENCY_HZ);
}

ErrorStatus ThrottlePwm_SetFrequency(uint32_t frequency_hz)
{
    return ThrottlePwm_ConfigureTimer(frequency_hz);
}

ErrorStatus ThrottlePwm_SetDutyPermille(uint16_t duty_permille)
{
    if (duty_permille > THROTTLE_PWM_DUTY_MAX)
    {
        return ERROR;
    }

    throttle_pwm_duty_permille = duty_permille;
    TIM_SetCompare1(THROTTLE_PWM_TIMER, ThrottlePwm_DutyToCompare(duty_permille));
    return SUCCESS;
}

ErrorStatus ThrottlePwm_SetDutyPercent(uint8_t duty_percent)
{
    return ThrottlePwm_SetDutyPermille(ThrottlePwm_PercentToPermille(duty_percent));
}

void ThrottlePwm_Start(void)
{
    throttle_pwm_enabled = 1U;
    TIM_Cmd(THROTTLE_PWM_TIMER, ENABLE);
}

void ThrottlePwm_Stop(void)
{
    throttle_pwm_duty_permille = 0U;
    TIM_SetCompare1(THROTTLE_PWM_TIMER, 0U);
    throttle_pwm_enabled = 0U;
    TIM_Cmd(THROTTLE_PWM_TIMER, DISABLE);
}

uint32_t ThrottlePwm_GetFrequencyHz(void)
{
    return throttle_pwm_frequency_hz;
}

uint16_t ThrottlePwm_GetDutyPermille(void)
{
    return throttle_pwm_duty_permille;
}

uint8_t ThrottlePwm_GetDutyPercent(void)
{
    return (uint8_t)(throttle_pwm_duty_permille / 10U);
}
