#ifndef __BUTTON_H
#define __BUTTON_H

#include "stm32f4xx.h"

typedef enum
{
    BUTTON_1 = 0,
    BUTTON_2,
    BUTTON_COUNT
} Button_Id;

void Button_Init(void);
FunctionalState Button_IsPressed(Button_Id button);
BitAction Button_ReadRawLevel(Button_Id button);

#endif
