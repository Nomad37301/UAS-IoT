param([string]$ArduinoCli = 'arduino-cli', [string]$ConfigFile = '', [string]$ExtraLibraries = '')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $taskRoot
try {
    $taskCommon = @()
    if ($ConfigFile) { $taskCommon += @('--config-file',$ConfigFile) }
    $taskOptions = @('--library','firmware/libraries/AetherCore','--warnings','all')
    if ($ExtraLibraries) { $taskOptions += @('--libraries',$ExtraLibraries) }
    & $ArduinoCli @taskCommon compile --fqbn 'esp32:esp32:esp32s3:CDCOnBoot=default' @taskOptions --build-path .build/final-node-b firmware/node_b/AetherNodeB
    if ($LASTEXITCODE -ne 0) { throw 'Node B build failed' }
    & $ArduinoCli @taskCommon compile --fqbn 'esp32:esp32:esp32s3:CDCOnBoot=default' @taskOptions --build-path .build/final-node-b-internal firmware/node_b/AetherNodeB_InternalFlash
    if ($LASTEXITCODE -ne 0) { throw 'Node B internal flash build failed' }
    & $ArduinoCli @taskCommon compile --fqbn 'esp32:esp32:esp32doit-devkit-v1' @taskOptions --build-path .build/final-node-c firmware/node_c/AetherNodeC
    if ($LASTEXITCODE -ne 0) { throw 'Node C build failed' }
} finally { Pop-Location }
