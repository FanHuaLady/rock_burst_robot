#ifndef __ROBOT_PROTOCOL_H
#define __ROBOT_PROTOCOL_H

#include "binding_manager.h"

#include <stdint.h>

#define ROBOT_PROTOCOL_VERSION       "1.0"
#define ROBOT_PROTOCOL_MAX_LINE      384U
#define ROBOT_PROTOCOL_MAX_TYPE_LEN  32U

typedef enum
{
    RP_MSG_INVALID = 0,
    RP_MSG_HELLO_ACK,
    RP_MSG_PING,
    RP_MSG_MODE,
    RP_MSG_MOTION,
    RP_MSG_LIGHT,
    RP_MSG_THROTTLE,
    RP_MSG_STOP,
    RP_MSG_RESTART,
    RP_MSG_CONFIG_GET,
    RP_MSG_CONFIG_SET,
    RP_MSG_CONFIG_SAVE,
    RP_MSG_CONFIG_LOAD,
    RP_MSG_CONFIG_DEFAULT,
    RP_MSG_JOG,
    RP_MSG_JOG_STOP,
    RP_MSG_CHIP_FAULTS_GET,
    RP_MSG_CHIP_FAULT_CLEAR,
    RP_MSG_STATE_GET
} RobotProtocol_MessageType;

typedef struct
{
    RobotProtocol_MessageType type;
    char type_name[ROBOT_PROTOCOL_MAX_TYPE_LEN];
    char mode[12];
    int32_t turn;
    int32_t forward;
    int32_t body_rotate;
    int32_t left_leg;
    int32_t right_leg;
    int32_t drill;
    int32_t current_ma;
    int32_t aux_current_ma;
    int32_t light;
    int32_t enabled;
    int32_t duty_permille;
    int32_t device;
    int32_t channel;
    int32_t reverse;
    Control_Binding bindings[ROBOT_AXIS_COUNT];
} RobotProtocol_Message;

RobotProtocol_MessageType RobotProtocol_ParseLine(const char *line,
                                                  uint16_t length,
                                                  RobotProtocol_Message *message);
uint16_t RobotProtocol_BuildHello(char *buffer,
                                  uint16_t buffer_length,
                                  const char *device_id,
                                  const char *firmware_version);
uint16_t RobotProtocol_BuildPong(char *buffer, uint16_t buffer_length);
uint16_t RobotProtocol_BuildAck(char *buffer,
                                uint16_t buffer_length,
                                const char *original_type,
                                uint8_t success);
uint16_t RobotProtocol_BuildError(char *buffer,
                                  uint16_t buffer_length,
                                  int32_t code,
                                  const char *message);

#endif
