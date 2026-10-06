[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    & clang++ -std=c++23 -Imods/src tests/incoming_player_attack_test.cc -o build/incoming_player_attack_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Incoming player attack test compilation failed.' }
    & ./build/incoming_player_attack_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Incoming player attack regression failed.' }
} finally { Pop-Location }
