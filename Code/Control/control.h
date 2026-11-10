#ifndef __CONTROL_H
#define __CONTROL_H

#include "stm32f4xx.h"
#include "binding_manager.h"
#include "robot_model.h"

#define CONTROL_OPERATOR_TIMEOUT_MS              3000U
#define CONTROL_OPERATOR_REVERSAL_COOLDOWN_MS     300U
#define CONTROL_OPERATOR_DEFAULT_CURRENT_LIMIT_MA 1500U
#define CONTROL_OPERATOR_MIN_CURRENT_LIMIT_MA     100U
#define CONTROL_OPERATOR_MAX_CURRENT_LIMIT_MA     MAX22216_MAX_HOLD_CURRENT_MA
#define CONTROL_JOG_TIMEOUT_MS                     300U
#define CONTROL_JOG_MIN_CURRENT_MA                 100U
#define CONTROL_JOG_MAX_CURRENT_MA                 800U
#define CONTROL_HYDRAULIC_RAMP_START_CURRENT_MA    100U
#define CONTROL_HYDRAULIC_RAMP_STEP_CURRENT_MA     300U
#define CONTROL_HYDRAULIC_RAMP_INTERVAL_MS           5U

typedef struct
{
    int16_t drive_turn;
    int16_t drive_forward;
    int16_t body_rotate;
    int16_t left_leg;
    int16_t right_leg;
    int16_t drill;
} Control_OperatorCommand;

void Control_Init(void);
void Control_Poll(void);
void Control_Tick1ms(void);
ErrorStatus Control_SetOperatorCommand(const Control_OperatorCommand *command);
void Control_GetOperatorCommand(Control_OperatorCommand *command);
void Control_StopOperator(void);
void Control_RestartOperator(void);
uint8_t Control_IsOperatorSuspended(void);
ErrorStatus Control_SetOperatorCurrentLimit(uint16_t current_ma);
ErrorStatus Control_SetOperatorAuxCurrentLimit(uint16_t current_ma);
uint16_t Control_GetOperatorCurrentLimit(void);
ErrorStatus Control_ClearDeviceFault(MAX22216_DeviceId device);
ErrorStatus Control_GetDeviceFaults(MAX22216_DeviceId device,
                                    uint16_t *fault0,
                                    uint16_t *fault1);
ErrorStatus Control_ClearAxisFault(Robot_AxisId axis);
ErrorStatus Control_GetAxisFault(Robot_AxisId axis, uint8_t *faulted);
ErrorStatus Control_SetAxis(Robot_AxisId axis, int16_t value);
ErrorStatus Control_GetAxis(Robot_AxisId axis, int16_t *value);
ErrorStatus Control_GetAxisActualCurrent(Robot_AxisId axis, uint16_t *current_ma);
ErrorStatus Control_SetDrive(int16_t turn, int16_t forward);
ErrorStatus Control_JogPhysical(MAX22216_DeviceId device,
                                uint8_t channel,
                                FunctionalState reverse,
                                uint16_t current_ma);
void Control_StopJog(void);
uint8_t Control_IsJogActive(void);
ErrorStatus Control_BindAxis(Robot_AxisId axis,
                             MAX22216_DeviceId device,
                             uint8_t channel,
                             FunctionalState reverse);
ErrorStatus Control_UnbindAxis(Robot_AxisId axis);
ErrorStatus Control_GetBinding(Robot_AxisId axis, Control_Binding *binding);
void Control_GetBindings(Control_Binding bindings[ROBOT_AXIS_COUNT]);
ErrorStatus Control_SetBindings(const Control_Binding bindings[ROBOT_AXIS_COUNT]);
ErrorStatus Control_ValidateBindings(uint8_t require_all_axes,
                                     Control_BindingValidation *validation);
ErrorStatus Control_SaveConfig(void);
ErrorStatus Control_LoadConfig(void);
ErrorStatus Control_ApplyDefaultConfig(void);
ErrorStatus Control_SetVehicleLight(uint8_t light, FunctionalState state);
FunctionalState Control_GetVehicleLightState(uint8_t light);
ErrorStatus Control_SetThrottle(FunctionalState enabled, uint16_t duty_permille);
uint8_t Control_IsThrottleEnabled(void);
uint16_t Control_GetThrottleDutyPermille(void);
void Control_StopAll(void);

#endif
