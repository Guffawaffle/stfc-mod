[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$Objective,
  [ValidateSet("scout", "review", "implementation", "reconciliation", "release", "triage")]
  [string]$Mode = "scout",
  [string]$Issue = "",
  [string]$AgentName = "background-agent",
  [string]$Scope = "",
  [string]$Questions = "",
  [string]$AllowedActions = "",
  [string]$ExtraForbidden = "",
  [switch]$AllowEdits,
  [switch]$AllowGitWrites,
  [switch]$AllowGitHubWrites,
  [switch]$AllowRuntimeActions,
  [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Resolve-FirstCommand {
  param(
    [string[]]$Names,
    [switch]$Optional
  )

  foreach ($name in $Names) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) {
      return $cmd.Source
    }
  }

  if ($Optional) {
    return $null
  }

  throw "None of these commands were found: $($Names -join ', ')"
}

function Invoke-Captured {
  param(
    [string]$FilePath,
    [string[]]$Arguments
  )

  if ([string]::IsNullOrWhiteSpace($FilePath)) {
    return [ordered]@{
      exitCode = 127
      stdout = ""
      stderr = "command unavailable"
    }
  }

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
    stdout = $stdout.Trim()
    stderr = $stderr.Trim()
  }
}

function Split-ListText {
  param([string]$Text)

  if ([string]::IsNullOrWhiteSpace($Text)) {
    return @()
  }

  return @($Text -split "`r?`n|;" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
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

function Get-WorkspacePathComparison {
  if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
      [System.Runtime.InteropServices.OSPlatform]::Windows)) {
    return [System.StringComparison]::OrdinalIgnoreCase
  }

  return [System.StringComparison]::Ordinal
}

function Resolve-OutputPath {
  param([string]$Path)

  if ([string]::IsNullOrWhiteSpace($Path)) {
    return $null
  }

  $candidate = if ([System.IO.Path]::IsPathRooted($Path)) {
    $Path
  } else {
    Join-Path $repoRoot.Path $Path
  }
  $fullPath = [System.IO.Path]::GetFullPath($candidate)
  $repoFullPath = [System.IO.Path]::GetFullPath($repoRoot.Path).TrimEnd([System.IO.Path]::DirectorySeparatorChar,
                                                                        [System.IO.Path]::AltDirectorySeparatorChar)
  $prefix = "$repoFullPath$([System.IO.Path]::DirectorySeparatorChar)"
  $comparison = Get-WorkspacePathComparison
  if (-not ($fullPath.Equals($repoFullPath, $comparison) -or $fullPath.StartsWith($prefix, $comparison))) {
    throw "OutputPath must stay within the repository workspace: $Path"
  }

  return $fullPath
}

function ConvertTo-BulletList {
  param([object[]]$Items)

  if (-not $Items -or $Items.Count -eq 0) {
    return "- none specified"
  }

  return ($Items | ForEach-Object { "- $_" }) -join "`n"
}

$git = Resolve-FirstCommand @("git")
$gh = Resolve-FirstCommand @("gh") -Optional
$blockers = [System.Collections.ArrayList]::new()
$warnings = [System.Collections.ArrayList]::new()
$steps = [ordered]@{}

$branch = Invoke-Captured $git @("branch", "--show-current")
$head = Invoke-Captured $git @("rev-parse", "HEAD")
$status = Invoke-Captured $git @("status", "--short", "--branch")
$remote = Invoke-Captured $git @("remote", "get-url", "origin")
$steps.git = [ordered]@{
  branch = $branch
  head = $head
  status = $status
  origin = $remote
}

if ($branch.exitCode -ne 0 -or [string]::IsNullOrWhiteSpace($branch.stdout)) {
  Add-Problem $blockers "branch-unresolved" "Could not resolve the current branch."
}
if ($head.exitCode -ne 0 -or [string]::IsNullOrWhiteSpace($head.stdout)) {
  Add-Problem $blockers "head-unresolved" "Could not resolve HEAD."
}
if ($status.exitCode -ne 0) {
  Add-Problem $blockers "status-unresolved" "Could not read worktree status."
}
if ($remote.exitCode -ne 0 -or [string]::IsNullOrWhiteSpace($remote.stdout)) {
  Add-Problem $blockers "origin-unresolved" "Could not resolve origin remote URL."
}

$prUrl = $null
if ($gh) {
  $prView = Invoke-Captured $gh @("pr", "view", "--json", "url", "--jq", ".url")
  $steps.githubPullRequest = $prView
  if ($prView.exitCode -eq 0 -and -not [string]::IsNullOrWhiteSpace($prView.stdout)) {
    $prUrl = $prView.stdout
  } else {
    Add-Problem $warnings "pull-request-unresolved" "Could not resolve an associated GitHub pull request."
  }
}

