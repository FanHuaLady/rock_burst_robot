#include "control_server.h"

#include "binding_manager.h"
#include "control.h"
#include "robot_protocol.h"
#include "throttle_pwm.h"
#include "vehicle_light.h"
#include "network.h"

#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include <stdio.h>
#include <string.h>

#define CONTROL_SERVER_PORT          9090U
#define CONTROL_SERVER_RX_SIZE      (ROBOT_PROTOCOL_MAX_LINE + 1U)
#define CONTROL_SERVER_TX_SIZE       512U
#define CONTROL_SERVER_LEASE_MS      1000U

typedef enum
{
    CONTROL_SERVER_MODE_SAFE = 0,
    CONTROL_SERVER_MODE_CONFIG,
    CONTROL_SERVER_MODE_DRIVE
} ControlServer_Mode;

static struct tcp_pcb *control_server_listen_pcb;
static struct tcp_pcb *control_server_client_pcb;
static char control_server_rx[CONTROL_SERVER_RX_SIZE];
static uint16_t control_server_rx_len;
static ControlServer_Mode control_server_mode;
static uint8_t control_server_handshake_complete;
static uint32_t control_server_last_valid_message_ms;

static err_t ControlServer_Accept(void *arg, struct tcp_pcb *pcb, err_t err);
static err_t ControlServer_Recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static void ControlServer_Error(void *arg, err_t err);
static void ControlServer_CloseClient(struct tcp_pcb *pcb);
static void ControlServer_EnterSafe(void);
static void ControlServer_ProcessLine(struct tcp_pcb *pcb, const char *line, uint16_t length);
static uint8_t ControlServer_Dispatch(const RobotProtocol_Message *message);
static err_t ControlServer_Send(struct tcp_pcb *pcb, const char *text, uint16_t length);
static void ControlServer_SendAck(struct tcp_pcb *pcb,
                                 const char *original_type,
                                 uint8_t success);
static void ControlServer_SendError(struct tcp_pcb *pcb, int32_t code, const char *message);
static void ControlServer_SendConfig(struct tcp_pcb *pcb);
static void ControlServer_SendChipFaults(struct tcp_pcb *pcb);
static void ControlServer_SendState(struct tcp_pcb *pcb);

void ControlServer_Init(void)
{
    struct tcp_pcb *pcb;

    control_server_listen_pcb = 0;
    control_server_client_pcb = 0;
    control_server_rx_len = 0U;
    control_server_mode = CONTROL_SERVER_MODE_SAFE;
    control_server_handshake_complete = 0U;
    control_server_last_valid_message_ms = 0U;

    pcb = tcp_new();
    if (pcb == 0)
    {
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, CONTROL_SERVER_PORT) != ERR_OK)
    {
        tcp_abort(pcb);
        return;
    }

    control_server_listen_pcb = tcp_listen(pcb);
    if (control_server_listen_pcb == 0)
    {
        tcp_abort(pcb);
        return;
    }
    tcp_accept(control_server_listen_pcb, ControlServer_Accept);
}

void ControlServer_Process(void)
{
    uint32_t now;

    if (control_server_client_pcb == 0)
    {
        return;
    }

    if (Network_IsReady() == DISABLE)
    {
        ControlServer_CloseClient(control_server_client_pcb);
        return;
    }

    now = Network_GetUptimeMs();
    if ((uint32_t)(now - control_server_last_valid_message_ms) >=
        CONTROL_SERVER_LEASE_MS)
    {
        ControlServer_CloseClient(control_server_client_pcb);
    }
}

