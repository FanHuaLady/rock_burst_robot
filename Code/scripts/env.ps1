$localToolBins = @(
    "D:\MyCmake\bin",
    "D:\MyArmGunToolchainArmNoneEabi\bin",
    "D:\MyNinja",
    "D:\MyOpenocd\bin"
)

foreach ($toolBin in $localToolBins) {
    if (-not (Test-Path $toolBin)) {
        continue
    }

    $exists = $false
    foreach ($pathPart in ($env:Path -split ';')) {
        if ($pathPart.TrimEnd('\') -ieq $toolBin.TrimEnd('\')) {
            $exists = $true
            break
        }
    }

    if (-not $exists) {
        $env:Path = "$toolBin;$env:Path"
    }
}
