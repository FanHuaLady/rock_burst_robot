#include "control.h"

#include "binding_manager.h"
#include "config_store.h"
#include "device_hydraulic.h"
#include "drive_mixer.h"
#include "robot_model.h"
#include "throttle_pwm.h"
#include "vehicle_light.h"

#include <string.h>

#define CONTROL_DIRECTION_REVERSAL_THRESHOLD 8

static uint8_t control_initialized;
static Robot_Model control_robot_model;
static Control_OperatorCommand control_operator_command;
static uint32_t control_operator_last_update_ms;
static uint32_t control_operator_suspend_until_ms;
static volatile uint32_t control_ms;
static uint8_t control_operator_dirty;
static uint8_t control_operator_suspended;
static uint16_t control_operator_current_limit_ma;
static uint16_t control_operator_aux_current_limit_ma;
static uint16_t control_actual_current_ma[ROBOT_AXIS_COUNT];
static uint8_t control_actual_current_valid[ROBOT_AXIS_COUNT];
static uint8_t control_axis_faulted[ROBOT_AXIS_COUNT];
static uint8_t control_axis_fault_valid[ROBOT_AXIS_COUNT];
static uint32_t control_actual_current_last_sample_ms;
static Robot_AxisId control_actual_current_axis;
static uint8_t control_jog_active;
static DeviceHydraulic_PhysicalChannel control_jog_physical;
static uint8_t control_jog_reverse;
static uint32_t control_jog_deadline_ms;
static uint8_t control_throttle_initialized;
static uint8_t control_throttle_enabled;
static uint16_t control_throttle_duty_permille;
static uint16_t control_output_current_ma[ROBOT_AXIS_COUNT];
static uint32_t control_next_ramp_ms[ROBOT_AXIS_COUNT];
static uint8_t control_output_reverse[ROBOT_AXIS_COUNT];
static uint8_t control_output_reverse_valid[ROBOT_AXIS_COUNT];

static ErrorStatus Control_RefreshAxisOutput(Robot_AxisId axis);
static ErrorStatus Control_UpdateHydraulicRamp(void);
static ErrorStatus Control_ApplyOperatorCommand(const Control_OperatorCommand *command);
static ErrorStatus Control_SetDriveLimited(int16_t turn,
                                           int16_t forward,
                                           uint16_t current_limit_ma);
static uint8_t Control_ClampAxisValue(int16_t value, int16_t *clamped_value);
static void Control_ClampOperatorCommand(Control_OperatorCommand *command);
static uint8_t Control_OperatorCommandHasMotion(const Control_OperatorCommand *command);
static uint8_t Control_OperatorCommandHasDirectionReversal(
    const Control_OperatorCommand *previous,
    const Control_OperatorCommand *next);
static uint8_t Control_IsDirectionReversal(int16_t previous, int16_t next);
static void Control_UpdateOperatorSuspension(void);
static int16_t Control_ScaleOperatorAxisValue(int16_t value,
                                              uint16_t current_limit_ma);
static void Control_PollAxisHealth(void);
static void Control_UpdateAxisHealth(Robot_AxisId axis);
static uint8_t Control_HasFault(uint16_t status, uint16_t fault0, uint16_t fault1);

void Control_Init(void)
{
    if (control_initialized != 0U)
    {
        return;
    }

    RobotModel_Init(&control_robot_model);
    ControlBindingManager_Init();
    DeviceHydraulic_Init();
    VehicleLight_Init();
    control_throttle_initialized =
        (ThrottlePwm_InitDefault() == SUCCESS) ? 1U : 0U;
    (void)memset(&control_operator_command, 0, sizeof(control_operator_command));
    (void)memset(control_actual_current_ma, 0, sizeof(control_actual_current_ma));
    (void)memset(control_actual_current_valid, 0, sizeof(control_actual_current_valid));
    (void)memset(control_axis_faulted, 0, sizeof(control_axis_faulted));
    (void)memset(control_axis_fault_valid, 0, sizeof(control_axis_fault_valid));
    control_operator_last_update_ms = 0U;
    control_operator_suspend_until_ms = 0U;
    control_ms = 0U;
    control_operator_dirty = 0U;
    control_operator_suspended = 0U;
    control_operator_current_limit_ma = CONTROL_OPERATOR_DEFAULT_CURRENT_LIMIT_MA;
    control_operator_aux_current_limit_ma = CONTROL_OPERATOR_DEFAULT_CURRENT_LIMIT_MA;
    control_actual_current_last_sample_ms = 0U;
    control_actual_current_axis = ROBOT_AXIS_LEFT_TRACK;
    control_jog_active = 0U;
    control_jog_reverse = 0U;
    control_jog_deadline_ms = 0U;
    control_throttle_enabled = 0U;
    control_throttle_duty_permille = 0U;
    (void)memset(control_output_current_ma, 0, sizeof(control_output_current_ma));
    (void)memset(control_next_ramp_ms, 0, sizeof(control_next_ramp_ms));
    (void)memset(control_output_reverse, 0, sizeof(control_output_reverse));
    (void)memset(control_output_reverse_valid, 0, sizeof(control_output_reverse_valid));
    if (Control_LoadConfig() == ERROR)
    {
        (void)Control_ApplyDefaultConfig();
    }

    control_initialized = 1U;
}

