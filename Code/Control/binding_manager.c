#include "binding_manager.h"

#include <string.h>

static Control_Binding control_binding_table[ROBOT_AXIS_COUNT];

static uint8_t ControlBindingManager_IsAxisValid(Robot_AxisId axis);
static uint8_t ControlBindingManager_IsPhysicalValid(const Control_Binding *binding);
static uint8_t ControlBindingManager_IsSamePhysical(const Control_Binding *left,
                                                    const Control_Binding *right);
static void ControlBindingManager_NormalizeBinding(Control_Binding *binding);
static void ControlBindingManager_InitValidation(Control_BindingValidation *validation);

void ControlBindingManager_Init(void)
{
    ControlBindingManager_Reset();
}

void ControlBindingManager_Reset(void)
{
    (void)memset(control_binding_table, 0, sizeof(control_binding_table));
}

ErrorStatus ControlBindingManager_Bind(Robot_AxisId axis,
                                       MAX22216_DeviceId device,
                                       uint8_t channel,
                                       FunctionalState reverse)
{
    Control_Binding *binding;
    Control_Binding candidate[ROBOT_AXIS_COUNT];

    if ((ControlBindingManager_IsAxisValid(axis) == 0U) ||
        (device >= MAX22216_DEV_COUNT) ||
        (channel >= MAX22216_MAX_CHANNELS))
    {
        return ERROR;
    }

    (void)memcpy(candidate, control_binding_table, sizeof(candidate));
    binding = &candidate[axis];
    binding->valid = 1U;
    binding->device = device;
    binding->channel = channel;
    binding->reverse = (reverse == ENABLE) ? 1U : 0U;

    if (ControlBindingManager_ValidateTable(candidate, 0U, 0) == ERROR)
    {
        return ERROR;
    }

    (void)memcpy(control_binding_table, candidate, sizeof(control_binding_table));
    return SUCCESS;
}

ErrorStatus ControlBindingManager_Unbind(Robot_AxisId axis)
{
    if (ControlBindingManager_IsAxisValid(axis) == 0U)
    {
        return ERROR;
    }

    (void)memset(&control_binding_table[axis], 0, sizeof(control_binding_table[axis]));
    return SUCCESS;
}

ErrorStatus ControlBindingManager_Get(Robot_AxisId axis,
                                      Control_Binding *binding)
{
    if ((binding == 0) || (ControlBindingManager_IsAxisValid(axis) == 0U))
    {
        return ERROR;
    }

    *binding = control_binding_table[axis];
    return (binding->valid != 0U) ? SUCCESS : ERROR;
}

void ControlBindingManager_GetTable(Control_Binding bindings[ROBOT_AXIS_COUNT])
{
    if (bindings == 0)
    {
        return;
    }

    (void)memcpy(bindings, control_binding_table, sizeof(control_binding_table));
}

ErrorStatus ControlBindingManager_SetTable(const Control_Binding bindings[ROBOT_AXIS_COUNT])
{
    Control_Binding candidate[ROBOT_AXIS_COUNT];
    Robot_AxisId axis;

    if (bindings == 0)
    {
        return ERROR;
    }

    (void)memset(candidate, 0, sizeof(candidate));
    for (axis = 0; axis < ROBOT_AXIS_COUNT; axis++)
    {
        candidate[axis] = bindings[axis];
        ControlBindingManager_NormalizeBinding(&candidate[axis]);
    }

    if (ControlBindingManager_ValidateTable(candidate, 0U, 0) == ERROR)
    {
        return ERROR;
    }

    (void)memcpy(control_binding_table, candidate, sizeof(control_binding_table));
    return SUCCESS;
}

