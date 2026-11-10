# Robot TCP Control Protocol

## Transport

- Board address: `192.168.1.50`
- TCP port: `9090`
- Encoding: UTF-8 JSON (current command values are ASCII)
- Framing: one JSON object per line, terminated by `\n` (CRLF is also accepted)
- Envelope: `{"version":"1.0","type":"...","data":{...}}`
- One active client is supported. A new client replaces the old connection and
  stops all outputs.

Every complete line must fit within 384 bytes. The server accepts fragmented
TCP data and reassembles it by newline.

## Handshake and Safety Lease

The board sends this message immediately after accepting a TCP connection:

```json
{"version":"1.0","type":"hello","data":{"device_id":"rockburst","firmware_version":"1.0.0","protocol_version":"1.0","status":"ready"}}
```

The controller must respond with `hello_ack` before sending control commands:

```json
{"version":"1.0","type":"hello_ack","data":{"app_version":"1.0","protocol_version":"1.0"}}
```

After the handshake, send `ping` periodically and expect `pong`:

```json
{"version":"1.0","type":"ping","data":{}}
```

The controller must send a valid message at least once per second. Use a
250-500 ms ping interval to leave scheduling and network margin. If no valid
message arrives for 1000 ms, if TCP closes/errors, or if the Ethernet link goes
down, the server closes the session and calls `Control_StopAll()`. This switches
the robot to `safe`, turns off motion, jog, lights, and throttle. Malformed
messages do not renew the lease.

## Modes

Send `cmd_mode` with one of `safe`, `config`, or `drive`:

```json
{"version":"1.0","type":"cmd_mode","data":{"mode":"drive"}}
```

- `safe`: stop all outputs.
- `config`: stop outputs; enables binding configuration and physical-channel jog.
- `drive`: stop existing outputs, then enables motion, lights, and throttle.

`cmd_stop` immediately enters `safe`. `cmd_restart` restarts the operator
controller and is accepted only in `drive`.

## Drive Commands

`cmd_motion` updates the button-style operator command. Axis values are in
`-100..100`; omitted auxiliary axes default to zero. Current limits are in mA.

```json
{"version":"1.0","type":"cmd_motion","data":{"turn":0,"forward":60,"body_rotate":0,"left_leg":0,"right_leg":0,"drill":0,"current_ma":1500,"aux_current_ma":1500}}
```

`turn` and `forward` are required. `body_rotate`, `left_leg`, `right_leg`,
`drill`, `current_ma`, and `aux_current_ma` are optional. The current limits
are `100..3000` mA. `cmd_motion` is accepted only in `drive`.

Lights and throttle are also accepted only in `drive`:

```json
{"version":"1.0","type":"cmd_light","data":{"light":0,"enabled":1}}
{"version":"1.0","type":"cmd_throttle","data":{"enabled":1,"duty_permille":500}}
```

`light` is `0` or `1`; `enabled` is `0` or `1`; `duty_permille` is `0..1000`.

## Configuration and Diagnostics

Configuration, jog, chip-fault, and state commands:

```json
{"version":"1.0","type":"cmd_config_get","data":{}}
{"version":"1.0","type":"cmd_config_set","data":{"bindings":[[1,0,0,0],[1,1,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]}}
{"version":"1.0","type":"cmd_config_save","data":{}}
{"version":"1.0","type":"cmd_config_load","data":{}}
{"version":"1.0","type":"cmd_config_default","data":{}}
{"version":"1.0","type":"cmd_jog","data":{"device":0,"channel":0,"reverse":0,"current_ma":300}}
{"version":"1.0","type":"cmd_jog_stop","data":{}}
{"version":"1.0","type":"cmd_chip_faults_get","data":{}}
{"version":"1.0","type":"cmd_chip_fault_clear","data":{"device":0}}
{"version":"1.0","type":"cmd_state_get","data":{}}
```

`bindings` must contain 10 rows in `Robot_AxisId` order. Each row is
`[valid,device,channel,reverse]`; `valid` and `reverse` are `0|1`, `device` is
`0..2` (A/B/C), and `channel` is `0..3`. The existing binding manager performs
whole-table validation before applying the table.

Jog is available only in `config`; its device is `0..2`, channel is `0..3`,
reverse is `0|1`, and current is `100..800` mA. Jog has a 300 ms hardware
control timeout, so a controller using jog must refresh it while held.

Configuration and diagnostic requests return `data_config`,
`data_chip_faults`, or `data_status` messages as applicable. Commands also
receive an `ack` with `original_type` and a boolean `success`. Invalid input
receives an `error` message.
