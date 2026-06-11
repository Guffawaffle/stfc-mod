[CmdletBinding()]
param(
  [int]$Tail = 80,
  [switch]$SkipPureTests
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Resolve-FirstCommand {
  param([string[]]$Names)

  foreach ($name in $Names) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) {
      return $cmd.Source
    }
  }

  throw "None of these commands were found: $($Names -join ', ')"
}

function ConvertTo-LineTail {
  param(
    [string]$Text,
    [int]$Count = 40
  )

  if ([string]::IsNullOrWhiteSpace($Text)) {
    return @()
  }

  $lines = $Text -split "`r?`n" | Where-Object { $_ -ne "" }
  if ($lines.Count -le $Count) {
    return @($lines)
  }

  return @($lines | Select-Object -Last $Count)
}

function Invoke-Captured {
  param(
    [string]$FilePath,
    [string[]]$Arguments
  )

  $psi = [System.Diagnostics.ProcessStartInfo]::new()
  $psi.FileName = $FilePath
  $psi.WorkingDirectory = $repoRoot.Path
  $psi.UseShellExecute = $false
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true

  foreach ($arg in $Arguments) {
    [void]$psi.ArgumentList.Add($arg)
  }

  $process = [System.Diagnostics.Process]::new()
  $process.StartInfo = $psi
  [void]$process.Start()
  $stdout = $process.StandardOutput.ReadToEnd()
  $stderr = $process.StandardError.ReadToEnd()
  $process.WaitForExit()

  return [ordered]@{
    exitCode = $process.ExitCode
    stdout = $stdout
    stderr = $stderr
    stdoutTail = ConvertTo-LineTail $stdout
    stderrTail = ConvertTo-LineTail $stderr
  }
}

function Try-ParseJson {
  param([string]$Text)

  if ([string]::IsNullOrWhiteSpace($Text)) {
    return $null
  }

  try {
    return $Text | ConvertFrom-Json -Depth 20
  } catch {
    return $null
  }
}

$steps = [ordered]@{}

try {
  $python = Resolve-FirstCommand @("py", "python3", "python")
  $scanner = Invoke-Captured $python @("scripts\scan_gameplay_seams.py", "--format", "json")
  $scannerData = Try-ParseJson $scanner.stdout
  $steps.scanner = [ordered]@{
    ok = ($scanner.exitCode -eq 0)
    command = "$python scripts\scan_gameplay_seams.py --format json"
    exitCode = $scanner.exitCode
    data = $scannerData
    stdoutTail = $scanner.stdoutTail
    stderrTail = $scanner.stderrTail
  }

  $git = Resolve-FirstCommand @("git")
  $diff = Invoke-Captured $git @("diff", "--check")
  $steps.diffCheck = [ordered]@{
    ok = ($diff.exitCode -eq 0)
    command = "git diff --check"
    exitCode = $diff.exitCode
    stdoutTail = $diff.stdoutTail
    stderrTail = $diff.stderrTail
  }

  $cachedDiff = Invoke-Captured $git @("diff", "--cached", "--check")
  $steps.cachedDiffCheck = [ordered]@{
    ok = ($cachedDiff.exitCode -eq 0)
    command = "git diff --cached --check"
    exitCode = $cachedDiff.exitCode
    stdoutTail = $cachedDiff.stdoutTail
    stderrTail = $cachedDiff.stderrTail
  }

  if ($SkipPureTests) {
    $steps.pureTests = [ordered]@{
      ok = $true
      skipped = $true
      reason = "SkipPureTests requested"
    }
  } else {
    $pwsh = Resolve-FirstCommand @("pwsh", "powershell")
    $ax = Join-Path $repoRoot.Path ".ax\ax.ps1"
    $pure = Invoke-Captured $pwsh @(
      "-NoLogo",
      "-NoProfile",
      "-ExecutionPolicy",
      "Bypass",
      "-File",
      $ax,
      "pure-tests",
      "-Tail",
      [string]$Tail
    )
    $pureData = Try-ParseJson $pure.stdout
    $pureOk = ($pure.exitCode -eq 0) -and ($pureData -ne $null) -and ($pureData.ok -eq $true)
    $steps.pureTests = [ordered]@{
      ok = $pureOk
      command = "$pwsh -File .ax\ax.ps1 pure-tests -Tail $Tail"
      exitCode = $pure.exitCode
      data = $pureData
      stdoutTail = $pure.stdoutTail
      stderrTail = $pure.stderrTail
    }
  }

  $ok = $true
  foreach ($step in $steps.Values) {
    if (-not $step.ok) {
      $ok = $false
      break
    }
  }

  $payload = [ordered]@{
    ok = $ok
    contract = "stfc-mod.review-contract"
    blocking = $true
    advisory = $false
    repoRoot = $repoRoot.Path
    steps = $steps
  }

  $payload | ConvertTo-Json -Depth 30
  if ($ok) {
    exit 0
  }
  exit 1
} catch {
  $payload = [ordered]@{
    ok = $false
    contract = "stfc-mod.review-contract"
    blocking = $true
    advisory = $false
    repoRoot = $repoRoot.Path
    error = $_.Exception.Message
    steps = $steps
  }
  $payload | ConvertTo-Json -Depth 30
  exit 1
}
