[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Force build/settings-test | Out-Null
    foreach ($fixture in @('boolean_settings', 'boolean_view', 'native_boolean_callback', 'page_catalog', 'page_sections', 'choice_setting', 'slider_setting', 'shortcut_editor')) {
        & clang++ --driver-mode=cl /std:c++latest /EHsc /MT /Imods/src /Ithird_party/libil2cpp `
            "tests/${fixture}_test.cc" "/Febuild/settings-test/$fixture.exe" /Fobuild/settings-test/
        if ($LASTEXITCODE -ne 0) { throw "Settings fixture compilation failed: $fixture" }
        & "./build/settings-test/$fixture.exe"
        if ($LASTEXITCODE -ne 0) { throw "Settings fixture failed: $fixture" }
    }
    & clang++ --driver-mode=cl /std:c++latest /EHsc /MT /Imods/src `
        tests/fleet_arrival_tracker.cc /Febuild/settings-test/fleet_arrival_tracker.exe /Fobuild/settings-test/
    if ($LASTEXITCODE -ne 0) { throw 'Fleet arrival fixture compilation failed' }
    & ./build/settings-test/fleet_arrival_tracker.exe
    if ($LASTEXITCODE -ne 0) { throw 'Fleet arrival fixture failed' }
} finally {
    Pop-Location
}
