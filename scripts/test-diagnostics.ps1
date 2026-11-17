param(
    [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$ZigExecutable,
    [string]$MsvcSysroot,
    [string]$VsDevShell,
    [string]$JsonInclude,
    [string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Windows)) {
    throw 'This local runner requires Windows x64; the capture sources remain portable.'
}
$source = (Resolve-Path -LiteralPath $SourceRoot).Path
if (-not $ZigExecutable) { $ZigExecutable = (Get-Command zig -ErrorAction Stop).Source }
$zig = (Resolve-Path -LiteralPath $ZigExecutable).Path
$version = (& $zig version).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^0\.(16|17)\.') { throw 'Use Zig 0.16 or 0.17.' }

if (-not $JsonInclude) {
    $manifest = Get-Content -LiteralPath (Join-Path $source 'build.zig.zon') -Raw
    if ($manifest -notmatch '(?s)\.nlohmann_json\s*=\s*\.\{.*?\.hash\s*=\s*"([^"]+)"') {
        throw 'Cannot locate the pinned nlohmann_json dependency; supply -JsonInclude.'
    }
    $packageHash = $Matches[1]
    $candidates = @(Join-Path $source "zig-pkg/$packageHash/single_include")
    if ($env:ZIG_GLOBAL_CACHE_DIR) { $candidates += Join-Path $env:ZIG_GLOBAL_CACHE_DIR "p/$packageHash/single_include" }
    if ($env:LOCALAPPDATA) { $candidates += Join-Path $env:LOCALAPPDATA "zig/p/$packageHash/single_include" }
    $JsonInclude = $candidates | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'nlohmann/json.hpp') -PathType Leaf } | Select-Object -First 1
}
if (-not $JsonInclude -or -not (Test-Path -LiteralPath (Join-Path $JsonInclude 'nlohmann/json.hpp') -PathType Leaf)) {
    throw 'Supply -JsonInclude pointing to an existing directory containing nlohmann/json.hpp.'
}
$JsonInclude = (Resolve-Path -LiteralPath $JsonInclude).Path

