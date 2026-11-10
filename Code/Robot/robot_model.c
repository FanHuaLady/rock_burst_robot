#include "robot_model.h"

#include <string.h>

void RobotModel_Init(Robot_Model *model)
{
    RobotModel_Reset(model);
}

void RobotModel_Reset(Robot_Model *model)
{
    if (model == 0)
    {
        return;
    }

    (void)memset(model, 0, sizeof(*model));
}

ErrorStatus RobotModel_SetAxis(Robot_Model *model, Robot_AxisId axis, int16_t value)
{
    if ((model == 0) || (axis >= ROBOT_AXIS_COUNT))
    {
        return ERROR;
    }

    model->axis[axis].value = value;
    return SUCCESS;
}

ErrorStatus RobotModel_GetAxis(const Robot_Model *model, Robot_AxisId axis, int16_t *value)
{
    if ((model == 0) || (value == 0) || (axis >= ROBOT_AXIS_COUNT))
    {
        return ERROR;
    }

    *value = model->axis[axis].value;
    return SUCCESS;
}