void Control_Poll(void)
{
    if (control_initialized == 0U)
    {
        return;
    }

    Control_UpdateOperatorSuspension();

    if (control_operator_dirty != 0U)
    {
        if (Control_ApplyOperatorCommand(&control_operator_command) == ERROR)
        {
            Control_StopAll();
            return;
        }
        control_operator_dirty = 0U;
    }

    if ((control_jog_active != 0U) &&
        ((int32_t)(control_ms - control_jog_deadline_ms) >= 0))
    {
        Control_StopJog();
    }

    if (Control_UpdateHydraulicRamp() == ERROR)
    {
        Control_StopAll();
        return;
    }

    Control_PollAxisHealth();
}

void Control_Tick1ms(void)
{
    control_ms++;
    DeviceHydraulic_Tick1ms();
    Control_UpdateOperatorSuspension();

    if ((Control_OperatorCommandHasMotion(&control_operator_command) != 0U) &&
        ((uint32_t)(control_ms - control_operator_last_update_ms) >
         CONTROL_OPERATOR_TIMEOUT_MS))
    {
        (void)memset(&control_operator_command, 0, sizeof(control_operator_command));
        control_operator_dirty = 1U;
        control_operator_suspended = 1U;
        control_operator_suspend_until_ms = 0U;
        control_operator_last_update_ms = control_ms;
    }
}

ErrorStatus Control_SetOperatorCommand(const Control_OperatorCommand *command)
{
    Control_OperatorCommand previous;
    Control_OperatorCommand clamped;

    if ((control_initialized == 0U) || (command == 0) ||
        (control_jog_active != 0U))
    {
        return ERROR;
    }

    clamped = *command;
    Control_ClampOperatorCommand(&clamped);
    previous = control_operator_command;
    Control_UpdateOperatorSuspension();
    if ((control_operator_suspended != 0U) &&
        (Control_OperatorCommandHasMotion(&clamped) != 0U))
    {
        return ERROR;
    }
    if (Control_OperatorCommandHasDirectionReversal(&previous, &clamped) != 0U)
    {
        (void)memset(&control_operator_command, 0, sizeof(control_operator_command));
        control_operator_last_update_ms = control_ms;
        control_operator_dirty = 1U;
        control_operator_suspended = 1U;
        control_operator_suspend_until_ms = control_ms + CONTROL_OPERATOR_REVERSAL_COOLDOWN_MS;
        return ERROR;
    }

    control_operator_last_update_ms = control_ms;
    if (memcmp(&control_operator_command,
               &clamped,
               sizeof(control_operator_command)) != 0)
    {
        control_operator_command = clamped;
        control_operator_dirty = 1U;
    }

    return SUCCESS;
}

void Control_GetOperatorCommand(Control_OperatorCommand *command)
{
    if (command == 0)
    {
        return;
    }

    *command = control_operator_command;
}

void Control_StopOperator(void)
{
    (void)memset(&control_operator_command, 0, sizeof(control_operator_command));
    control_operator_last_update_ms = control_ms;
    control_operator_dirty = 1U;
}

void Control_RestartOperator(void)
{
    (void)memset(&control_operator_command, 0, sizeof(control_operator_command));
    control_operator_last_update_ms = control_ms;
    control_operator_suspended = 0U;
    control_operator_suspend_until_ms = 0U;
    control_operator_dirty = 1U;
}

uint8_t Control_IsOperatorSuspended(void)
{
    return control_operator_suspended;
}

ErrorStatus Control_SetOperatorCurrentLimit(uint16_t current_ma)
{
    if ((control_initialized == 0U) ||
        (current_ma < CONTROL_OPERATOR_MIN_CURRENT_LIMIT_MA) ||
        (current_ma > CONTROL_OPERATOR_MAX_CURRENT_LIMIT_MA))
    {
        return ERROR;
    }

    if (control_operator_current_limit_ma != current_ma)
    {
        control_operator_current_limit_ma = current_ma;
        control_operator_dirty = 1U;
    }

    return SUCCESS;
}

