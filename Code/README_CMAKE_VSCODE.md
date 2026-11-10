# STM32F407VE Firmware Build

This firmware uses CMake, Ninja, GNU Arm Embedded Toolchain, STM32 StdPeriph,
and lwIP. Run the PowerShell scripts from this `Code` directory.

## Build

```powershell
.\scripts\check_env.ps1
.\scripts\build.ps1
```

The default is `robot-android`. Select another network profile with:

```powershell
.\scripts\build.ps1 -Mode robot-pc
```

Hardware test targets are available for vehicle lights, LED blink, and throttle
PWM:

```powershell
.\scripts\build.ps1 -Mode test-vehicle-light
.\scripts\build.ps1 -Mode test-led-blink
.\scripts\build.ps1 -Mode test-throttle-pwm
```

Build output is written under `Code/build/current/`, separate from any older
copied build cache.

## Flash and Debug

```powershell
.\scripts\flash.ps1 -Mode robot-android
.\scripts\debug_server.ps1
.\scripts\gdb_smoke.ps1
```

DAPLink OpenOCD configurations are in `debug/`. VS Code CMake presets are in
`CMakePresets.json`; the default editor preset targets Android robot firmware.

## Network Control

The robot controller connects to `192.168.1.50:9090` over TCP. Messages use
newline-delimited JSON. Handshake, command fields, heartbeat timing, and
disconnect-stop behavior are documented in `../Docs/Android长连接协议.md`.
