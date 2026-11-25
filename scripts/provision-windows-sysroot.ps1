# Share the runner's explicitly selected Windows toolset with Zig's build rules.
$ErrorActionPreference = 'Stop'
if ($env:STFC_WINDOWS_TOOLCHAIN_URL) {
  python scripts/ci-build-retry.py --prefetch -- zig build fetch-toolchains
  exit $LASTEXITCODE
}

# Zig's native discovery reads the default toolset even inside a pinned developer shell.
$root = Join-Path $env:RUNNER_TEMP 'stfc-msvc-sysroot'
$sdkVersion = $env:WindowsSDKVersion.TrimEnd('\', '/')
New-Item -ItemType Directory -Path $root, "$root/msvc", "$root/winsdk" | Out-Null
$links = @{
  'msvc/include' = Join-Path $env:VCToolsInstallDir 'include'
  'msvc/lib' = Join-Path $env:VCToolsInstallDir 'lib'
  'winsdk/include' = Join-Path $env:WindowsSdkDir "Include/$sdkVersion"
  'winsdk/lib' = Join-Path $env:WindowsSdkDir "Lib/$sdkVersion"
}
foreach ($link in $links.GetEnumerator()) {
  New-Item -ItemType Junction -Path (Join-Path $root $link.Key) -Target $link.Value | Out-Null
}
"path=$root" >> $env:GITHUB_OUTPUT
