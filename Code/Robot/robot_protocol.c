#include "robot_protocol.h"

#include <stdio.h>
#include <string.h>

#define RP_TOKEN_COUNT  80U

typedef enum
{
    RP_TOKEN_OBJECT,
    RP_TOKEN_ARRAY,
    RP_TOKEN_STRING,
    RP_TOKEN_PRIMITIVE
} RP_TokenType;

typedef struct
{
    RP_TokenType type;
    uint16_t start;
    uint16_t end;
    uint16_t next;
    int16_t parent;
} RP_Token;

typedef struct
{
    const char *json;
    uint16_t length;
    uint16_t position;
    uint16_t token_count;
    RP_Token tokens[RP_TOKEN_COUNT];
} RP_Parser;

static void RP_SkipWhitespace(RP_Parser *parser);
static int RP_ParseValue(RP_Parser *parser, int16_t parent);
static int RP_ParseString(RP_Parser *parser, int16_t parent);
static int RP_ParseObject(RP_Parser *parser, int16_t parent);
static int RP_ParseArray(RP_Parser *parser, int16_t parent);
static int RP_ParsePrimitive(RP_Parser *parser, int16_t parent);
static int RP_FindProperty(const RP_Parser *parser, int object_token, const char *key);
static uint8_t RP_CopyString(const RP_Parser *parser, int token, char *out, uint16_t out_size);
static uint8_t RP_ReadInt(const RP_Parser *parser,
                          int object_token,
                          const char *key,
                          int32_t *value,
                          uint8_t required);
static uint8_t RP_ParseBindings(const RP_Parser *parser,
                                int array_token,
                                Control_Binding bindings[ROBOT_AXIS_COUNT]);
static RobotProtocol_MessageType RP_LookupType(const char *name);
static uint8_t RP_TokenEquals(const RP_Parser *parser, int token, const char *text);
static uint8_t RP_TokenToInt(const RP_Parser *parser, int token, int32_t *value);
static uint8_t RP_IsValidPrimitive(const char *text, uint16_t length);
static uint16_t RP_Build(char *buffer, uint16_t buffer_length, const char *format, ...);

#include <stdarg.h>