uint16_t Control_GetOperatorCurrentLimit(void)
{
    return control_operator_current_limit_ma;
}

ErrorStatus Control_SetOperatorAuxCurrentLimit(uint16_t current_ma)
{
    if ((control_initialized == 0U) ||
        (current_ma < CONTROL_OPERATOR_MIN_CURRENT_LIMIT_MA) ||
        (current_ma > CONTROL_OPERATOR_MAX_CURRENT_LIMIT_MA))
    {
        return ERROR;
    }

    if (control_operator_aux_current_limit_ma != current_ma)
    {
        control_operator_aux_current_limit_ma = current_ma;
        control_operator_dirty = 1U;
    }

    return SUCCESS;
}

ErrorStatus Control_ClearDeviceFault(MAX22216_DeviceId device)
{
    Control_Binding bindings[ROBOT_AXIS_COUNT];
    Robot_AxisId axis;
    ErrorStatus result;

    if ((control_initialized == 0U) || (device >= MAX22216_DEV_COUNT))
    {
        return ERROR;
    }

    if (MAX22216_AllOff(device) == ERROR)
    {
        return ERROR;
    }
    (void)MAX22216_EnterActiveMode(device);
    if (MAX22216_ClearFaults(device) == ERROR)
    {
        return ERROR;
    }

    result = SUCCESS;
    ControlBindingManager_GetTable(bindings);
    for (axis = 0; axis < ROBOT_AXIS_COUNT; axis++)
    {
        if ((bindings[axis].valid != 0U) &&
            (bindings[axis].device == device))
        {
            if (Control_RefreshAxisOutput(axis) == ERROR)
            {
                result = ERROR;
            }
            Control_UpdateAxisHealth(axis);
        }
    }

    return result;
}

ErrorStatus Control_GetDeviceFaults(MAX22216_DeviceId device,
                                    uint16_t *fault0,
                                    uint16_t *fault1)
{
    if ((control_initialized == 0U) ||
        (device >= MAX22216_DEV_COUNT) ||
        (fault0 == 0) ||
        (fault1 == 0))
    {
        return ERROR;
    }

    *fault0 = MAX22216_ReadReg(device, MAX22216_REG_FAULT0);
    *fault1 = MAX22216_ReadReg(device, MAX22216_REG_FAULT1);
    return SUCCESS;
}

ErrorStatus Control_ClearAxisFault(Robot_AxisId axis)
{
    Control_Binding binding;

    if ((control_initialized == 0U) ||
        (ControlBindingManager_Get(axis, &binding) == ERROR) ||
        (binding.valid == 0U))
    {
        return ERROR;
    }

    return Control_ClearDeviceFault(binding.device);
}

ErrorStatus Control_GetAxisFault(Robot_AxisId axis, uint8_t *faulted)
{
    Control_Binding binding;

    if ((control_initialized == 0U) || (faulted == 0))
    {
        return ERROR;
    }

    *faulted = 0U;
    if ((axis >= ROBOT_AXIS_COUNT) ||
        (ControlBindingManager_Get(axis, &binding) == ERROR) ||
        (binding.valid == 0U) ||
        (control_axis_fault_valid[axis] == 0U))
    {
        return ERROR;
    }

    *faulted = control_axis_faulted[axis];
    return SUCCESS;
}

ErrorStatus Control_SetAxis(Robot_AxisId axis, int16_t value)
{
    if (control_initialized == 0U)
    {
        return ERROR;
    }

    if (Control_ClampAxisValue(value, &value) == 0U)
    {
        return ERROR;
    }

    if (RobotModel_SetAxis(&control_robot_model, axis, value) == ERROR)
    {
        return ERROR;
    }

    return Control_RefreshAxisOutput(axis);
}

ErrorStatus Control_GetAxis(Robot_AxisId axis, int16_t *value)
{
    if (control_initialized == 0U)
    {
        return ERROR;
    }

    return RobotModel_GetAxis(&control_robot_model, axis, value);
}