ErrorStatus ControlBindingManager_ValidateTable(const Control_Binding bindings[ROBOT_AXIS_COUNT],
                                                uint8_t require_all_axes,
                                                Control_BindingValidation *validation)
{
    const Control_Binding *table;
    Robot_AxisId axis;
    Robot_AxisId compare_axis;
    uint32_t flags;

    table = (bindings != 0) ? bindings : control_binding_table;
    flags = CONTROL_BINDING_VALIDATE_OK;
    ControlBindingManager_InitValidation(validation);

    for (axis = 0; axis < ROBOT_AXIS_COUNT; axis++)
    {
        if (table[axis].valid == 0U)
        {
            if ((require_all_axes != 0U) &&
                ((flags & CONTROL_BINDING_VALIDATE_MISSING_REQUIRED) == 0U))
            {
                flags |= CONTROL_BINDING_VALIDATE_MISSING_REQUIRED;
                if (validation != 0)
                {
                    validation->first_missing_axis = axis;
                }
            }
            continue;
        }

        if (validation != 0)
        {
            validation->bound_count++;
        }

        if (ControlBindingManager_IsPhysicalValid(&table[axis]) == 0U)
        {
            flags |= CONTROL_BINDING_VALIDATE_INVALID_PHYSICAL;
            if ((validation != 0) && (validation->problem_axis >= ROBOT_AXIS_COUNT))
            {
                validation->problem_axis = axis;
            }
            continue;
        }

        for (compare_axis = (Robot_AxisId)(axis + 1U);
             compare_axis < ROBOT_AXIS_COUNT;
             compare_axis++)
        {
            if ((table[compare_axis].valid != 0U) &&
                (ControlBindingManager_IsSamePhysical(&table[axis], &table[compare_axis]) != 0U))
            {
                flags |= CONTROL_BINDING_VALIDATE_DUPLICATE_CHANNEL;
                if ((validation != 0) && (validation->duplicate_axis >= ROBOT_AXIS_COUNT))
                {
                    validation->problem_axis = axis;
                    validation->duplicate_axis = compare_axis;
                    validation->duplicate_device = table[axis].device;
                    validation->duplicate_channel = table[axis].channel;
                }
            }
        }
    }

    if (validation != 0)
    {
        validation->flags = flags;
    }

    return (flags == CONTROL_BINDING_VALIDATE_OK) ? SUCCESS : ERROR;
}

ErrorStatus ControlBindingManager_ValidateCurrent(uint8_t require_all_axes,
                                                  Control_BindingValidation *validation)
{
    return ControlBindingManager_ValidateTable(control_binding_table,
                                               require_all_axes,
                                               validation);
}

uint8_t ControlBindingManager_IsBound(Robot_AxisId axis)
{
    if (ControlBindingManager_IsAxisValid(axis) == 0U)
    {
        return 0U;
    }

    return (control_binding_table[axis].valid != 0U) ? 1U : 0U;
}

ErrorStatus ControlBindingManager_ApplyDefaultBindings(void)
{
    ControlBindingManager_Reset();
    return SUCCESS;
}

static uint8_t ControlBindingManager_IsAxisValid(Robot_AxisId axis)
{
    return (axis < ROBOT_AXIS_COUNT) ? 1U : 0U;
}

static uint8_t ControlBindingManager_IsPhysicalValid(const Control_Binding *binding)
{
    if ((binding == 0) || (binding->valid == 0U))
    {
        return 1U;
    }

    return ((binding->device < MAX22216_DEV_COUNT) &&
            (binding->channel < MAX22216_MAX_CHANNELS)) ? 1U : 0U;
}

static uint8_t ControlBindingManager_IsSamePhysical(const Control_Binding *left,
                                                    const Control_Binding *right)
{
    if ((left == 0) || (right == 0) ||
        (left->valid == 0U) || (right->valid == 0U))
    {
        return 0U;
    }

    return ((left->device == right->device) &&
            (left->channel == right->channel)) ? 1U : 0U;
}

static void ControlBindingManager_NormalizeBinding(Control_Binding *binding)
{
    if (binding == 0)
    {
        return;
    }

    if (binding->valid == 0U)
    {
        (void)memset(binding, 0, sizeof(*binding));
        return;
    }

    binding->valid = 1U;
    binding->reverse = (binding->reverse != 0U) ? 1U : 0U;
}

static void ControlBindingManager_InitValidation(Control_BindingValidation *validation)
{
    if (validation == 0)
    {
        return;
    }

    (void)memset(validation, 0, sizeof(*validation));
    validation->first_missing_axis = ROBOT_AXIS_COUNT;
    validation->problem_axis = ROBOT_AXIS_COUNT;
    validation->duplicate_axis = ROBOT_AXIS_COUNT;
    validation->duplicate_device = MAX22216_DEV_COUNT;
}
