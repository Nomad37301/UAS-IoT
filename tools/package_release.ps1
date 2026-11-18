$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $taskRoot
try {
    New-Item -ItemType Directory -Force dist | Out-Null
    Compress-Archive -LiteralPath 'firmware/libraries/AetherCore' -DestinationPath 'dist/AetherCore-1.0.0.zip' -Force
    $taskStage = Join-Path $taskRoot ('.build/release-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $taskStage | Out-Null
    $taskCandidates = @()
    foreach ($taskDirectory in @('firmware','server','tests','tools','docs')) {
        $taskCandidates += Get-ChildItem -LiteralPath (Join-Path $taskRoot $taskDirectory) -File -Recurse
    }
    $taskCandidates += Get-ChildItem -LiteralPath $taskRoot -File
    $taskFiles = $taskCandidates | Where-Object {
        $taskRelative = [IO.Path]::GetRelativePath($taskRoot, $_.FullName)
        ($taskRelative -match '^(firmware|server|tests|tools|docs)[\\/]' -or
         ($taskRelative -notmatch '[\\/]' -and ($_.Extension -eq '.md' -or $_.Name -eq '.gitignore'))) -and
        $taskRelative -notmatch '(^|[\\/])(\.build|\.git|__pycache__|\.pytest_cache|data|dist|\.venv)([\\/]|$)' -and
        $_.Name -ne 'Secrets.h' -and ($_.Name -notlike '.env*' -or $_.Name -eq '.env.example') -and
        $_.Extension -notin @('.sqlite','.pyc','.pem','.key')
    }
    foreach ($taskFile in $taskFiles) {
        $taskRelative = [IO.Path]::GetRelativePath($taskRoot, $taskFile.FullName)
        $taskDestination = Join-Path $taskStage $taskRelative
        New-Item -ItemType Directory -Force (Split-Path -Parent $taskDestination) | Out-Null
        Copy-Item -LiteralPath $taskFile.FullName -Destination $taskDestination
    }
    Compress-Archive -Path (Join-Path $taskStage '*') -DestinationPath 'dist/AETHER-source-1.0.0.zip' -Force
    Get-ChildItem dist | Select-Object Name,Length
} finally { Pop-Location }
