param(
    [ValidateSet("test-vehicle-light", "test-led-blink", "test-throttle-pwm", "robot", "robot-pc", "robot-android")]
    [string]$Mode = "robot-android"
)

$ErrorActionPreference = "Stop"

. "$PSScriptRoot\env.ps1"

Push-Location (Join-Path $PSScriptRoot "..")
try {
    $preset = "gcc-arm-" + $Mode
    cmake --preset $preset
    cmake --build --preset $preset
} finally {
    Pop-Location
}
