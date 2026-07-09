[CmdletBinding()]
param(
  [ValidateSet("list", "status", "create", "cleanup")]
  [string]$Action = "list",
  [string]$LeaseId = "",
  [string]$AgentName = "background-agent",
  [string]$Objective = "",
  [string]$Issue = "",
  [string]$BaseRef = "HEAD",
  [string]$BranchName = "",
  [string]$AllowedScope = "",
  [switch]$AllowEdits,
  [switch]$AllowGitWrites,
  [switch]$AllowGithubWrites,
  [switch]$AllowRuntimeActions,
  [switch]$Execute,
  [switch]$Force,
  [string]$WorktreeRoot = "",
  [string]$LeaseStatePath = ".ax/agent-worktrees/leases.jsonl"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$repoParent = Split-Path -Parent $repoRoot.Path
$defaultWorktreeRoot = Join-Path $repoParent "stfc-mod-agent-worktrees"

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

function Split-StatusText {
  param([string]$Text)

  if ([string]::IsNullOrWhiteSpace($Text)) {
    return @()
  }

  return @($Text -split "`r?`n" | Where-Object { $_ -ne "" })
}

function Test-IsBrokerOwnedStatusLine {
  param([string]$Line)

  return $Line -match '^(?:\?\?|!!) AGENT_LEASE\.md$' -or
         $Line -match '^(?:\?\?|!!) \.agent-worktree/AGENT_LEASE\.md$' -or
         $Line -match '^(?:\?\?|!!) \.agent-worktree/?$'
}

function Remove-BrokerOwnedStatusLines {
  param([string[]]$Lines)

  return @($Lines | Where-Object { -not (Test-IsBrokerOwnedStatusLine $_) })
}

function Select-BrokerOwnedStatusLines {
  param([string[]]$Lines)

  return @($Lines | Where-Object { Test-IsBrokerOwnedStatusLine $_ })
}

function Invoke-Captured {
  param(
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$WorkingDirectory = $repoRoot.Path
  )

  $psi = [System.Diagnostics.ProcessStartInfo]::new()
  $psi.FileName = $FilePath
  $psi.WorkingDirectory = $WorkingDirectory
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
    stdout = $stdout.Trim()
    stderr = $stderr.Trim()
    stdoutTail = ConvertTo-LineTail $stdout
    stderrTail = ConvertTo-LineTail $stderr
  }
}

function Add-Problem {
  param(
    [System.Collections.ArrayList]$Target,
    [string]$Code,
    [string]$Message
  )

  [void]$Target.Add([ordered]@{
    code = $Code
    message = $Message
  })
}

function ConvertTo-Slug {
  param([string]$Text)

  $slug = $Text.ToLowerInvariant() -replace '[^a-z0-9]+', '-'
  $slug = $slug.Trim('-')
  if ([string]::IsNullOrWhiteSpace($slug)) {
    return "agent"
  }
  if ($slug.Length -gt 40) {
    return $slug.Substring(0, 40).Trim('-')
  }
  return $slug
}

function Get-WorkspacePathComparison {
  if ([System.IO.Path]::DirectorySeparatorChar -eq '\') {
    return [System.StringComparison]::OrdinalIgnoreCase
  }

  return [System.StringComparison]::Ordinal
}

function Resolve-UnderRoot {
  param(
    [string]$Path,
    [string]$Root,
    [string]$Label
  )

  $comparison = Get-WorkspacePathComparison
  $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd([System.IO.Path]::DirectorySeparatorChar,
                                                          [System.IO.Path]::AltDirectorySeparatorChar)
  $candidate = if ([System.IO.Path]::IsPathRooted($Path)) {
    $Path
  } else {
    Join-Path $rootFull $Path
  }
  $fullPath = [System.IO.Path]::GetFullPath($candidate)
  $prefix = "$rootFull$([System.IO.Path]::DirectorySeparatorChar)"
  if (-not ($fullPath.Equals($rootFull, $comparison) -or
            $fullPath.StartsWith($prefix, $comparison))) {
    throw "$Label must stay within $rootFull`: $Path"
  }

  return $fullPath
}

