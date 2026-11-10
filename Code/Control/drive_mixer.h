#ifndef __DRIVE_MIXER_H
#define __DRIVE_MIXER_H

#include "stm32f4xx.h"

typedef struct
{
    int16_t left_track;
    int16_t right_track;
} DriveMixer_Output;

ErrorStatus DriveMixer_Mix(int16_t turn, int16_t forward, DriveMixer_Output *output);

#endif
