#ifndef __THROTTLE_PWM_H
#define __THROTTLE_PWM_H

#include "stm32f4xx.h"

#define THROTTLE_PWM_DEFAULT_FREQUENCY_HZ 1000U
#define THROTTLE_PWM_DUTY_MAX            1000U
#define THROTTLE_PWM_DUTY_PERCENT_MAX     100U

ErrorStatus ThrottlePwm_InitDefault(void);
ErrorStatus ThrottlePwm_Init(uint32_t frequency_hz);
ErrorStatus ThrottlePwm_SetFrequency(uint32_t frequency_hz);
ErrorStatus ThrottlePwm_SetDutyPermille(uint16_t duty_permille);
ErrorStatus ThrottlePwm_SetDutyPercent(uint8_t duty_percent);
void ThrottlePwm_Start(void);
void ThrottlePwm_Stop(void);
uint32_t ThrottlePwm_GetFrequencyHz(void);
uint16_t ThrottlePwm_GetDutyPermille(void);
uint8_t ThrottlePwm_GetDutyPercent(void);

#endif