RobotProtocol_MessageType RobotProtocol_ParseLine(const char *line,
                                                  uint16_t length,
                                                  RobotProtocol_Message *message)
{
    RP_Parser parser;
    int root;
    int version_token;
    int type_token;
    int data_token;
    int token;

    if ((line == 0) || (message == 0) ||
        (length == 0U) || (length > ROBOT_PROTOCOL_MAX_LINE))
    {
        return RP_MSG_INVALID;
    }

    (void)memset(message, 0, sizeof(*message));
    message->current_ma = 1500;
    message->aux_current_ma = 1500;
    (void)memset(&parser, 0, sizeof(parser));
    parser.json = line;
    parser.length = length;

    root = RP_ParseValue(&parser, -1);
    RP_SkipWhitespace(&parser);
    if ((root < 0) || (parser.position != parser.length) ||
        (parser.tokens[root].type != RP_TOKEN_OBJECT))
    {
        return RP_MSG_INVALID;
    }

    version_token = RP_FindProperty(&parser, root, "version");
    type_token = RP_FindProperty(&parser, root, "type");
    data_token = RP_FindProperty(&parser, root, "data");
    if (!RP_TokenEquals(&parser, version_token, ROBOT_PROTOCOL_VERSION) ||
        !RP_CopyString(&parser, type_token, message->type_name,
                       sizeof(message->type_name)) ||
        (data_token < 0) || (parser.tokens[data_token].type != RP_TOKEN_OBJECT))
    {
        return RP_MSG_INVALID;
    }

    message->type = RP_LookupType(message->type_name);
    if (message->type == RP_MSG_INVALID)
    {
        return RP_MSG_INVALID;
    }

    switch (message->type)
    {
        case RP_MSG_HELLO_ACK:
            if (!RP_TokenEquals(&parser,
                                RP_FindProperty(&parser, data_token, "protocol_version"),
                                ROBOT_PROTOCOL_VERSION))
            {
                return RP_MSG_INVALID;
            }
            break;

        case RP_MSG_PING:
        case RP_MSG_STOP:
        case RP_MSG_RESTART:
        case RP_MSG_CONFIG_GET:
        case RP_MSG_CONFIG_SAVE:
        case RP_MSG_CONFIG_LOAD:
        case RP_MSG_CONFIG_DEFAULT:
        case RP_MSG_JOG_STOP:
        case RP_MSG_CHIP_FAULTS_GET:
        case RP_MSG_STATE_GET:
            break;

        case RP_MSG_MODE:
            if (!RP_CopyString(&parser,
                               RP_FindProperty(&parser, data_token, "mode"),
                               message->mode,
                               sizeof(message->mode)) ||
                ((strcmp(message->mode, "safe") != 0) &&
                 (strcmp(message->mode, "config") != 0) &&
                 (strcmp(message->mode, "drive") != 0)))
            {
                return RP_MSG_INVALID;
            }
            break;

        case RP_MSG_MOTION:
            if (!RP_ReadInt(&parser, data_token, "turn", &message->turn, 1U) ||
                !RP_ReadInt(&parser, data_token, "forward", &message->forward, 1U) ||
                !RP_ReadInt(&parser, data_token, "body_rotate", &message->body_rotate, 0U) ||
                !RP_ReadInt(&parser, data_token, "left_leg", &message->left_leg, 0U) ||
                !RP_ReadInt(&parser, data_token, "right_leg", &message->right_leg, 0U) ||
                !RP_ReadInt(&parser, data_token, "drill", &message->drill, 0U) ||
                !RP_ReadInt(&parser, data_token, "current_ma", &message->current_ma, 0U) ||
                !RP_ReadInt(&parser, data_token, "aux_current_ma", &message->aux_current_ma, 0U))
            {
                return RP_MSG_INVALID;
            }
            if ((message->turn < -100) || (message->turn > 100) ||
                (message->forward < -100) || (message->forward > 100) ||
                (message->body_rotate < -100) || (message->body_rotate > 100) ||
                (message->left_leg < -100) || (message->left_leg > 100) ||
                (message->right_leg < -100) || (message->right_leg > 100) ||
                (message->drill < -100) || (message->drill > 100) ||
                (message->current_ma < 100) || (message->current_ma > 3000) ||
                (message->aux_current_ma < 100) || (message->aux_current_ma > 3000))
            {
                return RP_MSG_INVALID;
            }
            break;

        case RP_MSG_LIGHT:
            if (!RP_ReadInt(&parser, data_token, "light", &message->light, 1U) ||
                !RP_ReadInt(&parser, data_token, "enabled", &message->enabled, 1U) ||
                (message->light < 0) || (message->light >= 2) ||
                (message->enabled < 0) || (message->enabled > 1))
            {
                return RP_MSG_INVALID;
            }
            break;

        case RP_MSG_THROTTLE:
            if (!RP_ReadInt(&parser, data_token, "enabled", &message->enabled, 1U) ||
                !RP_ReadInt(&parser, data_token, "duty_permille", &message->duty_permille, 1U) ||
                (message->enabled < 0) || (message->enabled > 1) ||
                (message->duty_permille < 0) || (message->duty_permille > 1000))
            {
                return RP_MSG_INVALID;
            }
            break;

        case RP_MSG_CONFIG_SET:
            token = RP_FindProperty(&parser, data_token, "bindings");
            if ((token < 0) ||
                !RP_ParseBindings(&parser, token, message->bindings))
            {
                return RP_MSG_INVALID;
            }
            break;

        case RP_MSG_JOG:
            if (!RP_ReadInt(&parser, data_token, "device", &message->device, 1U) ||
                !RP_ReadInt(&parser, data_token, "channel", &message->channel, 1U) ||
                !RP_ReadInt(&parser, data_token, "reverse", &message->reverse, 1U) ||
                !RP_ReadInt(&parser, data_token, "current_ma", &message->current_ma, 1U) ||
                (message->device < 0) || (message->device >= 3) ||
                (message->channel < 0) || (message->channel >= 4) ||
                (message->reverse < 0) || (message->reverse > 1) ||
                (message->current_ma < 100) || (message->current_ma > 800))
            {
                return RP_MSG_INVALID;
            }
            break;

        case RP_MSG_CHIP_FAULT_CLEAR:
            if (!RP_ReadInt(&parser, data_token, "device", &message->device, 1U) ||
                (message->device < 0) || (message->device >= 3))
            {
                return RP_MSG_INVALID;
            }
            break;

        default:
            return RP_MSG_INVALID;
    }

    return message->type;
}

