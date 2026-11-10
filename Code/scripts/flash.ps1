param(
    [ValidateSet("test-vehicle-light", "test-led-blink", "test-throttle-pwm", "robot", "robot-pc", "robot-android")]
    [string]$Mode = "robot-android",
    [ValidateSet("soft", "srst", "none")]
    [string]$Reset = "soft"
)

$ErrorActionPreference = "Stop"

. "$PSScriptRoot\env.ps1"

$elf = Join-Path $PSScriptRoot ("..\build\current\gcc-arm-" + $Mode + "\rock_burst.elf")
if (-not (Test-Path $elf)) {
    throw "Firmware ELF not found: $elf. Run Code\scripts\build.ps1 -Mode $Mode first."
}
$elfOpenocd = (Resolve-Path $elf).Path -replace "\\", "/"

$softCfg = "$PSScriptRoot\..\debug\openocd_stm32f407_daplink.cfg"
$srstCfg = "$PSScriptRoot\..\debug\openocd_stm32f407_daplink_srst.cfg"

if ($Reset -eq "srst") {
    openocd -f $srstCfg -c "program {$elfOpenocd} verify reset exit"
} elseif ($Reset -eq "none") {
    openocd -f $softCfg -c "init" -c "halt" -c "program {$elfOpenocd} verify" -c "resume 0x08000000" -c "shutdown"
} else {
    openocd -f $softCfg -c "init" -c "reset halt" -c "program {$elfOpenocd} verify" -c "reset run" -c "shutdown"
}
