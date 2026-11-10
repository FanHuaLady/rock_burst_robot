$ErrorActionPreference = "Stop"

. "$PSScriptRoot\env.ps1"

$elf = Join-Path $PSScriptRoot "..\build\gcc-arm-debug\rock_burst.elf"
if (-not (Test-Path $elf)) {
    throw "Firmware ELF not found: $elf. Run Code\scripts\build.ps1 first."
}

arm-none-eabi-gdb $elf -x "$PSScriptRoot\..\debug\gdb_network_snapshot.gdb"
