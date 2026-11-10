#ifndef __CONTROL_BINDING_MANAGER_H
#define __CONTROL_BINDING_MANAGER_H

#include "stm32f4xx.h"
#include "max22216.h"
#include "robot_model.h"

typedef struct
{
    uint8_t valid;
    MAX22216_DeviceId device;
    uint8_t channel;
    uint8_t reverse;
} Control_Binding;

#define CONTROL_BINDING_VALIDATE_OK                0x00000000UL
#define CONTROL_BINDING_VALIDATE_MISSING_REQUIRED  0x00000001UL
#define CONTROL_BINDING_VALIDATE_INVALID_PHYSICAL  0x00000002UL
#define CONTROL_BINDING_VALIDATE_DUPLICATE_CHANNEL 0x00000004UL

typedef struct
{
    uint32_t flags;
    uint16_t bound_count;
    Robot_AxisId first_missing_axis;
    Robot_AxisId problem_axis;
    Robot_AxisId duplicate_axis;
    MAX22216_DeviceId duplicate_device;
    uint8_t duplicate_channel;
} Control_BindingValidation;

void ControlBindingManager_Init(void);
void ControlBindingManager_Reset(void);
ErrorStatus ControlBindingManager_Bind(Robot_AxisId axis,
                                       MAX22216_DeviceId device,
                                       uint8_t channel,
                                       FunctionalState reverse);
ErrorStatus ControlBindingManager_Unbind(Robot_AxisId axis);
ErrorStatus ControlBindingManager_Get(Robot_AxisId axis,
                                      Control_Binding *binding);
void ControlBindingManager_GetTable(Control_Binding bindings[ROBOT_AXIS_COUNT]);
ErrorStatus ControlBindingManager_SetTable(const Control_Binding bindings[ROBOT_AXIS_COUNT]);
ErrorStatus ControlBindingManager_ValidateTable(const Control_Binding bindings[ROBOT_AXIS_COUNT],
                                                uint8_t require_all_axes,
                                                Control_BindingValidation *validation);
ErrorStatus ControlBindingManager_ValidateCurrent(uint8_t require_all_axes,
                                                  Control_BindingValidation *validation);
uint8_t ControlBindingManager_IsBound(Robot_AxisId axis);
ErrorStatus ControlBindingManager_ApplyDefaultBindings(void);

#endif
