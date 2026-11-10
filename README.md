# RockBurst Robot Firmware

本目录是 RockBurst 机器人工程，包含 STM32F407 固件、硬件资料和网络控制协议。
机器人通过 lwIP TCP 服务接收上位机的 NDJSON 控制消息；当前不包含遥控器，也不再包含旧 Web 控制程序。

## 构建

需要 CMake、Ninja 和 GNU Arm Embedded Toolchain。命令在 `Code` 目录执行：

```powershell
.\scripts\build.ps1
```

默认构建 Android 直连网络配置。电脑直连可使用：

```powershell
.\scripts\build.ps1 -Mode robot-pc
```

构建产物位于 `Code\build\current\gcc-arm-robot-android\` 或对应模式目录。

可选硬件测试固件：

```powershell
.\scripts\build.ps1 -Mode test-vehicle-light
.\scripts\build.ps1 -Mode test-led-blink
.\scripts\build.ps1 -Mode test-throttle-pwm
```

## 控制连接

- 开发板地址：`192.168.1.50`
- TCP 端口：`9090`
- 消息格式：每行一个 `{"version":"1.0","type":"...","data":{...}}` JSON 对象
- 上位机握手后需每 250-500 ms 发送 `ping`
- 连接断开、网线断开或 1 秒无有效消息时，固件全停并切换到 `safe`

协议字段与命令示例见 [Docs/Android长连接协议.md](Docs/Android长连接协议.md)。
网络配置见 [Docs/网络连接说明.md](Docs/网络连接说明.md)。

## 目录

- `Code/Robot`：机器人应用、TCP 控制服务和协议解析
- `Code/Control`、`Code/Device`：机器人控制逻辑与驱动抽象
- `Code/Hardware`：底层外设驱动，保留完整
- `Code/Network`：lwIP、以太网适配和网络配置
- `Code/Test`：车灯、LED、油门等独立硬件测试固件
- `PCB`、`Docs`、`Quotes`：硬件设计、技术资料和采购记录