static int RP_ParseValue(RP_Parser *parser, int16_t parent)
{
    RP_SkipWhitespace(parser);
    if (parser->position >= parser->length)
    {
        return -1;
    }

    switch (parser->json[parser->position])
    {
        case '{': return RP_ParseObject(parser, parent);
        case '[': return RP_ParseArray(parser, parent);
        case '"': return RP_ParseString(parser, parent);
        default:  return RP_ParsePrimitive(parser, parent);
    }
}

static int RP_ParseString(RP_Parser *parser, int16_t parent)
{
    uint16_t start;
    int token;

    if ((parser->position >= parser->length) ||
        (parser->json[parser->position] != '"') ||
        (parser->token_count >= RP_TOKEN_COUNT))
    {
        return -1;
    }

    start = parser->position++;
    while (parser->position < parser->length)
    {
        char c = parser->json[parser->position++];
        if (c == '"')
        {
            token = (int)parser->token_count++;
            parser->tokens[token].type = RP_TOKEN_STRING;
            parser->tokens[token].start = start;
            parser->tokens[token].end = parser->position;
            parser->tokens[token].next = parser->token_count;
            parser->tokens[token].parent = parent;
            return token;
        }
        if ((uint8_t)c < 0x20U)
        {
            return -1;
        }
        if (c == '\\')
        {
            if (parser->position >= parser->length)
            {
                return -1;
            }
            char escaped = parser->json[parser->position++];
            if ((escaped != '"') && (escaped != '\\') && (escaped != '/') &&
                (escaped != 'b') && (escaped != 'f') && (escaped != 'n') &&
                (escaped != 'r') && (escaped != 't'))
            {
                if ((escaped != 'u') || (parser->position + 4U > parser->length))
                {
                    return -1;
                }
                for (uint8_t digit = 0U; digit < 4U; digit++)
                {
                    char hex = parser->json[parser->position + digit];
                    if (!(((hex >= '0') && (hex <= '9')) ||
                          ((hex >= 'a') && (hex <= 'f')) ||
                          ((hex >= 'A') && (hex <= 'F'))))
                    {
                        return -1;
                    }
                }
                parser->position = (uint16_t)(parser->position + 4U);
            }
        }
    }
    return -1;
}

static int RP_ParseObject(RP_Parser *parser, int16_t parent)
{
    int token;
    int child;
    uint8_t closed = 0U;

    if (parser->token_count >= RP_TOKEN_COUNT)
    {
        return -1;
    }
    token = (int)parser->token_count++;
    parser->tokens[token].type = RP_TOKEN_OBJECT;
    parser->tokens[token].start = parser->position++;
    parser->tokens[token].parent = parent;

    RP_SkipWhitespace(parser);
    if ((parser->position < parser->length) &&
        (parser->json[parser->position] == '}'))
    {
        parser->position++;
        closed = 1U;
    }
    else
    {
        while (parser->position < parser->length)
        {
            RP_SkipWhitespace(parser);
            child = RP_ParseString(parser, (int16_t)token);
            if (child < 0)
            {
                return -1;
            }
            RP_SkipWhitespace(parser);
            if ((parser->position >= parser->length) ||
                (parser->json[parser->position++] != ':'))
            {
                return -1;
            }
            if (RP_ParseValue(parser, (int16_t)token) < 0)
            {
                return -1;
            }
            RP_SkipWhitespace(parser);
            if ((parser->position < parser->length) &&
                (parser->json[parser->position] == '}'))
            {
                parser->position++;
                closed = 1U;
                break;
            }
            if ((parser->position >= parser->length) ||
                (parser->json[parser->position++] != ','))
            {
                return -1;
            }
        }
    }

    if (closed == 0U)
    {
        return -1;
    }
    parser->tokens[token].end = parser->position;
    parser->tokens[token].next = parser->token_count;
    return token;
}

