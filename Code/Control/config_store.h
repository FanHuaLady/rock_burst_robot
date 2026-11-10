#ifndef __CONTROL_CONFIG_STORE_H
#define __CONTROL_CONFIG_STORE_H

#include "binding_manager.h"
#include "stm32f4xx.h"

#define CONTROL_CONFIG_STORE_FLASH_ADDRESS 0x08060000UL
#define CONTROL_CONFIG_STORE_FLASH_SIZE    0x00020000UL

ErrorStatus ControlConfigStore_LoadBindings(Control_Binding bindings[ROBOT_AXIS_COUNT]);
ErrorStatus ControlConfigStore_SaveBindings(const Control_Binding bindings[ROBOT_AXIS_COUNT]);

#endif