function Test-IsSameOrUnder {
  param(
    [string]$Path,
    [string]$Root
  )

  $comparison = Get-WorkspacePathComparison
  $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd([System.IO.Path]::DirectorySeparatorChar,
                                                          [System.IO.Path]::AltDirectorySeparatorChar)
  $pathFull = [System.IO.Path]::GetFullPath($Path).TrimEnd([System.IO.Path]::DirectorySeparatorChar,
                                                          [System.IO.Path]::AltDirectorySeparatorChar)
  $prefix = "$rootFull$([System.IO.Path]::DirectorySeparatorChar)"
  return $pathFull.Equals($rootFull, $comparison) -or
         $pathFull.StartsWith($prefix, $comparison)
}

function ConvertTo-ComparablePath {
  param([string]$Path)

  return [System.IO.Path]::GetFullPath($Path).TrimEnd([System.IO.Path]::DirectorySeparatorChar,
                                                      [System.IO.Path]::AltDirectorySeparatorChar)
}

function Resolve-LeaseStatePath {
  param([string]$Path)

  $comparison = Get-WorkspacePathComparison
  $candidate = if ([System.IO.Path]::IsPathRooted($Path)) {
    $Path
  } else {
    Join-Path $repoRoot.Path $Path
  }
  $fullPath = [System.IO.Path]::GetFullPath($candidate)
  $stateRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot.Path ".ax/agent-worktrees")).TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar,
    [System.IO.Path]::AltDirectorySeparatorChar)
  $prefix = "$stateRoot$([System.IO.Path]::DirectorySeparatorChar)"
  if ($fullPath.Equals($stateRoot, $comparison)) {
    throw "LeaseStatePath must be a file under .ax/agent-worktrees: $Path"
  }
  if (-not $fullPath.StartsWith($prefix, $comparison)) {
    throw "LeaseStatePath must stay under .ax/agent-worktrees: $Path"
  }

  return $fullPath
}

function Resolve-LeaseRoot {
  param([object]$Lease)

  $root = if ($Lease.worktreeRoot) {
    $Lease.worktreeRoot
  } else {
    $worktreeRootFullPath
  }
  $rootFull = [System.IO.Path]::GetFullPath($root).TrimEnd([System.IO.Path]::DirectorySeparatorChar,
                                                           [System.IO.Path]::AltDirectorySeparatorChar)
  if (Test-IsSameOrUnder $rootFull $repoRoot.Path) {
    throw "Recorded worktree root must not be inside the primary repository checkout: $rootFull"
  }

  return $rootFull
}

function Remove-BrokerOwnedLeaseFiles {
  param([string]$Path)

  $leaseRootFull = [System.IO.Path]::GetFullPath($Path)
  $removed = @()

  $legacyBrief = Resolve-UnderRoot "AGENT_LEASE.md" $leaseRootFull "Broker lease brief"
  if (Test-Path -LiteralPath $legacyBrief) {
    Remove-Item -LiteralPath $legacyBrief -Force
    $removed += "AGENT_LEASE.md"
  }

  $leaseBrief = Resolve-UnderRoot ".agent-worktree/AGENT_LEASE.md" $leaseRootFull "Broker lease brief"
  if (Test-Path -LiteralPath $leaseBrief) {
    Remove-Item -LiteralPath $leaseBrief -Force
    $removed += ".agent-worktree/AGENT_LEASE.md"
  }

  $leaseBriefDirectory = Resolve-UnderRoot ".agent-worktree" $leaseRootFull "Broker lease brief directory"
  if (Test-Path -LiteralPath $leaseBriefDirectory) {
    $remainingItems = @(Get-ChildItem -LiteralPath $leaseBriefDirectory -Force)
    if ($remainingItems.Count -eq 0) {
      Remove-Item -LiteralPath $leaseBriefDirectory -Force
      $removed += ".agent-worktree/"
    }
  }

  return @($removed)
}

