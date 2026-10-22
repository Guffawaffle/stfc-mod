# Standalone production-algorithm fixture; does not load or modify STFC.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    & clang++ -std=c++23 -Imods/src tests/object_tracking_test.cc -o build/object_tracking_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Object tracking test compilation failed.' }
    & ./build/object_tracking_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Object tracking regression failed.' }
} finally { Pop-Location }
