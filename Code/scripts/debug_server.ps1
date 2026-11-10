param(
    [ValidateSet("soft", "srst")]
    [string]$Reset = "soft"
)

$ErrorActionPreference = "Stop"

. "$PSScriptRoot\env.ps1"

if ($Reset -eq "srst") {
    openocd -f "$PSScriptRoot\..\debug\openocd_stm32f407_daplink_srst.cfg"
} else {
    openocd -f "$PSScriptRoot\..\debug\openocd_stm32f407_daplink.cfg"
}