ErrorStatus Control_GetAxisActualCurrent(Robot_AxisId axis, uint16_t *current_ma)
{
    Control_Binding binding;

    if ((control_initialized == 0U) || (current_ma == 0))
    {
        return ERROR;
    }

    *current_ma = 0U;
    if ((ControlBindingManager_Get(axis, &binding) == ERROR) ||
        (binding.valid == 0U))
    {
        return ERROR;
    }

    if (control_actual_current_valid[axis] == 0U)
    {
        return ERROR;
    }

    *current_ma = control_actual_current_ma[axis];
    return SUCCESS;
}

ErrorStatus Control_SetDrive(int16_t turn, int16_t forward)
{
    DriveMixer_Output output;

    if (control_initialized == 0U)
    {
        return ERROR;
    }

    if (DriveMixer_Mix(turn, forward, &output) == ERROR)
    {
        return ERROR;
    }

    if (Control_SetAxis(ROBOT_AXIS_LEFT_TRACK, output.left_track) == ERROR)
    {
        return ERROR;
    }
    if (Control_SetAxis(ROBOT_AXIS_RIGHT_TRACK, output.right_track) == ERROR)
    {
        return ERROR;
    }

    return SUCCESS;
}

ErrorStatus Control_JogPhysical(MAX22216_DeviceId device,
                                uint8_t channel,
                                FunctionalState reverse,
                                uint16_t current_ma)
{
    DeviceHydraulic_PhysicalChannel physical;
    uint8_t requested_reverse;

    if ((control_initialized == 0U) ||
        (device >= MAX22216_DEV_COUNT) ||
        (channel >= MAX22216_MAX_CHANNELS) ||
        (current_ma < CONTROL_JOG_MIN_CURRENT_MA) ||
        (current_ma > CONTROL_JOG_MAX_CURRENT_MA))
    {
        return ERROR;
    }

    physical.device = device;
    physical.channel = channel;
    requested_reverse = (reverse == ENABLE) ? 1U : 0U;
    if ((control_jog_active == 0U) ||
        (control_jog_physical.device != device) ||
        (control_jog_physical.channel != channel) ||
        (control_jog_reverse != requested_reverse))
    {
        Control_StopAll();
        if (DeviceHydraulic_SetDirection(physical, reverse) == ERROR)
        {
            return ERROR;
        }
        control_jog_physical = physical;
        control_jog_reverse = requested_reverse;
    }

    if ((DeviceHydraulic_ConfigCurrent(physical, current_ma) == ERROR) ||
        (DeviceHydraulic_ChannelOn(physical) == ERROR))
    {
        Control_StopJog();
        return ERROR;
    }

    control_jog_active = 1U;
    control_jog_deadline_ms = control_ms + CONTROL_JOG_TIMEOUT_MS;
    return SUCCESS;
}

void Control_StopJog(void)
{
    if (control_jog_active != 0U)
    {
        (void)DeviceHydraulic_ChannelOff(control_jog_physical);
    }
    control_jog_active = 0U;
    control_jog_deadline_ms = control_ms;
}

uint8_t Control_IsJogActive(void)
{
    return control_jog_active;
}

ErrorStatus Control_BindAxis(Robot_AxisId axis,
                             MAX22216_DeviceId device,
                             uint8_t channel,
                             FunctionalState reverse)
{
    ErrorStatus result;

    if (control_initialized == 0U)
    {
        return ERROR;
    }

    Control_StopAll();
    result = ControlBindingManager_Bind(axis, device, channel, reverse);
    if (result == SUCCESS)
    {
        result = Control_RefreshAxisOutput(axis);
    }
    return result;
}

ErrorStatus Control_UnbindAxis(Robot_AxisId axis)
{
    Control_Binding binding;
    ErrorStatus result;

    if (control_initialized == 0U)
    {
        return ERROR;
    }

    if (ControlBindingManager_Get(axis, &binding) == SUCCESS)
    {
        (void)DeviceHydraulic_ChannelOff((DeviceHydraulic_PhysicalChannel)
        {
            binding.device,
            binding.channel
        });
    }

    result = ControlBindingManager_Unbind(axis);
    if (result == SUCCESS)
    {
        result = Control_RefreshAxisOutput(axis);
    }
    return result;
}

ErrorStatus Control_GetBinding(Robot_AxisId axis, Control_Binding *binding)
{
    if ((control_initialized == 0U) || (binding == 0))
    {
        return ERROR;
    }

    if (ControlBindingManager_Get(axis, binding) == ERROR)
    {
        (void)memset(binding, 0, sizeof(*binding));
        return SUCCESS;
    }

    return SUCCESS;
}

void Control_GetBindings(Control_Binding bindings[ROBOT_AXIS_COUNT])
{
    ControlBindingManager_GetTable(bindings);
}