function Get-BrokerLeaseDirectoryExtraItems {
  param([string]$Path)

  $leaseRootFull = [System.IO.Path]::GetFullPath($Path)
  $leaseBriefDirectory = Resolve-UnderRoot ".agent-worktree" $leaseRootFull "Broker lease brief directory"
  if (-not (Test-Path -LiteralPath $leaseBriefDirectory)) {
    return @()
  }

  $extras = @()
  foreach ($item in @(Get-ChildItem -LiteralPath $leaseBriefDirectory -Force -Recurse)) {
    $relative = [System.IO.Path]::GetRelativePath($leaseBriefDirectory, $item.FullName).Replace("\", "/")
    if ($relative -ne "AGENT_LEASE.md") {
      $extras += ".agent-worktree/$relative"
    }
  }

  return @($extras)
}

function Split-ListText {
  param([string]$Text)

  if ([string]::IsNullOrWhiteSpace($Text)) {
    return @()
  }

  return @($Text -split "`r?`n|;" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
}

function Read-LeaseRecords {
  param([string]$Path)

  if (-not (Test-Path -LiteralPath $Path)) {
    return @()
  }

  $records = @()
  foreach ($line in [System.IO.File]::ReadLines($Path)) {
    if ([string]::IsNullOrWhiteSpace($line)) {
      continue
    }
    try {
      $records += ($line | ConvertFrom-Json -Depth 30)
    } catch {
      $records += [pscustomobject]@{
        event = "parse-error"
        raw = $line
      }
    }
  }
  return @($records)
}

function Write-LeaseRecord {
  param(
    [System.Collections.IDictionary]$Record,
    [string]$Path
  )

  $directory = Split-Path -Parent $Path
  if (-not [string]::IsNullOrWhiteSpace($directory)) {
    [void][System.IO.Directory]::CreateDirectory($directory)
  }

  $line = $Record | ConvertTo-Json -Depth 30 -Compress
  [System.IO.File]::AppendAllText($Path, "$line`n", [System.Text.UTF8Encoding]::new($false))
}

function Format-DisplayCommand {
  param(
    [string]$FilePath,
    [string[]]$Arguments
  )

  $parts = @($FilePath) + $Arguments
  return ($parts | ForEach-Object {
    if ($_ -match '^[A-Za-z0-9_./:@=+\-\\]+$') {
      $_
    } else {
      '"' + ($_.Replace('"', '\"')) + '"'
    }
  }) -join " "
}

function Get-LatestLease {
  param(
    [object[]]$Records,
    [string]$Id
  )

  $matches = @($Records | Where-Object { $_.leaseId -eq $Id })
  if ($matches.Count -eq 0) {
    return $null
  }

  return $matches | Select-Object -Last 1
}

function ConvertTo-LeaseSummary {
  param([object[]]$Records)

  $summaries = [ordered]@{}
  foreach ($record in $Records) {
    if ([string]::IsNullOrWhiteSpace($record.leaseId)) {
      continue
    }
    $summaries[$record.leaseId] = $record
  }

  return @($summaries.GetEnumerator() | ForEach-Object {
    $record = $_.Value
    [ordered]@{
      leaseId = $record.leaseId
      event = $record.event
      agentName = $record.agentName
      branchName = $record.branchName
      worktreePath = $record.worktreePath
      baseRef = $record.baseRef
      baseSha = $record.baseSha
      updatedAt = $record.recordedAt
      objective = $record.objective
    }
  })
}

function ConvertFrom-WorktreePorcelain {
  param([string]$Text)

  $items = @()
  $current = [ordered]@{}
  foreach ($line in ($Text -split "`r?`n")) {
    if ([string]::IsNullOrWhiteSpace($line)) {
      if ($current.Count -gt 0) {
        $items += [pscustomobject]$current
        $current = [ordered]@{}
      }
      continue
    }

    $parts = $line -split " ", 2
    $key = $parts[0]
    $value = if ($parts.Count -gt 1) { $parts[1] } else { $true }
    $current[$key] = $value
  }

  if ($current.Count -gt 0) {
    $items += [pscustomobject]$current
  }

  return @($items)
}

function Find-RegisteredWorktree {
  param(
    [object[]]$Worktrees,
    [string]$Path
  )

  $comparison = Get-WorkspacePathComparison
  $target = ConvertTo-ComparablePath $Path
  foreach ($worktree in $Worktrees) {
    if (-not $worktree.worktree) {
      continue
    }
    $candidate = ConvertTo-ComparablePath ([string]$worktree.worktree)
    if ($candidate.Equals($target, $comparison)) {
      return $worktree
    }
  }

  return $null
}

function ConvertTo-WorktreeBranchName {
  param([object]$Worktree)

  if ($null -eq $Worktree -or -not $Worktree.branch) {
    return ""
  }

  $branch = [string]$Worktree.branch
  if ($branch.StartsWith("refs/heads/", [System.StringComparison]::OrdinalIgnoreCase)) {
    return $branch.Substring("refs/heads/".Length)
  }

  return $branch
}

function Test-RegisteredLeaseWorktree {
  param(
    [string]$Path,
    [string]$BranchName
  )

  $worktreeList = Invoke-Captured $git @("worktree", "list", "--porcelain")
  $worktrees = if ($worktreeList.exitCode -eq 0) {
    ConvertFrom-WorktreePorcelain $worktreeList.stdout
  } else {
    @()
  }
  $registered = Find-RegisteredWorktree $worktrees $Path
  $actualBranch = ConvertTo-WorktreeBranchName $registered
  $expectedBranch = [string]$BranchName
  $branchMatches = [string]::IsNullOrWhiteSpace($expectedBranch) -or
                   [string]::Equals($actualBranch, $expectedBranch, [System.StringComparison]::Ordinal)
  $branchOk = ($null -ne $registered) -and
              $branchMatches

  return [ordered]@{
    ok = ($worktreeList.exitCode -eq 0) -and ($null -ne $registered) -and $branchOk
    exitCode = $worktreeList.exitCode
    registered = $null -ne $registered
    path = $Path
    registeredPath = if ($registered) { $registered.worktree } else { $null }
    expectedBranch = if ($expectedBranch) { $expectedBranch } else { $null }
    actualBranch = if ($actualBranch) { $actualBranch } else { $null }
    branchOk = $branchOk
    stderrTail = $worktreeList.stderrTail
  }
}

function Write-LeaseBrief {
  param(
    [string]$Path,
    [System.Collections.IDictionary]$Record
  )

  $scopeText = if ($Record.allowedScope.Count -eq 0) {
    "- none specified"
  } else {
    ($Record.allowedScope | ForEach-Object { "- $_" }) -join "`n"
  }
  $fence = '```'
  $content = @"
# Background Agent Worktree Lease

Lease: $($Record.leaseId)
Agent: $($Record.agentName)
Issue: $($Record.issue)
Branch: $($Record.branchName)
Base: $($Record.baseRef) / $($Record.baseSha)

## Objective

$($Record.objective)

## Allowed Scope

$scopeText

## Permissions

${fence}json
$($Record.permissions | ConvertTo-Json -Depth 10)
${fence}

## Rules

- Keep all work inside this worktree.
- Do not push, tag, release, or mutate GitHub unless the lease explicitly permits it.
- Do not cycle STFC, sidecar, or game/runtime state unless the lease explicitly permits it.
- Leave a concise handoff for the bridge before cleanup.
"@
  $leaseBriefDirectory = Join-Path $Path ".agent-worktree"
  [void][System.IO.Directory]::CreateDirectory($leaseBriefDirectory)
  [System.IO.File]::WriteAllText((Join-Path $leaseBriefDirectory "AGENT_LEASE.md"),
                                 $content,
                                 [System.Text.UTF8Encoding]::new($false))
}

$blockers = [System.Collections.ArrayList]::new()
$warnings = [System.Collections.ArrayList]::new()
$steps = [ordered]@{}
$planned = [System.Collections.ArrayList]::new()
$executed = [System.Collections.ArrayList]::new()
try {
  $git = Resolve-FirstCommand @("git")
  $leaseStateFullPath = Resolve-LeaseStatePath $LeaseStatePath
  $worktreeRootFullPath = [System.IO.Path]::GetFullPath($(if ([string]::IsNullOrWhiteSpace($WorktreeRoot)) {
        $defaultWorktreeRoot
      } elseif ([System.IO.Path]::IsPathRooted($WorktreeRoot)) {
        $WorktreeRoot
      } else {
        Join-Path $repoParent $WorktreeRoot
      })).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)

  if (Test-IsSameOrUnder $worktreeRootFullPath $repoRoot.Path) {
    Add-Problem $blockers "worktree-root-inside-repo" "WorktreeRoot must not be inside the primary repository checkout."
  }

  $records = Read-LeaseRecords $leaseStateFullPath
  $timestamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
  $effectiveWorktreeRoot = $worktreeRootFullPath
  $leaseIdResolved = if ([string]::IsNullOrWhiteSpace($LeaseId)) {
    "run-$((Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss'))-$(ConvertTo-Slug $AgentName)"
  } else {
    ConvertTo-Slug $LeaseId
  }
  $branchNameResolved = if ([string]::IsNullOrWhiteSpace($BranchName)) {
    "agent/$leaseIdResolved"
  } else {
    $BranchName
  }
  $effectiveBranchName = $branchNameResolved
  $leasePath = Resolve-UnderRoot $leaseIdResolved $worktreeRootFullPath "Lease path"
} catch {
  $payload = [ordered]@{
    ok = $false
    command = "agent-worktree"
    action = $Action
    dryRun = -not $Execute
    repoRoot = $repoRoot.Path
    error = $_.Exception.Message
    blockers = @($blockers)
    warnings = @($warnings)
    plannedActions = @($planned)
    executedActions = @($executed)
    steps = $steps
  }
  $payload | ConvertTo-Json -Depth 40
  exit 1
}

try {
  if ($Action -eq "list") {
    $worktreeList = Invoke-Captured $git @("worktree", "list", "--porcelain")
    $steps.gitWorktrees = [ordered]@{
      ok = $worktreeList.exitCode -eq 0
      exitCode = $worktreeList.exitCode
      worktrees = ConvertFrom-WorktreePorcelain $worktreeList.stdout
      stdoutTail = $worktreeList.stdoutTail
      stderrTail = $worktreeList.stderrTail
    }
    if (-not $steps.gitWorktrees.ok) {
      Add-Problem $warnings "git-worktree-list-failed" "Could not read git worktree list."
    }
  } elseif ($Action -eq "status") {
    $lease = Get-LatestLease $records $leaseIdResolved
    if ($null -eq $lease) {
      Add-Problem $blockers "lease-not-found" "Lease $leaseIdResolved was not found in $LeaseStatePath."
    } else {
      if (-not [string]::IsNullOrWhiteSpace([string]$lease.branchName)) {
        $effectiveBranchName = [string]$lease.branchName
      }
      if ($lease.event -eq "cleanup") {
        Add-Problem $warnings "lease-cleaned" "Lease $leaseIdResolved is already recorded as cleaned."
      } elseif ($lease.worktreePath) {
        $leaseRoot = Resolve-LeaseRoot $lease
        $effectiveWorktreeRoot = $leaseRoot
        $leasePath = Resolve-UnderRoot $lease.worktreePath $leaseRoot "Lease path"
        $registration = Test-RegisteredLeaseWorktree $leasePath $lease.branchName
        $steps.registeredWorktree = $registration
        if ($registration.exitCode -ne 0) {
          Add-Problem $blockers "worktree-list-failed" "Could not verify registered git worktrees."
        } elseif (-not $registration.registered) {
          Add-Problem $blockers "worktree-not-registered" "Lease path is not registered as a worktree for this repository."
        } elseif (-not $registration.branchOk) {
          Add-Problem $blockers "worktree-branch-mismatch" "Lease worktree is not on the recorded lease branch."
        } elseif (Test-Path -LiteralPath $leasePath) {
          $status = Invoke-Captured $git @("-C", $leasePath, "status", "--short", "--branch")
          $steps.worktreeStatus = [ordered]@{
            ok = $status.exitCode -eq 0
            exitCode = $status.exitCode
            stdoutTail = $status.stdoutTail
            stderrTail = $status.stderrTail
          }
        } else {
          Add-Problem $warnings "worktree-path-missing" "Lease path is not present on disk."
        }
      } else {
        Add-Problem $warnings "worktree-path-missing" "Lease record does not contain a worktree path."
      }
    }
  } elseif ($Action -eq "create") {
    if ([string]::IsNullOrWhiteSpace($Objective)) {
      Add-Problem $blockers "objective-required" "Create requires -Objective."
    }
    if (Test-Path -LiteralPath $leasePath) {
      Add-Problem $blockers "lease-path-exists" "Lease path already exists: $leasePath"
    }
    $existingLease = Get-LatestLease $records $leaseIdResolved
    if ($existingLease -and $existingLease.event -ne "cleanup") {
      Add-Problem $blockers "lease-id-active" "Lease id $leaseIdResolved already has an active record."
    }

    $base = Invoke-Captured $git @("rev-parse", "$BaseRef^{commit}")
    $baseSha = $base.stdout.Trim()
    $steps.base = [ordered]@{
      ok = ($base.exitCode -eq 0) -and ($baseSha -match '^[0-9a-f]{40}$')
      baseRef = $BaseRef
      baseSha = if ($baseSha) { $baseSha } else { $null }
      exitCode = $base.exitCode
      stderrTail = $base.stderrTail
    }
    if (-not $steps.base.ok) {
      Add-Problem $blockers "base-ref-unresolved" "Could not resolve base ref $BaseRef."
    }

    $branchFormat = Invoke-Captured $git @("check-ref-format", "--branch", $branchNameResolved)
    $steps.branchName = [ordered]@{
      ok = $branchFormat.exitCode -eq 0
      branchName = $branchNameResolved
      exitCode = $branchFormat.exitCode
      stdoutTail = $branchFormat.stdoutTail
      stderrTail = $branchFormat.stderrTail
    }
    if (-not $steps.branchName.ok) {
      Add-Problem $blockers "branch-name-invalid" "Branch name is not a valid Git branch ref: $branchNameResolved"
    }

    $branchExists = Invoke-Captured $git @("show-ref", "--verify", "--quiet", "refs/heads/$branchNameResolved")
    if ($branchExists.exitCode -eq 0) {
      Add-Problem $blockers "branch-exists" "Branch $branchNameResolved already exists."
    }

    $args = @("worktree", "add", "-b", $branchNameResolved, $leasePath, $baseSha)
    [void]$planned.Add([ordered]@{
      kind = "create-worktree"
      display = Format-DisplayCommand $git $args
      args = $args
      destructive = $false
    })

    if ($Execute -and $blockers.Count -eq 0) {
      [void][System.IO.Directory]::CreateDirectory($worktreeRootFullPath)
      $create = Invoke-Captured $git $args
      [void]$executed.Add([ordered]@{
        kind = "create-worktree"
        exitCode = $create.exitCode
        stdoutTail = $create.stdoutTail
        stderrTail = $create.stderrTail
      })
      if ($create.exitCode -ne 0) {
        Add-Problem $blockers "worktree-create-failed" "git worktree add failed."
      } else {
        $record = [ordered]@{
          event = "create"
          recordedAt = $timestamp
          leaseId = $leaseIdResolved
          agentName = $AgentName
          objective = $Objective
          issue = $Issue
          baseRef = $BaseRef
          baseSha = $baseSha
          branchName = $branchNameResolved
          worktreeRoot = $worktreeRootFullPath
          worktreePath = $leasePath
          allowedScope = Split-ListText $AllowedScope
          permissions = [ordered]@{
            editsAllowed = [bool]$AllowEdits
            gitWritesAllowed = [bool]$AllowGitWrites
            githubWritesAllowed = [bool]$AllowGithubWrites
            runtimeActionsAllowed = [bool]$AllowRuntimeActions
          }
        }
        Write-LeaseRecord $record $leaseStateFullPath
        Write-LeaseBrief $leasePath $record
      }
    }
  } elseif ($Action -eq "cleanup") {
    $lease = Get-LatestLease $records $leaseIdResolved
    if ($null -eq $lease) {
      Add-Problem $blockers "lease-not-found" "Lease $leaseIdResolved was not found in $LeaseStatePath."
    } else {
      if (-not [string]::IsNullOrWhiteSpace([string]$lease.branchName)) {
        $effectiveBranchName = [string]$lease.branchName
      }
      if ($lease.event -eq "cleanup") {
        Add-Problem $warnings "lease-already-cleaned" "Lease $leaseIdResolved is already recorded as cleaned."
      } else {
        $leaseRoot = Resolve-LeaseRoot $lease
        $effectiveWorktreeRoot = $leaseRoot
        $leasePath = Resolve-UnderRoot $lease.worktreePath $leaseRoot "Lease path"
        $registration = Test-RegisteredLeaseWorktree $leasePath $lease.branchName
        $steps.registeredWorktree = $registration
        if ($registration.exitCode -ne 0) {
          Add-Problem $blockers "worktree-list-failed" "Could not verify registered git worktrees."
        } elseif (-not $registration.registered) {
          Add-Problem $blockers "worktree-not-registered" "Lease path is not registered as a worktree for this repository."
        } elseif (-not $registration.branchOk) {
          Add-Problem $blockers "worktree-branch-mismatch" "Lease worktree is not on the recorded lease branch."
        }
        if (-not (Test-Path -LiteralPath $leasePath)) {
          Add-Problem $warnings "worktree-path-missing" "Lease path is not present on disk: $leasePath"
        } elseif ($blockers.Count -eq 0) {
          $status = Invoke-Captured $git @("-C", $leasePath, "status", "--porcelain", "--ignored=matching")
          $statusLines = Split-StatusText $status.stdout
          $brokerOwnedStatusLines = @(
            Select-BrokerOwnedStatusLines $statusLines |
              Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
          )
          $brokerExtraItems = Get-BrokerLeaseDirectoryExtraItems $leasePath
          $userStatusLines = @(
            @((Remove-BrokerOwnedStatusLines $statusLines) + $brokerExtraItems) |
              Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
          )
          $dirty = $userStatusLines.Count -gt 0
          $steps.worktreeStatus = [ordered]@{
            ok = $status.exitCode -eq 0
            exitCode = $status.exitCode
            dirty = $dirty
            stdoutTail = @($userStatusLines | Select-Object -Last 40)
            brokerOwnedTail = @($brokerOwnedStatusLines | Select-Object -Last 40)
            brokerExtraTail = @($brokerExtraItems | Select-Object -Last 40)
            stderrTail = $status.stderrTail
          }
          if ($status.exitCode -ne 0) {
            Add-Problem $blockers "worktree-status-failed" "Could not inspect worktree status."
          } elseif ($dirty -and -not $Force) {
            Add-Problem $blockers "worktree-dirty" "Worktree has local changes; use -Force only after collecting the handoff."
          }
        }

        [void]$planned.Add([ordered]@{
          kind = "remove-broker-lease-files"
          paths = @("AGENT_LEASE.md", ".agent-worktree/")
          destructive = $true
        })
        $args = if ($Force) {
          @("worktree", "remove", "--force", $leasePath)
        } else {
          @("worktree", "remove", $leasePath)
        }
        [void]$planned.Add([ordered]@{
          kind = "cleanup-worktree"
          display = Format-DisplayCommand $git $args
          args = $args
          destructive = $true
        })

        if ($Execute -and $blockers.Count -eq 0) {
          $removedBrokerFiles = Remove-BrokerOwnedLeaseFiles $leasePath
          [void]$executed.Add([ordered]@{
            kind = "remove-broker-lease-files"
            removed = @($removedBrokerFiles)
          })
          $cleanup = Invoke-Captured $git $args
          [void]$executed.Add([ordered]@{
            kind = "cleanup-worktree"
            exitCode = $cleanup.exitCode
            stdoutTail = $cleanup.stdoutTail
            stderrTail = $cleanup.stderrTail
          })
          if ($cleanup.exitCode -ne 0) {
            Add-Problem $blockers "worktree-cleanup-failed" "git worktree remove failed."
          } else {
            $record = [ordered]@{
              event = "cleanup"
              recordedAt = $timestamp
              leaseId = $leaseIdResolved
              agentName = $lease.agentName
              objective = $lease.objective
              issue = $lease.issue
              baseRef = $lease.baseRef
              baseSha = $lease.baseSha
              branchName = $lease.branchName
              worktreeRoot = $lease.worktreeRoot
              worktreePath = $leasePath
              force = [bool]$Force
            }
            Write-LeaseRecord $record $leaseStateFullPath
          }
        }
      }
    }
  }

  $ok = $blockers.Count -eq 0
  $payload = [ordered]@{
    ok = $ok
    command = "agent-worktree"
    action = $Action
    dryRun = -not $Execute
    repoRoot = $repoRoot.Path
    worktreeRoot = $effectiveWorktreeRoot
    leaseStatePath = $leaseStateFullPath
    leaseId = $leaseIdResolved
    branchName = $effectiveBranchName
    leasePath = $leasePath
    blockers = @($blockers)
    warnings = @($warnings)
    leases = ConvertTo-LeaseSummary $records
    plannedActions = @($planned)
    executedActions = @($executed)
    steps = $steps
  }

  $payload | ConvertTo-Json -Depth 40
  if ($ok) {
    exit 0
  }
  exit 1
} catch {
  $payload = [ordered]@{
    ok = $false
    command = "agent-worktree"
    action = $Action
    dryRun = -not $Execute
    repoRoot = $repoRoot.Path
    error = $_.Exception.Message
    blockers = @($blockers)
    warnings = @($warnings)
    plannedActions = @($planned)
    executedActions = @($executed)
    steps = $steps
  }
  $payload | ConvertTo-Json -Depth 40
  exit 1
}