static err_t ControlServer_Accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct tcp_pcb *old_pcb;
    char response[CONTROL_SERVER_TX_SIZE];
    uint16_t length;

    (void)arg;
    if ((err != ERR_OK) || (pcb == 0))
    {
        return ERR_OK;
    }

    old_pcb = control_server_client_pcb;
    if (old_pcb != 0)
    {
        control_server_client_pcb = 0;
        tcp_arg(old_pcb, 0);
        tcp_recv(old_pcb, 0);
        tcp_err(old_pcb, 0);
        tcp_abort(old_pcb);
    }

    ControlServer_EnterSafe();
    control_server_client_pcb = pcb;
    control_server_rx_len = 0U;
    control_server_handshake_complete = 0U;
    control_server_last_valid_message_ms = Network_GetUptimeMs();
    tcp_arg(pcb, pcb);
    tcp_recv(pcb, ControlServer_Recv);
    tcp_err(pcb, ControlServer_Error);

    length = RobotProtocol_BuildHello(response,
                                      sizeof(response),
                                      "rockburst",
                                      "1.0.0");
    if (length != 0U)
    {
        (void)ControlServer_Send(pcb, response, length);
    }
    return ERR_OK;
}

static err_t ControlServer_Recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    struct pbuf *part;
    uint16_t index;
    uint8_t value;

    (void)arg;
    if ((p == 0) || (err != ERR_OK))
    {
        if (p != 0)
        {
            pbuf_free(p);
        }
        ControlServer_CloseClient(pcb);
        return ERR_OK;
    }

    tcp_recved(pcb, p->tot_len);
    for (part = p; part != 0; part = part->next)
    {
        for (index = 0U; index < part->len; index++)
        {
            value = ((const uint8_t *)part->payload)[index];
            if (value == '\r')
            {
                continue;
            }
            if (value == '\n')
            {
                if (control_server_rx_len != 0U)
                {
                    control_server_rx[control_server_rx_len] = '\0';
                    ControlServer_ProcessLine(pcb,
                                              control_server_rx,
                                              control_server_rx_len);
                    control_server_rx_len = 0U;
                    if (control_server_client_pcb != pcb)
                    {
                        pbuf_free(p);
                        return ERR_OK;
                    }
                }
                continue;
            }
            if (control_server_rx_len >= (CONTROL_SERVER_RX_SIZE - 1U))
            {
                pbuf_free(p);
                ControlServer_CloseClient(pcb);
                return ERR_OK;
            }
            control_server_rx[control_server_rx_len++] = (char)value;
        }
    }
    pbuf_free(p);
    return ERR_OK;
}

static void ControlServer_Error(void *arg, err_t err)
{
    (void)err;
    if ((struct tcp_pcb *)arg == control_server_client_pcb)
    {
        control_server_client_pcb = 0;
        control_server_rx_len = 0U;
        control_server_handshake_complete = 0U;
        ControlServer_EnterSafe();
    }
}

static void ControlServer_CloseClient(struct tcp_pcb *pcb)
{
    if (pcb == 0)
    {
        return;
    }

    ControlServer_EnterSafe();
    control_server_rx_len = 0U;
    control_server_handshake_complete = 0U;
    if (pcb == control_server_client_pcb)
    {
        control_server_client_pcb = 0;
    }
    tcp_arg(pcb, 0);
    tcp_recv(pcb, 0);
    tcp_err(pcb, 0);
    if (tcp_close(pcb) != ERR_OK)
    {
        tcp_abort(pcb);
    }
}

static void ControlServer_EnterSafe(void)
{
    Control_StopAll();
    control_server_mode = CONTROL_SERVER_MODE_SAFE;
}

static void ControlServer_ProcessLine(struct tcp_pcb *pcb,
                                      const char *line,
                                      uint16_t length)
{
    RobotProtocol_Message message;
    RobotProtocol_MessageType type;

    type = RobotProtocol_ParseLine(line, length, &message);
    if (type == RP_MSG_INVALID)
    {
        ControlServer_SendError(pcb, 1, "invalid_message");
        return;
    }

    control_server_last_valid_message_ms = Network_GetUptimeMs();
    if (type == RP_MSG_HELLO_ACK)
    {
        control_server_handshake_complete = 1U;
        ControlServer_SendAck(pcb, message.type_name, 1U);
        return;
    }
    if (control_server_handshake_complete == 0U)
    {
        ControlServer_SendError(pcb, 2, "handshake_required");
        return;
    }
    if (type == RP_MSG_PING)
    {
        char response[CONTROL_SERVER_TX_SIZE];
        uint16_t response_length = RobotProtocol_BuildPong(response, sizeof(response));
        if (response_length != 0U)
        {
            (void)ControlServer_Send(pcb, response, response_length);
        }
        return;
    }

    ControlServer_SendAck(pcb,
                         message.type_name,
                         ControlServer_Dispatch(&message));
}