ErrorStatus Control_SetBindings(const Control_Binding bindings[ROBOT_AXIS_COUNT])
{
    if ((control_initialized == 0U) || (bindings == 0))
    {
        return ERROR;
    }

    Control_StopAll();
    return ControlBindingManager_SetTable(bindings);
}

ErrorStatus Control_ValidateBindings(uint8_t require_all_axes,
                                     Control_BindingValidation *validation)
{
    if (control_initialized == 0U)
    {
        return ERROR;
    }

    return ControlBindingManager_ValidateCurrent(require_all_axes, validation);
}

ErrorStatus Control_SaveConfig(void)
{
    Control_Binding bindings[ROBOT_AXIS_COUNT];

    if (control_initialized == 0U)
    {
        return ERROR;
    }

    if (ControlBindingManager_ValidateCurrent(0U, 0) == ERROR)
    {
        return ERROR;
    }

    Control_StopAll();
    ControlBindingManager_GetTable(bindings);
    return ControlConfigStore_SaveBindings(bindings);
}

ErrorStatus Control_LoadConfig(void)
{
    Control_Binding bindings[ROBOT_AXIS_COUNT];

    if (ControlConfigStore_LoadBindings(bindings) == ERROR)
    {
        return ERROR;
    }

    Control_StopAll();
    return ControlBindingManager_SetTable(bindings);
}

ErrorStatus Control_ApplyDefaultConfig(void)
{
    if (control_initialized != 0U)
    {
        Control_StopAll();
    }
    else
    {
        RobotModel_Reset(&control_robot_model);
        DeviceHydraulic_ResetAll();
    }

    return ControlBindingManager_ApplyDefaultBindings();
}

ErrorStatus Control_SetVehicleLight(uint8_t light, FunctionalState state)
{
    if ((control_initialized == 0U) || (light >= VEHICLE_LIGHT_COUNT))
    {
        return ERROR;
    }

    VehicleLight_Set((VehicleLight_Id)light, state);
    return SUCCESS;
}

FunctionalState Control_GetVehicleLightState(uint8_t light)
{
    if ((control_initialized == 0U) || (light >= VEHICLE_LIGHT_COUNT))
    {
        return DISABLE;
    }

    return VehicleLight_GetState((VehicleLight_Id)light);
}

ErrorStatus Control_SetThrottle(FunctionalState enabled, uint16_t duty_permille)
{
    if ((control_initialized == 0U) ||
        (control_throttle_initialized == 0U) ||
        (duty_permille > THROTTLE_PWM_DUTY_MAX))
    {
        return ERROR;
    }

    control_throttle_duty_permille = duty_permille;
    if (enabled != DISABLE)
    {
        if (ThrottlePwm_SetDutyPermille(duty_permille) == ERROR)
        {
            return ERROR;
        }
        ThrottlePwm_Start();
        control_throttle_enabled = 1U;
    }
    else
    {
        ThrottlePwm_Stop();
        control_throttle_enabled = 0U;
    }

    return SUCCESS;
}

uint8_t Control_IsThrottleEnabled(void)
{
    return control_throttle_enabled;
}

uint16_t Control_GetThrottleDutyPermille(void)
{
    return control_throttle_duty_permille;
}

void Control_StopAll(void)
{
    RobotModel_Reset(&control_robot_model);
    DeviceHydraulic_ResetAll();
    VehicleLight_AllOff();
    if (control_throttle_initialized != 0U)
    {
        ThrottlePwm_Stop();
    }
    control_throttle_enabled = 0U;
    (void)memset(&control_operator_command, 0, sizeof(control_operator_command));
    control_operator_last_update_ms = control_ms;
    control_operator_dirty = 0U;
    control_jog_active = 0U;
    control_jog_deadline_ms = control_ms;
    control_operator_suspend_until_ms = 0U;
    (void)memset(control_output_current_ma, 0, sizeof(control_output_current_ma));
    (void)memset(control_next_ramp_ms, 0, sizeof(control_next_ramp_ms));
    (void)memset(control_output_reverse, 0, sizeof(control_output_reverse));
    (void)memset(control_output_reverse_valid, 0, sizeof(control_output_reverse_valid));
}