static int RP_ParseArray(RP_Parser *parser, int16_t parent)
{
    int token;
    uint8_t closed = 0U;

    if (parser->token_count >= RP_TOKEN_COUNT)
    {
        return -1;
    }
    token = (int)parser->token_count++;
    parser->tokens[token].type = RP_TOKEN_ARRAY;
    parser->tokens[token].start = parser->position++;
    parser->tokens[token].parent = parent;

    RP_SkipWhitespace(parser);
    if ((parser->position < parser->length) &&
        (parser->json[parser->position] == ']'))
    {
        parser->position++;
        closed = 1U;
    }
    else
    {
        while (parser->position < parser->length)
        {
            if (RP_ParseValue(parser, (int16_t)token) < 0)
            {
                return -1;
            }
            RP_SkipWhitespace(parser);
            if ((parser->position < parser->length) &&
                (parser->json[parser->position] == ']'))
            {
                parser->position++;
                closed = 1U;
                break;
            }
            if ((parser->position >= parser->length) ||
                (parser->json[parser->position++] != ','))
            {
                return -1;
            }
        }
    }

    if (closed == 0U)
    {
        return -1;
    }
    parser->tokens[token].end = parser->position;
    parser->tokens[token].next = parser->token_count;
    return token;
}

static int RP_ParsePrimitive(RP_Parser *parser, int16_t parent)
{
    uint16_t start = parser->position;
    int token;

    while ((parser->position < parser->length) &&
           (parser->json[parser->position] != ',') &&
           (parser->json[parser->position] != ']') &&
           (parser->json[parser->position] != '}') &&
           (parser->json[parser->position] != ' ') &&
           (parser->json[parser->position] != '\t') &&
           (parser->json[parser->position] != '\r') &&
           (parser->json[parser->position] != '\n'))
    {
        parser->position++;
    }
    if ((parser->position == start) || (parser->token_count >= RP_TOKEN_COUNT))
    {
        return -1;
    }
    if (!RP_IsValidPrimitive(parser->json + start,
                             (uint16_t)(parser->position - start)))
    {
        return -1;
    }

    token = (int)parser->token_count++;
    parser->tokens[token].type = RP_TOKEN_PRIMITIVE;
    parser->tokens[token].start = start;
    parser->tokens[token].end = parser->position;
    parser->tokens[token].next = parser->token_count;
    parser->tokens[token].parent = parent;
    return token;
}

static void RP_SkipWhitespace(RP_Parser *parser)
{
    while ((parser->position < parser->length) &&
           ((parser->json[parser->position] == ' ') ||
            (parser->json[parser->position] == '\t') ||
            (parser->json[parser->position] == '\r') ||
            (parser->json[parser->position] == '\n')))
    {
        parser->position++;
    }
}

static int RP_FindProperty(const RP_Parser *parser, int object_token, const char *key)
{
    int token;
    uint16_t key_length;
    uint16_t token_length;

    if ((object_token < 0) ||
        (parser->tokens[object_token].type != RP_TOKEN_OBJECT))
    {
        return -1;
    }

    key_length = (uint16_t)strlen(key);
    token = object_token + 1;
    while ((token + 1) < parser->tokens[object_token].next)
    {
        token_length = (uint16_t)(parser->tokens[token].end -
                                  parser->tokens[token].start - 2U);
        if ((parser->tokens[token].type == RP_TOKEN_STRING) &&
            (token_length == key_length) &&
            (memcmp(parser->json + parser->tokens[token].start + 1U,
                    key,
                    key_length) == 0))
        {
            return token + 1;
        }
        token = parser->tokens[token + 1].next;
    }
    return -1;
}

static uint8_t RP_CopyString(const RP_Parser *parser,
                             int token,
                             char *out,
                             uint16_t out_size)
{
    uint16_t length;

    if ((token < 0) || (out == 0) || (out_size == 0U) ||
        (parser->tokens[token].type != RP_TOKEN_STRING))
    {
        return 0U;
    }
    length = (uint16_t)(parser->tokens[token].end -
                        parser->tokens[token].start - 2U);
    if (length >= out_size)
    {
        return 0U;
    }
    (void)memcpy(out,
                 parser->json + parser->tokens[token].start + 1U,
                 length);
    out[length] = '\0';
    return 1U;
}

static uint8_t RP_ReadInt(const RP_Parser *parser,
                          int object_token,
                          const char *key,
                          int32_t *value,
                          uint8_t required)
{
    int token = RP_FindProperty(parser, object_token, key);
    if (token < 0)
    {
        return (required == 0U) ? 1U : 0U;
    }
    return RP_TokenToInt(parser, token, value);
}

