#ifndef __RELAY_H
#define __RELAY_H

#include "stm32f4xx.h"

/* PA3 drives the relay transistor; high energizes the relay coil. */
#define RELAY_GPIO       GPIOA
#define RELAY_GPIO_CLK   RCC_AHB1Periph_GPIOA
#define RELAY_PIN        GPIO_Pin_3

void Relay_Init(void);
void Relay_On(void);
void Relay_Off(void);
void Relay_Toggle(void);
uint8_t Relay_IsOn(void);

#endif
