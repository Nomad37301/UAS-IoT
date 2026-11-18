$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $taskRoot
try {
    New-Item -ItemType Directory -Force .build | Out-Null
    $taskCore = 'firmware/libraries/AetherCore/src'
    $taskCases = @(
        @{ Name='gy906'; Test='tests/gy906/test_mlx90614.cpp'; Fake='tests/gy906/fakes'; Sources=@('Mlx90614.cpp') },
        @{ Name='voltage'; Test='tests/voltage_adc/test_voltage_sensor.cpp'; Fake='tests/voltage_adc/fakes'; Sources=@('VoltageSensor.cpp') },
        @{ Name='buzzer'; Test='tests/active_buzzer/test_active_buzzer.cpp'; Fake='tests/active_buzzer/fakes'; Sources=@('ActiveBuzzer.cpp') },
        @{ Name='button'; Test='tests/boot_mute/test_boot_mute.cpp'; Fake='tests/boot_mute/fakes'; Sources=@('DebouncedButton.cpp','ActiveBuzzer.cpp') },
        @{ Name='rgb'; Test='tests/rgb_status/test_status_rgb.cpp'; Fake='tests/rgb_status/fakes'; Sources=@('StatusRgb.cpp') },
        @{ Name='relay'; Test='tests/recovery_relay/test_recovery_relay.cpp'; Fake='tests/recovery_relay/fakes'; Sources=@('RecoveryRelay.cpp') },
        @{ Name='modbus'; Test='tests/modbus_master/test_modbus_master.cpp'; Fake='tests/modbus_master/fakes'; Sources=@('ModbusRtuMaster.cpp') },
        @{ Name='radio'; Test='tests/nrf24_radio/test_transport.cpp'; Fake='tests/nrf24_radio/fakes'; Sources=@('NodeBRadio.cpp') },
        @{ Name='packet'; Test='tests/nrf24_radio/test_packet.cpp'; Fake='tests/nrf24_radio/fakes'; Sources=@('NodeBRadio.cpp') },
        @{ Name='blackbox'; Test='tests/littlefs_blackbox/test_blackbox.cpp'; Fake='tests/littlefs_blackbox/fakes'; Sources=@('BlackboxLogger.cpp') },
        @{ Name='core'; Test='tests/integration/test_core.cpp'; Fake=''; Sources=@() }
    )
    foreach ($taskCase in $taskCases) {
        $taskBinary = ".build/check-$($taskCase.Name).exe"
        $taskArguments = @('-std=c++11','-Wall','-Wextra','-Werror')
        if ($taskCase.Fake) { $taskArguments += @('-I',$taskCase.Fake) }
        $taskArguments += @('-I',$taskCore,$taskCase.Test)
        foreach ($taskSource in $taskCase.Sources) { $taskArguments += "$taskCore/$taskSource" }
        $taskArguments += @('-o',$taskBinary)
        & g++ @taskArguments
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $($taskCase.Name)" }
        & "./$taskBinary"
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $($taskCase.Name)" }
    }
    python tests/littlefs_blackbox/verify_json.py
    if ($LASTEXITCODE -ne 0) { throw 'JSON verification failed' }
    Write-Output 'PASS: all 11 host firmware test binaries use the final shared library'
} finally { Pop-Location }
