[CmdletBinding()]
param(
  [string]$GamePath = 'C:\Games\Star Trek Fleet Command\default\game',
  [string]$UnityVersion,
  [string]$OutputRoot,
  [string]$DotNetPath,
  [switch]$FailOnActionableChanges
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
  $OutputRoot = Join-Path $repoRoot '.cache\protobuf-refresh'
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)

$gameAssembly = Join-Path $GamePath 'GameAssembly.dll'
$metadata = Join-Path $GamePath 'prime_Data\il2cpp_data\Metadata\global-metadata.dat'
$unityPlayer = Join-Path $GamePath 'UnityPlayer.dll'
foreach ($required in @($gameAssembly, $metadata, $unityPlayer)) {
  if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
    throw "Missing required game file: $required"
  }
}
if ([string]::IsNullOrWhiteSpace($UnityVersion)) {
  $productVersion = (Get-Item -LiteralPath $unityPlayer).VersionInfo.ProductVersion
  if ($productVersion -notmatch '^(\d+\.\d+\.\d+[abfp]\d+)') {
    throw "Could not derive the Unity version from UnityPlayer.dll: $productVersion"
  }
  $UnityVersion = $matches[1]
}

$protodecCommit = '52ddf82ca90d277e56fe1fb0d27d6766136cf442'
$archiveHash = '750B000ECEEA3D6242A7E6953F6357F7AA7D0B47716305137EFFF3F1A095E33C'
$toolRoot = Join-Path $OutputRoot "protodec-$protodecCommit"
$archivePath = Join-Path $OutputRoot "$protodecCommit.zip"
$sourceRoot = Join-Path $toolRoot "protodec-$protodecCommit"
$patchPath = Join-Path $repoRoot 'tools\protobuf\protodec-stfc.patch'
$candidateRoot = Join-Path $OutputRoot 'candidates'

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
  $archiveUri = "https://github.com/Xpl0itR/Protodec/archive/$protodecCommit.zip"
  Write-Host "Downloading pinned Protodec $protodecCommit"
  Invoke-WebRequest -Uri $archiveUri -OutFile $archivePath
}

$actualArchiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
if ($actualArchiveHash -ne $archiveHash) {
  throw "Protodec archive hash mismatch. Expected $archiveHash, got $actualArchiveHash"
}

$resolvedToolRoot = [System.IO.Path]::GetFullPath($toolRoot)
$outputPrefix = $OutputRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $resolvedToolRoot.StartsWith($outputPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
  throw "Refusing to recreate tool source outside the output root: $resolvedToolRoot"
}
if (Test-Path -LiteralPath $resolvedToolRoot) {
  Remove-Item -LiteralPath $resolvedToolRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $resolvedToolRoot | Out-Null
Expand-Archive -LiteralPath $archivePath -DestinationPath $resolvedToolRoot

$patchHash = (Get-FileHash -LiteralPath $patchPath -Algorithm SHA256).Hash
$previousGitCeiling = $env:GIT_CEILING_DIRECTORIES
$env:GIT_CEILING_DIRECTORIES = $repoRoot
Push-Location $sourceRoot
try {
  & git apply --unsafe-paths --check $patchPath 2>$null
  if ($LASTEXITCODE -ne 0) {
    throw 'The STFC compatibility patch no longer applies cleanly to the pinned Protodec source.'
  }
  & git apply --unsafe-paths --whitespace=nowarn $patchPath
  if ($LASTEXITCODE -ne 0) {
    throw 'Failed to apply the STFC compatibility patch to Protodec.'
  }
} finally {
  Pop-Location
  $env:GIT_CEILING_DIRECTORIES = $previousGitCeiling
}

if ([string]::IsNullOrWhiteSpace($DotNetPath)) {
  $cachedDotNet = Join-Path $repoRoot '.cache\dotnet10\dotnet.exe'
  $DotNetPath = $(if (Test-Path -LiteralPath $cachedDotNet) { $cachedDotNet } else { 'dotnet' })
}
if (Test-Path -LiteralPath $DotNetPath -PathType Leaf) {
  $DotNetPath = (Resolve-Path -LiteralPath $DotNetPath).Path
}

$sdkList = & $DotNetPath --list-sdks
$dotNet10Sdk = $sdkList | ForEach-Object {
  if ($_ -match '^(10\.\d+\.\d+)') { [version]$matches[1] }
} | Sort-Object -Descending | Select-Object -First 1
if ($LASTEXITCODE -ne 0 -or $null -eq $dotNet10Sdk) {
  throw 'Protodec currently requires a .NET 10 SDK. Pass -DotNetPath or install .NET 10.'
}

$toolGlobalJson = [ordered]@{
  sdk = [ordered]@{
    version = $dotNet10Sdk.ToString()
    rollForward = 'latestPatch'
    allowPrerelease = $false
  }
}
$toolGlobalJson | ConvertTo-Json -Depth 3 |
  Set-Content -LiteralPath (Join-Path $sourceRoot 'global.json') -Encoding utf8NoBOM

Write-Host 'Building the pinned STFC-compatible Protodec'
Push-Location $sourceRoot
try {
  & $DotNetPath build 'src\protodec\protodec.csproj' -c Release
  if ($LASTEXITCODE -ne 0) {
    throw 'Protodec build failed.'
  }
} finally {
  Pop-Location
}

$resolvedOutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$resolvedCandidates = [System.IO.Path]::GetFullPath($candidateRoot)
if (-not $resolvedCandidates.StartsWith($resolvedOutputRoot + [System.IO.Path]::DirectorySeparatorChar,
                                        [System.StringComparison]::OrdinalIgnoreCase)) {
  throw "Refusing to replace candidate directory outside output root: $resolvedCandidates"
}
if (Test-Path -LiteralPath $resolvedCandidates) {
  Remove-Item -LiteralPath $resolvedCandidates -Recurse -Force
}
New-Item -ItemType Directory -Path $resolvedCandidates | Out-Null

$protodecDll = Join-Path $sourceRoot 'bin\protodec\Release\net10.0\protodec.dll'
Write-Host 'Extracting protobuf candidates from the installed game'
& $DotNetPath $protodecDll il2cpp $gameAssembly $metadata $UnityVersion $resolvedCandidates --log-level Error
if ($LASTEXITCODE -ne 0) {
  throw 'Protobuf extraction failed.'
}

$comparisonScript = Join-Path $PSScriptRoot 'Compare-ProtobufCorpus.ps1'
& $comparisonScript -CandidatePath $resolvedCandidates `
                    -TrackedPath (Join-Path $repoRoot 'mods\src\prime\proto') `
                    -ReportPath (Join-Path $OutputRoot 'protobuf-refresh-report.json')
if ($LASTEXITCODE -ne 0) {
  throw 'Protobuf corpus comparison failed.'
}

$reportPath = Join-Path $OutputRoot 'protobuf-refresh-report.json'
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json -AsHashtable
$report.source = [ordered]@{
  unityVersion = $UnityVersion
  gameAssemblySha256 = (Get-FileHash -LiteralPath $gameAssembly -Algorithm SHA256).Hash
  globalMetadataSha256 = (Get-FileHash -LiteralPath $metadata -Algorithm SHA256).Hash
  protodecCommit = $protodecCommit
  protodecArchiveSha256 = $archiveHash
  stfcPatchSha256 = $patchHash
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding utf8NoBOM

Write-Host "Candidates: $resolvedCandidates"
Write-Host "Report:     $reportPath"
if ($FailOnActionableChanges -and $report.actionableChangeCount -ne 0) {
  throw "The protobuf refresh found $($report.actionableChangeCount) actionable schema changes."
}