static ErrorStatus Control_ApplyOperatorCommand(const Control_OperatorCommand *command)
{
    int16_t body_rotate;
    int16_t left_leg;
    int16_t right_leg;
    int16_t drill;

    if (command == 0)
    {
        return ERROR;
    }

    if (Control_SetDriveLimited(command->drive_turn,
                                command->drive_forward,
                                control_operator_current_limit_ma) == ERROR)
    {
        return ERROR;
    }
    body_rotate = Control_ScaleOperatorAxisValue(command->body_rotate,
                                                 control_operator_aux_current_limit_ma);
    left_leg = Control_ScaleOperatorAxisValue(command->left_leg,
                                               control_operator_aux_current_limit_ma);
    right_leg = Control_ScaleOperatorAxisValue(command->right_leg,
                                                control_operator_aux_current_limit_ma);
    drill = Control_ScaleOperatorAxisValue(command->drill,
                                            control_operator_aux_current_limit_ma);

    if (Control_SetAxis(ROBOT_AXIS_BODY_ROTATE, body_rotate) == ERROR)
    {
        return ERROR;
    }
    if (Control_SetAxis(ROBOT_AXIS_LEFT_LEG, left_leg) == ERROR)
    {
        return ERROR;
    }
    if (Control_SetAxis(ROBOT_AXIS_RIGHT_LEG, right_leg) == ERROR)
    {
        return ERROR;
    }
    if (Control_SetAxis(ROBOT_AXIS_DRILL_ROTATION, drill) == ERROR)
    {
        return ERROR;
    }

    return SUCCESS;
}

static ErrorStatus Control_SetDriveLimited(int16_t turn,
                                           int16_t forward,
                                           uint16_t current_limit_ma)
{
    DriveMixer_Output output;

    if (DriveMixer_Mix(turn, forward, &output) == ERROR)
    {
        return ERROR;
    }

    if (Control_SetAxis(ROBOT_AXIS_LEFT_TRACK,
                        Control_ScaleOperatorAxisValue(output.left_track,
                                                        current_limit_ma)) == ERROR)
    {
        return ERROR;
    }
    if (Control_SetAxis(ROBOT_AXIS_RIGHT_TRACK,
                        Control_ScaleOperatorAxisValue(output.right_track,
                                                        current_limit_ma)) == ERROR)
    {
        return ERROR;
    }

    return SUCCESS;
}

static ErrorStatus Control_RefreshAxisOutput(Robot_AxisId axis)
{
    Control_Binding binding;
    int16_t value;
    int32_t absolute_value;
    uint16_t current_ma;
    uint16_t output_current_ma;
    uint16_t start_current_ma;
    uint8_t reverse;
    uint8_t direction_changed;
    DeviceHydraulic_PhysicalChannel physical;

    if (ControlBindingManager_Get(axis, &binding) == ERROR)
    {
        return SUCCESS;
    }

    if (RobotModel_GetAxis(&control_robot_model, axis, &value) == ERROR)
    {
        return ERROR;
    }

    physical.device = binding.device;
    physical.channel = binding.channel;

    if (value == 0)
    {
        control_output_current_ma[axis] = 0U;
        control_next_ramp_ms[axis] = 0U;
        control_output_reverse_valid[axis] = 0U;
        return DeviceHydraulic_ChannelOff(physical);
    }

    absolute_value = (value < 0) ? -(int32_t)value : (int32_t)value;
    if (absolute_value > 100)
    {
        absolute_value = 100;
    }
    current_ma = (uint16_t)((absolute_value * MAX22216_MAX_HOLD_CURRENT_MA) / 100);
    if (current_ma < 100U)
    {
        current_ma = 100U;
    }

    reverse = ((value < 0) ? 1U : 0U) ^ (binding.reverse != 0U ? 1U : 0U);
    direction_changed = (control_output_reverse_valid[axis] == 0U) ||
                        (control_output_reverse[axis] != reverse);
    if (DeviceHydraulic_SetDirection(physical, (reverse != 0U) ? ENABLE : DISABLE) == ERROR)
    {
        return ERROR;
    }

    if (direction_changed)
    {
        control_output_current_ma[axis] = 0U;
    }

    output_current_ma = control_output_current_ma[axis];
    if (output_current_ma == 0U)
    {
        start_current_ma = (current_ma < CONTROL_HYDRAULIC_RAMP_START_CURRENT_MA) ?
                           current_ma :
                           CONTROL_HYDRAULIC_RAMP_START_CURRENT_MA;
        if (DeviceHydraulic_ConfigCurrent(physical, start_current_ma) == ERROR)
        {
            return ERROR;
        }
        control_output_current_ma[axis] = start_current_ma;
        control_next_ramp_ms[axis] = control_ms + CONTROL_HYDRAULIC_RAMP_INTERVAL_MS;
    }
    else if (current_ma < output_current_ma)
    {
        /* Reducing demand remains immediate; only increases are ramped. */
        if (DeviceHydraulic_ConfigCurrent(physical, current_ma) == ERROR)
        {
            return ERROR;
        }
        control_output_current_ma[axis] = current_ma;
    }

    if (DeviceHydraulic_ChannelOn(physical) == ERROR)
    {
        return ERROR;
    }

    control_output_reverse[axis] = reverse;
    control_output_reverse_valid[axis] = 1U;
    return SUCCESS;
}