static uint8_t RP_ParseBindings(const RP_Parser *parser,
                                int array_token,
                                Control_Binding bindings[ROBOT_AXIS_COUNT])
{
    int row;
    int value_token;
    int32_t values[4];
    int field;

    if ((array_token < 0) || (parser->tokens[array_token].type != RP_TOKEN_ARRAY))
    {
        return 0U;
    }

    row = array_token + 1;
    for (int axis = 0; axis < ROBOT_AXIS_COUNT; axis++)
    {
        if ((row >= parser->tokens[array_token].next) ||
            (parser->tokens[row].type != RP_TOKEN_ARRAY))
        {
            return 0U;
        }
        value_token = row + 1;
        for (field = 0; field < 4; field++)
        {
            if ((value_token >= parser->tokens[row].next) ||
                !RP_TokenToInt(parser, value_token, &values[field]))
            {
                return 0U;
            }
            value_token = parser->tokens[value_token].next;
        }
        if ((value_token != parser->tokens[row].next) ||
            (values[0] < 0) || (values[0] > 1) ||
            (values[1] < 0) || (values[1] > 2) ||
            (values[2] < 0) || (values[2] > 3) ||
            (values[3] < 0) || (values[3] > 1))
        {
            return 0U;
        }
        bindings[axis].valid = (uint8_t)values[0];
        bindings[axis].device = (MAX22216_DeviceId)values[1];
        bindings[axis].channel = (uint8_t)values[2];
        bindings[axis].reverse = (uint8_t)values[3];
        row = parser->tokens[row].next;
    }
    return (row == parser->tokens[array_token].next) ? 1U : 0U;
}

static RobotProtocol_MessageType RP_LookupType(const char *name)
{
    static const struct
    {
        const char *name;
        RobotProtocol_MessageType type;
    } entries[] =
    {
        {"hello_ack", RP_MSG_HELLO_ACK},
        {"ping", RP_MSG_PING},
        {"cmd_mode", RP_MSG_MODE},
        {"cmd_motion", RP_MSG_MOTION},
        {"cmd_light", RP_MSG_LIGHT},
        {"cmd_throttle", RP_MSG_THROTTLE},
        {"cmd_stop", RP_MSG_STOP},
        {"cmd_restart", RP_MSG_RESTART},
        {"cmd_config_get", RP_MSG_CONFIG_GET},
        {"cmd_config_set", RP_MSG_CONFIG_SET},
        {"cmd_config_save", RP_MSG_CONFIG_SAVE},
        {"cmd_config_load", RP_MSG_CONFIG_LOAD},
        {"cmd_config_default", RP_MSG_CONFIG_DEFAULT},
        {"cmd_jog", RP_MSG_JOG},
        {"cmd_jog_stop", RP_MSG_JOG_STOP},
        {"cmd_chip_faults_get", RP_MSG_CHIP_FAULTS_GET},
        {"cmd_chip_fault_clear", RP_MSG_CHIP_FAULT_CLEAR},
        {"cmd_state_get", RP_MSG_STATE_GET}
    };

    for (uint16_t index = 0U; index < (sizeof(entries) / sizeof(entries[0])); index++)
    {
        if (strcmp(name, entries[index].name) == 0)
        {
            return entries[index].type;
        }
    }
    return RP_MSG_INVALID;
}

static uint8_t RP_TokenEquals(const RP_Parser *parser, int token, const char *text)
{
    uint16_t length;
    if ((token < 0) || (parser->tokens[token].type != RP_TOKEN_STRING))
    {
        return 0U;
    }
    length = (uint16_t)(parser->tokens[token].end -
                        parser->tokens[token].start - 2U);
    return ((length == strlen(text)) &&
            (memcmp(parser->json + parser->tokens[token].start + 1U,
                    text,
                    length) == 0)) ? 1U : 0U;
}

static uint8_t RP_TokenToInt(const RP_Parser *parser, int token, int32_t *value)
{
    uint16_t position;
    uint16_t end;
    uint32_t parsed = 0U;
    uint32_t limit;
    uint8_t negative = 0U;

    if ((token < 0) || (value == 0) ||
        (parser->tokens[token].type != RP_TOKEN_PRIMITIVE))
    {
        return 0U;
    }
    position = parser->tokens[token].start;
    end = parser->tokens[token].end;
    if (position < end && parser->json[position] == '-')
    {
        negative = 1U;
        position++;
    }
    if (position >= end)
    {
        return 0U;
    }
    limit = negative ? 2147483648UL : 2147483647UL;
    while (position < end)
    {
        char c = parser->json[position++];
        if ((c < '0') || (c > '9') ||
            (parsed > (limit - (uint32_t)(c - '0')) / 10U))
        {
            return 0U;
        }
        parsed = parsed * 10U + (uint32_t)(c - '0');
    }
    if (negative != 0U)
    {
        *value = (parsed == 2147483648UL) ? INT32_MIN : -(int32_t)parsed;
    }
    else
    {
        *value = (int32_t)parsed;
    }
    return 1U;
}

