$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$head = (& git -C $repositoryRoot rev-parse HEAD).Trim().ToLowerInvariant()
$worktreeStatus = (& git -C $repositoryRoot status --porcelain=v1 --untracked-files=all | Out-String).Trim()
if ($worktreeStatus) {
  throw "Runtime identity build-matrix tests require a clean checkout.`n$worktreeStatus"
}
$badCommit = "0000000000000000000000000000000000000000"
$probePath = Join-Path $repositoryRoot "runtime_identity_dirty_probe.untracked"
$submoduleProbePath = Join-Path $repositoryRoot "macos-launcher/deps/PLzmaSDK/runtime_identity_ci_probe.untracked"
$headerPath = Join-Path $repositoryRoot "build/.gens/mods/windows/x64/release/rules/runtime_identity/runtime_identity.generated.h"
$baseArguments = @(
  "f", "-p", "windows", "-m", "release", "-y",
  "--stfc_build_class=development",
  "--stfc_test_target=",
  "--stfc_test_expiry=",
  "--stfc_support_boundary="
)

function Invoke-Configure {
  param([string[]] $IdentityArguments)

  Push-Location $repositoryRoot
  try {
    $arguments = $baseArguments + $IdentityArguments
    $output = & xmake @arguments 2>&1 | Out-String
    return @{
      ExitCode = $LASTEXITCODE
      Output = $output
    }
  }
  finally {
    Pop-Location
  }
}

function Assert-ConfigureFails {
  param(
    [string] $Name,
    [string] $ExpectedMessage,
    [string[]] $IdentityArguments
  )

  $result = Invoke-Configure $IdentityArguments
  if ($result.ExitCode -eq 0) {
    throw "$Name unexpectedly succeeded.`n$($result.Output)"
  }
  if (-not $result.Output.Contains($ExpectedMessage)) {
    throw "$Name failed without the expected message '$ExpectedMessage'.`n$($result.Output)"
  }
  Write-Output "PASS: $Name"
}

function Assert-ConfigureSucceeds {
  param(
    [string] $Name,
    [string[]] $IdentityArguments
  )

  $result = Invoke-Configure $IdentityArguments
  if ($result.ExitCode -ne 0) {
    throw "$Name failed.`n$($result.Output)"
  }
  Write-Output "PASS: $Name"
}

function Assert-GeneratedHeaderContains {
  param([string[]] $ExpectedLines)

  if (-not (Test-Path -LiteralPath $headerPath)) {
    throw "Generated runtime identity header not found: $headerPath"
  }
  $header = Get-Content -LiteralPath $headerPath -Raw
  foreach ($line in $ExpectedLines) {
    if (-not $header.Contains($line)) {
      throw "Generated runtime identity header is missing: $line"
    }
  }
}

Assert-ConfigureFails `
  -Name "mismatched base commit" `
  -ExpectedMessage "stfc_base_commit must match checked-out HEAD" `
  -IdentityArguments @("--stfc_source_state_id=", "--stfc_base_commit=$badCommit")

Assert-ConfigureFails `
  -Name "mismatched clean source identity" `
  -ExpectedMessage "git source identity must match checked-out HEAD" `
  -IdentityArguments @("--stfc_source_state_id=git:$badCommit", "--stfc_base_commit=$head")

Assert-ConfigureFails `
  -Name "malformed source identity" `
  -ExpectedMessage "source identity must be git:<40-hex-sha> or dirty-sha256:<64-hex-hash>" `
  -IdentityArguments @("--stfc_source_state_id=arbitrary", "--stfc_base_commit=$head")

try {
  [System.IO.File]::WriteAllText($submoduleProbePath, "submodule build-output probe")
  Assert-ConfigureSucceeds `
    -Name "dirty submodule worktree does not alter source identity" `
    -IdentityArguments @("--stfc_source_state_id=git:$head", "--stfc_base_commit=$head")
}
finally {
  Remove-Item -LiteralPath $submoduleProbePath -Force -ErrorAction SilentlyContinue
}

try {
  [System.IO.File]::WriteAllText($probePath, "dirty source identity probe")
  Assert-ConfigureFails `
    -Name "false clean identity for dirty checkout" `
    -ExpectedMessage "a dirty Git checkout cannot claim a clean git source identity" `
    -IdentityArguments @("--stfc_source_state_id=git:$head", "--stfc_base_commit=$head")

  Assert-ConfigureSucceeds `
    -Name "derived dirty source identity" `
    -IdentityArguments @("--stfc_source_state_id=", "--stfc_base_commit=")
  Assert-GeneratedHeaderContains @(
    '#define STFC_SOURCE_STATE_ID "dirty-sha256:',
    '#define STFC_SOURCE_REPRODUCIBLE_STR "false"',
    '#define STFC_SOURCE_REPRODUCIBLE 0'
  )
}
finally {
  Remove-Item -LiteralPath $probePath -Force -ErrorAction SilentlyContinue
}

Assert-ConfigureSucceeds `
  -Name "matching clean source identity" `
  -IdentityArguments @("--stfc_source_state_id=git:$head", "--stfc_base_commit=$head")
Assert-GeneratedHeaderContains @(
  "#define STFC_SOURCE_STATE_ID `"git:$head`"",
  "#define STFC_BASE_COMMIT `"$head`"",
  '#define STFC_SOURCE_REPRODUCIBLE_STR "true"',
  '#define STFC_SOURCE_REPRODUCIBLE 1'
)