if (-not $OutputDirectory) {
    $runName = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
    $OutputDirectory = Join-Path $source ".zig-cache/diagnostics-tests/$runName"
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$fixtureRoot = Join-Path $output 'fixtures'
[IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
$executable = Join-Path $output 'diagnostics-capture-tests.exe'
$compileLog = Join-Path $output 'compile.log'
$runLog = Join-Path $output 'run.log'
$compileArgs = @('build-exe', '-target', 'x86_64-windows-msvc', '-ODebug', '-fno-sanitize-c', '--subsystem', 'console', '-fentry=mainCRTStartup',
    '--cache-dir', (Join-Path $output 'cache'), '--global-cache-dir', (Join-Path $source '.zig-cache/diagnostics-global-cache'),
    '-I', (Join-Path $source 'src/mod'), '-isystem', $JsonInclude)

if ($MsvcSysroot) {
    $sdk = (Resolve-Path -LiteralPath $MsvcSysroot).Path.Replace('\', '/')
    foreach ($relative in @('msvc/include', 'winsdk/include/ucrt', 'msvc/lib/x64', 'winsdk/lib/ucrt/x64', 'winsdk/lib/um/x64')) {
        if (-not (Test-Path -LiteralPath (Join-Path $sdk $relative) -PathType Container)) { throw "Incomplete MSVC sysroot: $relative" }
    }
    $libc = Join-Path $output 'msvc-libc.txt'
    $libcLines = @("include_dir=$sdk/winsdk/include/ucrt", "sys_include_dir=$sdk/msvc/include", "crt_dir=$sdk/winsdk/lib/ucrt/x64",
        "msvc_lib_dir=$sdk/msvc/lib/x64", "kernel32_lib_dir=$sdk/winsdk/lib/um/x64", 'gcc_dir=', 'darwin_sdk_dir=')
    [IO.File]::WriteAllText($libc, ($libcLines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    foreach ($include in @('um', 'shared', 'winrt', 'cppwinrt')) { $compileArgs += @('-isystem', "$sdk/winsdk/include/$include") }
    foreach ($library in @('winsdk/lib/um/x64', 'winsdk/lib/ucrt/x64', 'msvc/lib/x64')) { $compileArgs += @('-L', "$sdk/$library") }
} else {
    if ($VsDevShell -or -not $env:INCLUDE -or -not $env:LIB) {
        if (-not $VsDevShell) {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
            if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'Open an x64 Visual Studio developer shell, or supply -VsDevShell or -MsvcSysroot.' }
            $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'No Visual Studio C++ installation found; supply -VsDevShell or -MsvcSysroot.' }
            $VsDevShell = Join-Path $installation 'Common7/Tools/Launch-VsDevShell.ps1'
        }
        & $VsDevShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
    }
    $includes = @($env:INCLUDE -split ';' | Where-Object { $_ })
    $libraries = @($env:LIB -split ';' | Where-Object { $_ })
    foreach ($include in $includes) { $compileArgs += @('-isystem', $include) }
    foreach ($library in $libraries) { $compileArgs += @('-L', $library) }
    # Bind CRT discovery to the same SDK as the explicit include and library search paths.
    $vcInclude = $includes | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'vcruntime.h') } | Select-Object -First 1
    $ucrtInclude = $includes | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'corecrt.h') } | Select-Object -First 1
    $vcLibrary = $libraries | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'libcmt.lib') } | Select-Object -First 1
    $ucrtLibrary = $libraries | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'libucrt.lib') } | Select-Object -First 1
    $kernelLibrary = $libraries | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'kernel32.lib') } | Select-Object -First 1
    if (-not $vcInclude -or -not $ucrtInclude -or -not $vcLibrary -or -not $ucrtLibrary -or -not $kernelLibrary) {
        throw 'The selected developer shell lacks an x64 MSVC/Windows SDK; supply -MsvcSysroot.'
    }
    $libc = Join-Path $output 'msvc-libc.txt'
    $libcLines = @("include_dir=$ucrtInclude", "sys_include_dir=$vcInclude", "crt_dir=$ucrtLibrary",
        "msvc_lib_dir=$vcLibrary", "kernel32_lib_dir=$kernelLibrary", 'gcc_dir=', 'darwin_sdk_dir=')
    [IO.File]::WriteAllText($libc, ($libcLines -join "`n").Replace('\', '/') + "`n", [Text.UTF8Encoding]::new($false))
}

$sources = @('tests/diagnostics_capture_test.cc', 'src/mod/diagnostics/capture.cc', 'src/mod/diagnostics/storage.cc')
$compileArgs += @('--libc', $libc, '-cflags', '-std=c++23', '-UNDEBUG', '-fms-runtime-lib=static', '-Wno-unused-command-line-argument', '--')
foreach ($relative in $sources) {
    $path = Join-Path $source $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing fixture source: $relative" }
    $compileArgs += $path
}
$compileArgs += @('-llibcmt', '-llibcpmt', '-llibucrt', '-llibvcruntime', '-lkernel32', "-femit-bin=$executable")
$started = [DateTime]::UtcNow
& $zig @compileArgs *> $compileLog
$compileExit = $LASTEXITCODE
$runExit = $null
if ($compileExit -eq 0) {
    & $executable $fixtureRoot *> $runLog
    $runExit = $LASTEXITCODE
}
$receipt = [ordered]@{ sourceRoot = $source; zigExecutable = $zig; zigVersion = $version; arguments = $compileArgs;
    libcFile = $libc;
    compileExit = $compileExit; runExit = $runExit; startedUtc = $started.ToString('o'); finishedUtc = [DateTime]::UtcNow.ToString('o');
    fixtureRoot = $fixtureRoot; compileLog = $compileLog; runLog = $runLog; gameUsed = $false; deployed = $false }
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'receipt.json') -Encoding utf8
if ($compileExit -ne 0) { Get-Content -LiteralPath $compileLog -Tail 60; throw "Diagnostics fixture compilation failed; see $compileLog" }
Get-Content -LiteralPath $runLog
if ($runExit -ne 0) { throw "Diagnostics fixture failed; see $runLog" }
Write-Output "Diagnostics capture fixtures passed. Receipt: $(Join-Path $output 'receipt.json')"
