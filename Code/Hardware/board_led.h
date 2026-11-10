#ifndef __BOARD_LED_H
#define __BOARD_LED_H

#include "stm32f4xx.h"

typedef enum
{
    BOARD_LED_1 = 0,
    BOARD_LED_2,
    BOARD_LED_COUNT
} BoardLed_Id;

void BoardLed_Init(void);
void BoardLed_On(BoardLed_Id led);
void BoardLed_Off(BoardLed_Id led);
void BoardLed_Set(BoardLed_Id led, FunctionalState state);
void BoardLed_Toggle(BoardLed_Id led);
void BoardLed_AllOff(void);
FunctionalState BoardLed_GetState(BoardLed_Id led);

#endif