static ErrorStatus Control_UpdateHydraulicRamp(void)
{
    Control_Binding binding;
    DeviceHydraulic_PhysicalChannel physical;
    Robot_AxisId axis;
    int16_t value;
    int32_t absolute_value;
    uint16_t target_current_ma;
    uint16_t next_current_ma;

    for (axis = ROBOT_AXIS_LEFT_TRACK; axis < ROBOT_AXIS_COUNT; axis++)
    {
        if (control_output_current_ma[axis] == 0U)
        {
            continue;
        }

        if ((int32_t)(control_ms - control_next_ramp_ms[axis]) < 0)
        {
            continue;
        }

        if ((ControlBindingManager_Get(axis, &binding) == ERROR) ||
            (RobotModel_GetAxis(&control_robot_model, axis, &value) == ERROR))
        {
            return ERROR;
        }

        if (value == 0)
        {
            control_output_current_ma[axis] = 0U;
            control_output_reverse_valid[axis] = 0U;
            continue;
        }

        absolute_value = (value < 0) ? -(int32_t)value : (int32_t)value;
        if (absolute_value > 100)
        {
            absolute_value = 100;
        }
        target_current_ma = (uint16_t)((absolute_value * MAX22216_MAX_HOLD_CURRENT_MA) / 100);
        if (target_current_ma < 100U)
        {
            target_current_ma = 100U;
        }

        if (control_output_current_ma[axis] >= target_current_ma)
        {
            continue;
        }

        next_current_ma = (uint16_t)(control_output_current_ma[axis] +
                                     CONTROL_HYDRAULIC_RAMP_STEP_CURRENT_MA);
        if (next_current_ma < control_output_current_ma[axis] ||
            next_current_ma > target_current_ma)
        {
            next_current_ma = target_current_ma;
        }

        physical.device = binding.device;
        physical.channel = binding.channel;
        if (DeviceHydraulic_ConfigCurrent(physical, next_current_ma) == ERROR)
        {
            return ERROR;
        }

        control_output_current_ma[axis] = next_current_ma;
        control_next_ramp_ms[axis] = control_ms + CONTROL_HYDRAULIC_RAMP_INTERVAL_MS;
    }

    return SUCCESS;
}

static uint8_t Control_ClampAxisValue(int16_t value, int16_t *clamped_value)
{
    if (clamped_value == 0)
    {
        return 0U;
    }

    if (value > 100)
    {
        value = 100;
    }
    else if (value < -100)
    {
        value = -100;
    }

    *clamped_value = value;
    return 1U;
}

static void Control_ClampOperatorCommand(Control_OperatorCommand *command)
{
    if (command == 0)
    {
        return;
    }

    (void)Control_ClampAxisValue(command->drive_turn, &command->drive_turn);
    (void)Control_ClampAxisValue(command->drive_forward, &command->drive_forward);
    (void)Control_ClampAxisValue(command->body_rotate, &command->body_rotate);
    (void)Control_ClampAxisValue(command->left_leg, &command->left_leg);
    (void)Control_ClampAxisValue(command->right_leg, &command->right_leg);
    (void)Control_ClampAxisValue(command->drill, &command->drill);
}

static uint8_t Control_OperatorCommandHasMotion(const Control_OperatorCommand *command)
{
    if (command == 0)
    {
        return 0U;
    }

    return ((command->drive_turn != 0) ||
            (command->drive_forward != 0) ||
            (command->body_rotate != 0) ||
            (command->left_leg != 0) ||
            (command->right_leg != 0) ||
            (command->drill != 0)) ? 1U : 0U;
}