static uint8_t RP_IsValidPrimitive(const char *text, uint16_t length)
{
    uint16_t position = 0U;

    if ((length == 4U) &&
        ((memcmp(text, "true", 4U) == 0) || (memcmp(text, "null", 4U) == 0)))
    {
        return 1U;
    }
    if ((length == 5U) && (memcmp(text, "false", 5U) == 0))
    {
        return 1U;
    }

    if ((position < length) && (text[position] == '-'))
    {
        position++;
    }
    if (position >= length)
    {
        return 0U;
    }
    if (text[position] == '0')
    {
        position++;
        if ((position < length) && (text[position] >= '0') && (text[position] <= '9'))
        {
            return 0U;
        }
    }
    else if ((text[position] >= '1') && (text[position] <= '9'))
    {
        do
        {
            position++;
        } while ((position < length) && (text[position] >= '0') && (text[position] <= '9'));
    }
    else
    {
        return 0U;
    }
    if ((position < length) && (text[position] == '.'))
    {
        position++;
        if ((position >= length) || (text[position] < '0') || (text[position] > '9'))
        {
            return 0U;
        }
        do
        {
            position++;
        } while ((position < length) && (text[position] >= '0') && (text[position] <= '9'));
    }
    if ((position < length) && ((text[position] == 'e') || (text[position] == 'E')))
    {
        position++;
        if ((position < length) && ((text[position] == '+') || (text[position] == '-')))
        {
            position++;
        }
        if ((position >= length) || (text[position] < '0') || (text[position] > '9'))
        {
            return 0U;
        }
        do
        {
            position++;
        } while ((position < length) && (text[position] >= '0') && (text[position] <= '9'));
    }
    return (position == length) ? 1U : 0U;
}

uint16_t RobotProtocol_BuildHello(char *buffer,
                                  uint16_t buffer_length,
                                  const char *device_id,
                                  const char *firmware_version)
{
    return RP_Build(buffer, buffer_length,
                    "{\"version\":\"%s\",\"type\":\"hello\",\"data\":"
                    "{\"device_id\":\"%s\",\"firmware_version\":\"%s\","
                    "\"protocol_version\":\"%s\",\"status\":\"ready\"}}\n",
                    ROBOT_PROTOCOL_VERSION, device_id, firmware_version,
                    ROBOT_PROTOCOL_VERSION);
}

uint16_t RobotProtocol_BuildPong(char *buffer, uint16_t buffer_length)
{
    return RP_Build(buffer, buffer_length,
                    "{\"version\":\"%s\",\"type\":\"pong\",\"data\":{}}\n",
                    ROBOT_PROTOCOL_VERSION);
}

uint16_t RobotProtocol_BuildAck(char *buffer,
                                uint16_t buffer_length,
                                const char *original_type,
                                uint8_t success)
{
    return RP_Build(buffer, buffer_length,
                    "{\"version\":\"%s\",\"type\":\"ack\",\"data\":"
                    "{\"original_type\":\"%s\",\"success\":%s}}\n",
                    ROBOT_PROTOCOL_VERSION, original_type,
                    success ? "true" : "false");
}

uint16_t RobotProtocol_BuildError(char *buffer,
                                  uint16_t buffer_length,
                                  int32_t code,
                                  const char *message)
{
    return RP_Build(buffer, buffer_length,
                    "{\"version\":\"%s\",\"type\":\"error\",\"data\":"
                    "{\"code\":%ld,\"message\":\"%s\"}}\n",
                    ROBOT_PROTOCOL_VERSION, (long)code, message);
}

static uint16_t RP_Build(char *buffer, uint16_t buffer_length, const char *format, ...)
{
    int length;
    va_list args;

    if ((buffer == 0) || (buffer_length == 0U))
    {
        return 0U;
    }
    va_start(args, format);
    length = vsnprintf(buffer, buffer_length, format, args);
    va_end(args);
    if ((length < 0) || ((uint32_t)length >= buffer_length))
    {
        return 0U;
    }
    return (uint16_t)length;
}
