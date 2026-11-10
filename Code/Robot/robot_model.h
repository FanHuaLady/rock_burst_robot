#ifndef __ROBOT_MODEL_H
#define __ROBOT_MODEL_H

#include "stm32f4xx.h"

typedef enum
{
    ROBOT_AXIS_LEFT_TRACK = 0,
    ROBOT_AXIS_RIGHT_TRACK,
    ROBOT_AXIS_BODY_ROTATE,
    ROBOT_AXIS_LEFT_LEG,
    ROBOT_AXIS_RIGHT_LEG,
    ROBOT_AXIS_ARM_1,
    ROBOT_AXIS_ARM_2,
    ROBOT_AXIS_ARM_3,
    ROBOT_AXIS_ARM_4,
    ROBOT_AXIS_DRILL_ROTATION,
    ROBOT_AXIS_COUNT
} Robot_AxisId;

typedef struct
{
    int16_t value;
} Robot_AxisCommand;

typedef struct
{
    Robot_AxisCommand axis[ROBOT_AXIS_COUNT];
} Robot_Model;

void RobotModel_Init(Robot_Model *model);
void RobotModel_Reset(Robot_Model *model);
ErrorStatus RobotModel_SetAxis(Robot_Model *model, Robot_AxisId axis, int16_t value);
ErrorStatus RobotModel_GetAxis(const Robot_Model *model, Robot_AxisId axis, int16_t *value);

#endif