static uint8_t Control_OperatorCommandHasDirectionReversal(
    const Control_OperatorCommand *previous,
    const Control_OperatorCommand *next)
{
    DriveMixer_Output previous_drive;
    DriveMixer_Output next_drive;

    if ((previous == 0) || (next == 0))
    {
        return 0U;
    }

    if ((DriveMixer_Mix(previous->drive_turn,
                        previous->drive_forward,
                        &previous_drive) == ERROR) ||
        (DriveMixer_Mix(next->drive_turn,
                        next->drive_forward,
                        &next_drive) == ERROR))
    {
        return 0U;
    }

    return (Control_IsDirectionReversal(previous_drive.left_track,
                                         next_drive.left_track) != 0U) ||
           (Control_IsDirectionReversal(previous_drive.right_track,
                                         next_drive.right_track) != 0U) ||
           (Control_IsDirectionReversal(previous->body_rotate,
                                         next->body_rotate) != 0U) ||
           (Control_IsDirectionReversal(previous->left_leg,
                                         next->left_leg) != 0U) ||
           (Control_IsDirectionReversal(previous->right_leg,
                                         next->right_leg) != 0U) ||
           (Control_IsDirectionReversal(previous->drill,
                                         next->drill) != 0U);
}

static uint8_t Control_IsDirectionReversal(int16_t previous, int16_t next)
{
    if (((previous >= -CONTROL_DIRECTION_REVERSAL_THRESHOLD) &&
         (previous <= CONTROL_DIRECTION_REVERSAL_THRESHOLD)) ||
        ((next >= -CONTROL_DIRECTION_REVERSAL_THRESHOLD) &&
         (next <= CONTROL_DIRECTION_REVERSAL_THRESHOLD)))
    {
        return 0U;
    }

    return (((previous > 0) && (next < 0)) ||
            ((previous < 0) && (next > 0))) ? 1U : 0U;
}

static void Control_UpdateOperatorSuspension(void)
{
    if ((control_operator_suspended == 0U) ||
        (control_operator_suspend_until_ms == 0U))
    {
        return;
    }

    if ((int32_t)(control_ms - control_operator_suspend_until_ms) >= 0)
    {
        control_operator_suspended = 0U;
        control_operator_suspend_until_ms = 0U;
    }
}

static int16_t Control_ScaleOperatorAxisValue(int16_t value,
                                              uint16_t current_limit_ma)
{
    int32_t magnitude;
    int32_t scaled;

    magnitude = (value < 0) ? -(int32_t)value : (int32_t)value;
    scaled = (magnitude * (int32_t)current_limit_ma +
              (MAX22216_MAX_HOLD_CURRENT_MA / 2U)) /
             MAX22216_MAX_HOLD_CURRENT_MA;
    if (value < 0)
    {
        scaled = -scaled;
    }

    return (int16_t)scaled;
}

static void Control_PollAxisHealth(void)
{
    Robot_AxisId axis;

    if ((uint32_t)(control_ms - control_actual_current_last_sample_ms) < 50U)
    {
        return;
    }

    control_actual_current_last_sample_ms = control_ms;
    axis = control_actual_current_axis;
    control_actual_current_axis++;
    if (control_actual_current_axis >= ROBOT_AXIS_COUNT)
    {
        control_actual_current_axis = ROBOT_AXIS_LEFT_TRACK;
    }

    Control_UpdateAxisHealth(axis);
}

static void Control_UpdateAxisHealth(Robot_AxisId axis)
{
    Control_Binding binding;
    uint16_t status;
    uint16_t fault0;
    uint16_t fault1;

    (void)memset(&binding, 0, sizeof(binding));
    if ((ControlBindingManager_Get(axis, &binding) == ERROR) ||
        (binding.valid == 0U))
    {
        control_actual_current_ma[axis] = 0U;
        control_actual_current_valid[axis] = 0U;
        control_axis_faulted[axis] = 0U;
        control_axis_fault_valid[axis] = 0U;
        return;
    }

    control_actual_current_ma[axis] =
        MAX22216_ReadChannelCurrent_mA(binding.device, binding.channel);
    control_actual_current_valid[axis] = 1U;

    status = MAX22216_ReadStatusReg(binding.device);
    fault0 = MAX22216_ReadReg(binding.device, MAX22216_REG_FAULT0);
    fault1 = MAX22216_ReadReg(binding.device, MAX22216_REG_FAULT1);
    control_axis_faulted[axis] = Control_HasFault(status, fault0, fault1);
    control_axis_fault_valid[axis] = 1U;
}

static uint8_t Control_HasFault(uint16_t status, uint16_t fault0, uint16_t fault1)
{
    return (((status & (MAX22216_STATUS_OVT |
                         MAX22216_STATUS_OCP |
                         MAX22216_STATUS_COMER |
                         MAX22216_STATUS_UVM)) != 0U) ||
            (fault0 != 0U) ||
            (fault1 != 0U)) ? 1U : 0U;
}