static uint8_t ControlServer_Dispatch(const RobotProtocol_Message *message)
{
    Control_OperatorCommand command;

    switch (message->type)
    {
        case RP_MSG_MODE:
            if (strcmp(message->mode, "safe") == 0)
            {
                ControlServer_EnterSafe();
            }
            else if (strcmp(message->mode, "config") == 0)
            {
                Control_StopAll();
                control_server_mode = CONTROL_SERVER_MODE_CONFIG;
            }
            else
            {
                Control_StopAll();
                Control_RestartOperator();
                control_server_mode = CONTROL_SERVER_MODE_DRIVE;
            }
            return 1U;

        case RP_MSG_MOTION:
            if (control_server_mode != CONTROL_SERVER_MODE_DRIVE)
            {
                return 0U;
            }
            command.drive_turn = (int16_t)message->turn;
            command.drive_forward = (int16_t)message->forward;
            command.body_rotate = (int16_t)message->body_rotate;
            command.left_leg = (int16_t)message->left_leg;
            command.right_leg = (int16_t)message->right_leg;
            command.drill = (int16_t)message->drill;
            return ((Control_SetOperatorCurrentLimit((uint16_t)message->current_ma) == SUCCESS) &&
                    (Control_SetOperatorAuxCurrentLimit((uint16_t)message->aux_current_ma) == SUCCESS) &&
                    (Control_SetOperatorCommand(&command) == SUCCESS)) ? 1U : 0U;

        case RP_MSG_LIGHT:
            return ((control_server_mode == CONTROL_SERVER_MODE_DRIVE) &&
                    (Control_SetVehicleLight((uint8_t)message->light,
                                             (message->enabled != 0) ? ENABLE : DISABLE) == SUCCESS))
                       ? 1U : 0U;

        case RP_MSG_THROTTLE:
            return ((control_server_mode == CONTROL_SERVER_MODE_DRIVE) &&
                    (Control_SetThrottle((message->enabled != 0) ? ENABLE : DISABLE,
                                         (uint16_t)message->duty_permille) == SUCCESS))
                       ? 1U : 0U;

        case RP_MSG_STOP:
            ControlServer_EnterSafe();
            return 1U;

        case RP_MSG_RESTART:
            if (control_server_mode != CONTROL_SERVER_MODE_DRIVE)
            {
                return 0U;
            }
            Control_RestartOperator();
            return 1U;

        case RP_MSG_CONFIG_GET:
            if (control_server_mode != CONTROL_SERVER_MODE_CONFIG)
            {
                return 0U;
            }
            ControlServer_SendConfig(control_server_client_pcb);
            return 1U;

        case RP_MSG_CONFIG_SET:
            return ((control_server_mode == CONTROL_SERVER_MODE_CONFIG) &&
                    (Control_SetBindings(message->bindings) == SUCCESS)) ? 1U : 0U;

        case RP_MSG_CONFIG_SAVE:
            return ((control_server_mode == CONTROL_SERVER_MODE_CONFIG) &&
                    (Control_SaveConfig() == SUCCESS)) ? 1U : 0U;

        case RP_MSG_CONFIG_LOAD:
            return ((control_server_mode == CONTROL_SERVER_MODE_CONFIG) &&
                    (Control_LoadConfig() == SUCCESS)) ? 1U : 0U;

        case RP_MSG_CONFIG_DEFAULT:
            return ((control_server_mode == CONTROL_SERVER_MODE_CONFIG) &&
                    (Control_ApplyDefaultConfig() == SUCCESS)) ? 1U : 0U;

        case RP_MSG_JOG:
            return ((control_server_mode == CONTROL_SERVER_MODE_CONFIG) &&
                    (Control_JogPhysical((MAX22216_DeviceId)message->device,
                                         (uint8_t)message->channel,
                                         (message->reverse != 0) ? ENABLE : DISABLE,
                                         (uint16_t)message->current_ma) == SUCCESS)) ? 1U : 0U;

        case RP_MSG_JOG_STOP:
            Control_StopJog();
            return 1U;

        case RP_MSG_CHIP_FAULTS_GET:
            ControlServer_SendChipFaults(control_server_client_pcb);
            return 1U;

        case RP_MSG_CHIP_FAULT_CLEAR:
            return (Control_ClearDeviceFault((MAX22216_DeviceId)message->device) == SUCCESS)
                       ? 1U : 0U;

        case RP_MSG_STATE_GET:
            ControlServer_SendState(control_server_client_pcb);
            return 1U;

        default:
            return 0U;
    }
}

