#include "drive_mixer.h"

static int16_t DriveMixer_ClampPercent(int32_t value);
static int16_t DriveMixer_ScalePercent(int16_t value, int16_t percent);

ErrorStatus DriveMixer_Mix(int16_t turn, int16_t forward, DriveMixer_Output *output)
{
    int16_t left;
    int16_t right;

    if (output == 0)
    {
        return ERROR;
    }

    turn = DriveMixer_ClampPercent(turn);
    forward = DriveMixer_ClampPercent(forward);

    if (forward != 0)
    {
        left = forward;
        right = forward;
        if (turn > 0)
        {
            right = DriveMixer_ScalePercent(forward, (int16_t)(100 - turn));
        }
        else if (turn < 0)
        {
            left = DriveMixer_ScalePercent(forward, (int16_t)(100 + turn));
        }
    }
    else
    {
        left = turn;
        right = (int16_t)-turn;
    }

    output->left_track = left;
    output->right_track = right;

    return SUCCESS;
}

static int16_t DriveMixer_ScalePercent(int16_t value, int16_t percent)
{
    int16_t scaled;

    scaled = (int16_t)(((int32_t)value * (int32_t)percent) / 100);
    if ((value != 0) && (scaled == 0))
    {
        scaled = (value > 0) ? 1 : -1;
    }
    return scaled;
}

static int16_t DriveMixer_ClampPercent(int32_t value)
{
    if (value > 100)
    {
        value = 100;
    }
    else if (value < -100)
    {
        value = -100;
    }

    return (int16_t)value;
}
