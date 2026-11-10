# Network Layer

The network layer combines the LAN8720 Ethernet MAC/PHY interface with lwIP.
It is shared by the robot controller and the optional hardware-test firmware.

- `network.c`: lwIP initialization, Ethernet polling, link state, and uptime.
- `ethernetif.c`: frame exchange between the LAN8720 driver and lwIP.
- `net_config.h`: static address and PC/Android build profiles.
- `lwip/`: vendored lwIP source and headers.

The robot firmware starts a raw-API TCP server on port `9090`; HTTP/Web server
applications have been removed. The expected board address is `192.168.1.50`.
See `../../Docs/网络连接说明.md` and `../../Docs/Android长连接协议.md`.

The firmware entry loop is:

```text
main -> App_Init() -> App_Poll()
```