static err_t ControlServer_Send(struct tcp_pcb *pcb, const char *text, uint16_t length)
{
    err_t result;

    if ((pcb == 0) || (text == 0) || (length == 0U))
    {
        return ERR_ARG;
    }
    result = tcp_write(pcb, text, length, TCP_WRITE_FLAG_COPY);
    if (result == ERR_OK)
    {
        result = tcp_output(pcb);
    }
    return result;
}

static void ControlServer_SendAck(struct tcp_pcb *pcb,
                                  const char *original_type,
                                  uint8_t success)
{
    char response[CONTROL_SERVER_TX_SIZE];
    uint16_t length = RobotProtocol_BuildAck(response,
                                             sizeof(response),
                                             original_type,
                                             success);
    if (length != 0U)
    {
        (void)ControlServer_Send(pcb, response, length);
    }
}

static void ControlServer_SendError(struct tcp_pcb *pcb, int32_t code, const char *message)
{
    char response[CONTROL_SERVER_TX_SIZE];
    uint16_t length = RobotProtocol_BuildError(response,
                                              sizeof(response),
                                              code,
                                              message);
    if (length != 0U)
    {
        (void)ControlServer_Send(pcb, response, length);
    }
}

static void ControlServer_SendConfig(struct tcp_pcb *pcb)
{
    Control_Binding bindings[ROBOT_AXIS_COUNT];
    char response[CONTROL_SERVER_TX_SIZE];
    int offset;
    int written;
    uint16_t length;
    Robot_AxisId axis;

    Control_GetBindings(bindings);
    offset = snprintf(response,
                      sizeof(response),
                      "{\"version\":\"%s\",\"type\":\"data_config\",\"data\":{\"bindings\":[",
                      ROBOT_PROTOCOL_VERSION);
    if ((offset < 0) || (offset >= (int)sizeof(response)))
    {
        return;
    }
    for (axis = 0; axis < ROBOT_AXIS_COUNT; axis++)
    {
        written = snprintf(&response[offset],
                           sizeof(response) - (size_t)offset,
                           "%s[%u,%u,%u,%u]",
                           (axis == 0) ? "" : ",",
                           (unsigned int)bindings[axis].valid,
                           (unsigned int)bindings[axis].device,
                           (unsigned int)bindings[axis].channel,
                           (unsigned int)bindings[axis].reverse);
        if ((written < 0) || ((offset + written) >= (int)sizeof(response)))
        {
            return;
        }
        offset += written;
    }
    written = snprintf(&response[offset], sizeof(response) - (size_t)offset, "]}}\n");
    if ((written < 0) || ((offset + written) >= (int)sizeof(response)))
    {
        return;
    }
    length = (uint16_t)(offset + written);
    (void)ControlServer_Send(pcb, response, length);
}

