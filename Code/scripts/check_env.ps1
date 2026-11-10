$ErrorActionPreference = "Stop"

. "$PSScriptRoot\env.ps1"

$tools = @(
    "cmake",
    "ninja",
    "arm-none-eabi-gcc",
    "arm-none-eabi-gdb",
    "arm-none-eabi-objcopy",
    "arm-none-eabi-size",
    "openocd"
)

$missing = @()
foreach ($tool in $tools) {
    $cmd = Get-Command $tool -ErrorAction SilentlyContinue
    if ($null -eq $cmd) {
        Write-Host "[missing] $tool"
        $missing += $tool
    } else {
        Write-Host "[ok]      $tool -> $($cmd.Source)"
    }
}

if ($missing.Count -ne 0) {
    Write-Host ""
    Write-Host "Missing tools: $($missing -join ', ')"
    Write-Host "Install CMake, Ninja, GNU Arm Embedded Toolchain, and OpenOCD, then make sure they are in PATH."
    exit 1
}

Write-Host ""
Write-Host "Embedded toolchain looks ready."
