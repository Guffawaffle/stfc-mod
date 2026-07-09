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
  [switch]$AllowGithubWrites,
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
  if (-not ($fullPath.Equals($repoFullPath, [System.StringComparison]::OrdinalIgnoreCase) -or
            $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase))) {
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

$branch = Invoke-Captured $git @("branch", "--show-current")
$head = Invoke-Captured $git @("rev-parse", "HEAD")
$status = Invoke-Captured $git @("status", "--short", "--branch")
$remote = Invoke-Captured $git @("remote", "get-url", "origin")

$prUrl = $null
if ($gh) {
  $prView = Invoke-Captured $gh @("pr", "view", "--json", "url", "--jq", ".url")
  if ($prView.exitCode -eq 0 -and -not [string]::IsNullOrWhiteSpace($prView.stdout)) {
    $prUrl = $prView.stdout
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
if (-not $AllowGithubWrites) {
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
if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
  $writtenPath = Resolve-OutputPath $OutputPath
  $directory = Split-Path -Parent $writtenPath
  if (-not [string]::IsNullOrWhiteSpace($directory)) {
    [void][System.IO.Directory]::CreateDirectory($directory)
  }
  [System.IO.File]::WriteAllText($writtenPath, $briefMarkdown, [System.Text.UTF8Encoding]::new($false))
}

$payload = [ordered]@{
  ok = $true
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
    githubWritesAllowed = [bool]$AllowGithubWrites
    runtimeActionsAllowed = [bool]$AllowRuntimeActions
  }
  scope = $scopeItems
  questions = $questionItems
  allowedActions = $allowed
  forbiddenActions = $forbiddenItems
  expectedOutput = $expectedOutput
  briefMarkdown = $briefMarkdown
  writtenPath = $writtenPath
}

$payload | ConvertTo-Json -Depth 30
exit 0