static void ControlServer_SendChipFaults(struct tcp_pcb *pcb)
{
    uint16_t fault0[MAX22216_DEV_COUNT];
    uint16_t fault1[MAX22216_DEV_COUNT];
    char response[CONTROL_SERVER_TX_SIZE];
    int length;
    MAX22216_DeviceId device;

    for (device = MAX22216_DEV_A; device < MAX22216_DEV_COUNT; device++)
    {
        if (Control_GetDeviceFaults(device, &fault0[device], &fault1[device]) == ERROR)
        {
            ControlServer_SendError(pcb, 3, "chip_fault_read_failed");
            return;
        }
    }

    length = snprintf(response,
                      sizeof(response),
                      "{\"version\":\"%s\",\"type\":\"data_chip_faults\",\"data\":"
                      "{\"fault0\":[%u,%u,%u],\"fault1\":[%u,%u,%u]}}\n",
                      ROBOT_PROTOCOL_VERSION,
                      fault0[0], fault0[1], fault0[2],
                      fault1[0], fault1[1], fault1[2]);
    if ((length > 0) && (length < (int)sizeof(response)))
    {
        (void)ControlServer_Send(pcb, response, (uint16_t)length);
    }
}

static void ControlServer_SendState(struct tcp_pcb *pcb)
{
    static const char *mode_names[] = {"safe", "config", "drive"};
    static const Robot_AxisId axes[] =
    {
        ROBOT_AXIS_LEFT_TRACK,
        ROBOT_AXIS_RIGHT_TRACK,
        ROBOT_AXIS_BODY_ROTATE,
        ROBOT_AXIS_LEFT_LEG,
        ROBOT_AXIS_RIGHT_LEG,
        ROBOT_AXIS_DRILL_ROTATION
    };
    int16_t values[6] = {0};
    uint16_t currents[6] = {0};
    uint8_t faults[6] = {0};
    char response[CONTROL_SERVER_TX_SIZE];
    int length;
    uint8_t index;

    for (index = 0U; index < 6U; index++)
    {
        (void)Control_GetAxis(axes[index], &values[index]);
        (void)Control_GetAxisActualCurrent(axes[index], &currents[index]);
        (void)Control_GetAxisFault(axes[index], &faults[index]);
    }

    length = snprintf(response,
                      sizeof(response),
                      "{\"version\":\"%s\",\"type\":\"data_status\",\"data\":{"
                      "\"mode\":\"%s\",\"connected\":true,\"handshake\":%s,"
                      "\"operator_suspended\":%s,\"jog_active\":%s,"
                      "\"axes\":[%d,%d,%d,%d,%d,%d],"
                      "\"currents_ma\":[%u,%u,%u,%u,%u,%u],"
                      "\"axis_faults\":[%u,%u,%u,%u,%u,%u],"
                      "\"lights\":[%u,%u],\"throttle_enabled\":%u,"
                      "\"throttle_duty_permille\":%u}}\n",
                      ROBOT_PROTOCOL_VERSION,
                      mode_names[control_server_mode],
                      control_server_handshake_complete ? "true" : "false",
                      Control_IsOperatorSuspended() ? "true" : "false",
                      Control_IsJogActive() ? "true" : "false",
                      values[0], values[1], values[2], values[3], values[4], values[5],
                      currents[0], currents[1], currents[2],
                      currents[3], currents[4], currents[5],
                      faults[0], faults[1], faults[2], faults[3], faults[4], faults[5],
                      (unsigned int)Control_GetVehicleLightState(0U),
                      (unsigned int)Control_GetVehicleLightState(1U),
                      (unsigned int)Control_IsThrottleEnabled(),
                      (unsigned int)Control_GetThrottleDutyPermille());
    if ((length > 0) && (length < (int)sizeof(response)))
    {
        (void)ControlServer_Send(pcb, response, (uint16_t)length);
    }
}