$scopeItems = Split-ListText $Scope
$questionItems = Split-ListText $Questions
$allowedItems = Split-ListText $AllowedActions
$extraForbiddenItems = Split-ListText $ExtraForbidden

$defaultAllowed = @(
  "Read files and command output needed to answer the objective.",
  "Use fast local discovery commands such as rg, git diff, git show, and gh view/list when relevant.",
  "Return a concise structured handoff to the orchestrator."
)
$defaultForbidden = @()
if (-not $AllowEdits) {
  $defaultForbidden += "Do not create, edit, move, or delete files."
}
if (-not $AllowGitWrites) {
  $defaultForbidden += "Do not run git switch, checkout, rebase, merge, reset, commit, tag, or push."
}
if (-not $AllowGitHubWrites) {
  $defaultForbidden += "Do not create, edit, close, merge, label, or comment on GitHub issues, PRs, releases, or tags."
}
if (-not $AllowRuntimeActions) {
  $defaultForbidden += "Do not cycle STFC, sidecar, or modify game/runtime files."
}
$defaultForbidden += @(
  "Do not create sibling clones or worktrees.",
  "Do not inspect secrets, tokens, or unrelated private files.",
  "Do not treat findings as final; the orchestrator owns edits, validation, and publishing."
)
$forbiddenItems = @($defaultForbidden + $extraForbiddenItems)
$allowed = @($defaultAllowed + $allowedItems)

$expectedOutput = @(
  "summary: 3-6 bullets of what was inspected and learned",
  "findings: actionable items with file/line evidence when available",
  "risks: behavior, release, validation, or branch risks",
  "openQuestions: only questions that materially block the orchestrator",
  "commandsRun: commands or API reads used",
  "filesRead: important local or remote files inspected",
  "recommendedNextSteps: concrete next moves for the orchestrator",
  "mutationConfirmation: state whether any mutations were made; default should be none"
)

$fence = '```'
$briefMarkdown = @"
# Background Agent Brief

Agent: $AgentName
Mode: $Mode
Issue: $(if ([string]::IsNullOrWhiteSpace($Issue)) { "none" } else { $Issue })
Repository: $($remote.stdout)
Branch: $($branch.stdout)
Head: $($head.stdout)
Pull Request: $(if ($prUrl) { $prUrl } else { "none detected" })

## Objective

$Objective

## Scope

$(ConvertTo-BulletList $scopeItems)

## Questions To Answer

$(ConvertTo-BulletList $questionItems)

## Allowed Actions

$(ConvertTo-BulletList $allowed)

## Forbidden Actions

$(ConvertTo-BulletList $forbiddenItems)

## Expected Handoff

$(ConvertTo-BulletList $expectedOutput)

## Current Worktree

${fence}text
$($status.stdout)
${fence}
"@

$writtenPath = $null
if ($blockers.Count -eq 0 -and -not [string]::IsNullOrWhiteSpace($OutputPath)) {
  $writtenPath = Resolve-OutputPath $OutputPath
  $directory = Split-Path -Parent $writtenPath
  if (-not [string]::IsNullOrWhiteSpace($directory)) {
    [void][System.IO.Directory]::CreateDirectory($directory)
  }
  [System.IO.File]::WriteAllText($writtenPath, $briefMarkdown, [System.Text.UTF8Encoding]::new($false))
}

$payload = [ordered]@{
  ok = $blockers.Count -eq 0
  command = "agent-brief"
  repoRoot = $repoRoot.Path
  mode = $Mode
  agentName = $AgentName
  issue = if ([string]::IsNullOrWhiteSpace($Issue)) { $null } else { $Issue }
  objective = $Objective
  context = [ordered]@{
    origin = $remote.stdout
    branch = $branch.stdout
    head = $head.stdout
    pullRequest = $prUrl
    status = $status.stdout
  }
  permissions = [ordered]@{
    editsAllowed = [bool]$AllowEdits
    gitWritesAllowed = [bool]$AllowGitWrites
    githubWritesAllowed = [bool]$AllowGitHubWrites
    runtimeActionsAllowed = [bool]$AllowRuntimeActions
  }
  blockers = @($blockers)
  warnings = @($warnings)
  scope = $scopeItems
  questions = $questionItems
  allowedActions = $allowed
  forbiddenActions = $forbiddenItems
  expectedOutput = $expectedOutput
  briefMarkdown = $briefMarkdown
  writtenPath = $writtenPath
  steps = $steps
}

$payload | ConvertTo-Json -Depth 30
if ($blockers.Count -eq 0) {
  exit 0
}
exit 1
