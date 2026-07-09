[CmdletBinding()]
param(
  [string]$Repo = "Guffawaffle/stfc-mod",
  [string]$Tag = "",
  [string]$State = "",
  [string]$Reason = "",
  [string]$ReplacementTag = "",
  [string]$Operator = "",
  [switch]$Execute,
  [string]$RecordPath = "docs/release-withdrawals/release-withdrawals.jsonl"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$validStates = @("superseded", "known-bad", "yanked")
$stableForkTagPattern = '^v\d+\.\d+\.\d+-guffa\.\d+$'

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
    [int]$Count = 40,
    [int]$MaxLineLength = 1200
  )

  if ([string]::IsNullOrWhiteSpace($Text)) {
    return @()
  }

  $lines = $Text -split "`r?`n" | Where-Object { $_ -ne "" } | ForEach-Object {
    if ($_.Length -gt $MaxLineLength) {
      "$($_.Substring(0, $MaxLineLength))...[truncated]"
    } else {
      $_
    }
  }
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

function ConvertFrom-JsonOrNull {
  param([string]$Text)

  if ([string]::IsNullOrWhiteSpace($Text)) {
    return $null
  }

  try {
    return $Text | ConvertFrom-Json -Depth 40
  } catch {
    return $null
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

function Format-DisplayCommand {
  param(
    [string]$FilePath,
    [string[]]$Arguments
  )

  $parts = @($FilePath) + $Arguments
  return ($parts | ForEach-Object {
    if ($_ -match '^[A-Za-z0-9_./:@=+\-]+$') {
      $_
    } else {
      '"' + ($_.Replace('"', '\"')) + '"'
    }
  }) -join " "
}

function Add-Action {
  param(
    [System.Collections.ArrayList]$Target,
    [string]$Kind,
    [string]$Description,
    [string]$FilePath,
    [string[]]$Arguments,
    [bool]$Destructive = $false
  )

  [void]$Target.Add([ordered]@{
    kind = $Kind
    description = $Description
    destructive = $Destructive
    file = $FilePath
    args = $Arguments
    display = Format-DisplayCommand $FilePath $Arguments
  })
}

function Resolve-RecordPath {
  param([string]$Path)

  if ([System.IO.Path]::IsPathRooted($Path)) {
    return $Path
  }

  return Join-Path $repoRoot.Path $Path
}

function Write-WithdrawalRecord {
  param(
    [System.Collections.IDictionary]$Record,
    [string]$Path
  )

  $fullPath = Resolve-RecordPath $Path
  $directory = Split-Path -Parent $fullPath
  if (-not [string]::IsNullOrWhiteSpace($directory)) {
    [void][System.IO.Directory]::CreateDirectory($directory)
  }

  $line = ($Record | ConvertTo-Json -Depth 20 -Compress)
  [System.IO.File]::AppendAllText($fullPath, "$line`n", [System.Text.UTF8Encoding]::new($false))
  return $fullPath
}

function Copy-WithdrawalRecord {
  param([System.Collections.IDictionary]$Record)

  $copy = [ordered]@{}
  foreach ($key in $Record.Keys) {
    $copy[$key] = $Record[$key]
  }

  return $copy
}

function Remove-LeadingReleaseWithdrawalNotice {
  param([string]$Body)

  if ([string]::IsNullOrWhiteSpace($Body)) {
    return ""
  }

  $lines = @($Body -split "`r?`n")
  if ($lines.Count -lt 6) {
    return $Body
  }

  $hasExistingNotice = ($lines[0] -eq "> [!WARNING]") -and
                       $lines[1].StartsWith("> Fork release state: **", [System.StringComparison]::Ordinal)
  if (-not $hasExistingNotice) {
    return $Body
  }

  $requiredPrefixes = @("> Reason: ", "> Replacement: ", "> Operator: ", "> Recorded: ")
  for ($i = 0; $i -lt $requiredPrefixes.Count; $i++) {
    if (-not $lines[$i + 2].StartsWith($requiredPrefixes[$i], [System.StringComparison]::Ordinal)) {
      return $Body
    }
  }

  $dropCount = 6
  if ($lines.Count -gt $dropCount -and $lines[$dropCount] -eq "") {
    $dropCount++
  }
  if ($lines.Count -le $dropCount) {
    return ""
  }

  return ($lines[$dropCount..($lines.Count - 1)] -join "`r`n")
}

function New-WithdrawalExecutionRecord {
  param(
    [System.Collections.IDictionary]$BaseRecord,
    [string]$Event,
    [System.Collections.ArrayList]$ExecutedActions,
    [System.Collections.ArrayList]$Blockers
  )

  $copy = Copy-WithdrawalRecord $BaseRecord
  $copy.event = $Event
  $copy.recordedAt = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
  $copy.ok = $Blockers.Count -eq 0
  $deleteAction = @($ExecutedActions | Where-Object { $_.kind -eq "delete-release-and-tag" } | Select-Object -First 1)
  if ($deleteAction.Count -gt 0) {
    $copy.deleteExitCode = $deleteAction[0].exitCode
    $copy.deleteSucceeded = $deleteAction[0].exitCode -eq 0
  } else {
    $copy.deleteExitCode = $null
    $copy.deleteSucceeded = $false
  }
  $copy.executedActions = @($ExecutedActions)
  $copy.blockers = @($Blockers)

  return $copy
}

function Build-ReleaseNotice {
  param(
    [string]$State,
    [string]$Reason,
    [string]$ReplacementTag,
    [string]$Operator,
    [string]$Timestamp
  )

  $replacement = if ([string]::IsNullOrWhiteSpace($ReplacementTag)) { "none recorded" } else { $ReplacementTag }
  $label = switch ($State) {
    "superseded" { "Superseded" }
    "known-bad" { "Known bad" }
    "yanked" { "Yanked" }
  }

  return @"
> [!WARNING]
> Fork release state: **$label**
> Reason: $Reason
> Replacement: $replacement
> Operator: $Operator
> Recorded: $Timestamp
"@
}

$blockers = [System.Collections.ArrayList]::new()
$warnings = [System.Collections.ArrayList]::new()
$plannedActions = [System.Collections.ArrayList]::new()
$executedActions = [System.Collections.ArrayList]::new()
$recordEventsWritten = [System.Collections.ArrayList]::new()
$commands = [ordered]@{}
$steps = [ordered]@{}
$record = $null
$recordWrittenPath = $null

try {
  $gh = Resolve-FirstCommand @("gh")
  $commands.gh = $gh

  $normalizedState = $State.Trim().ToLowerInvariant()
  $normalizedTag = $Tag.Trim()
  $normalizedReason = $Reason.Trim()
  $normalizedReplacement = $ReplacementTag.Trim()
  $timestamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")

  if ([string]::IsNullOrWhiteSpace($normalizedTag)) {
    Add-Problem $blockers "tag-required" "A release tag is required."
  }
  $tagShapeOk = (-not [string]::IsNullOrWhiteSpace($normalizedTag)) -and ($normalizedTag -match $stableForkTagPattern)
  if (-not [string]::IsNullOrWhiteSpace($normalizedTag) -and -not $tagShapeOk) {
    Add-Problem $blockers "tag-shape-invalid" "Tag $normalizedTag is not a stable fork tag shaped like vX.Y.Z-guffa.N."
  }
  $replacementShapeOk = $true
  if (-not [string]::IsNullOrWhiteSpace($normalizedReplacement)) {
    $replacementShapeOk = $normalizedReplacement -match $stableForkTagPattern
    if (-not $replacementShapeOk) {
      Add-Problem $blockers "replacement-tag-shape-invalid" "Replacement tag $normalizedReplacement is not a stable fork tag shaped like vX.Y.Z-guffa.N."
    }
  }
  $steps.tag = [ordered]@{
    ok = $tagShapeOk
    tag = if ($normalizedTag) { $normalizedTag } else { $null }
    replacementTag = if ($normalizedReplacement) { $normalizedReplacement } else { $null }
    expectedPattern = $stableForkTagPattern
    shapeOk = $tagShapeOk
    replacementShapeOk = $replacementShapeOk
  }
  if ($validStates -notcontains $normalizedState) {
    Add-Problem $blockers "state-required" "State must be one of: $($validStates -join ', ')."
  }
  if ([string]::IsNullOrWhiteSpace($normalizedReason)) {
    Add-Problem $blockers "reason-required" "A non-empty reason is required for every release withdrawal action."
  }

  if ([string]::IsNullOrWhiteSpace($Operator)) {
    $user = Invoke-Captured $gh @("api", "user", "--jq", ".login")
    $steps.operator = [ordered]@{
      ok = ($user.exitCode -eq 0) -and (-not [string]::IsNullOrWhiteSpace($user.stdout))
      exitCode = $user.exitCode
      stdoutTail = $user.stdoutTail
      stderrTail = $user.stderrTail
    }
    if ($steps.operator.ok) {
      $Operator = $user.stdout.Trim()
    } else {
      Add-Problem $blockers "operator-unresolved" "Could not resolve the GitHub operator. Pass -Operator explicitly."
    }
  }
  $normalizedOperator = $Operator.Trim()

  $releaseData = $null
  if ($tagShapeOk) {
    $releaseView = Invoke-Captured $gh @(
      "release",
      "view",
      $normalizedTag,
      "--repo",
      $Repo,
      "--json",
      "tagName,name,isDraft,isPrerelease,body,url,createdAt,publishedAt,targetCommitish,databaseId"
    )
    $releaseData = ConvertFrom-JsonOrNull $releaseView.stdout
    $releaseSummary = $null
    if ($releaseData) {
      $releaseBody = if ($null -eq $releaseData.body) { "" } else { [string]$releaseData.body }
      $releaseSummary = [ordered]@{
        tagName = $releaseData.tagName
        name = $releaseData.name
        isDraft = $releaseData.isDraft
        isPrerelease = $releaseData.isPrerelease
        url = $releaseData.url
        createdAt = $releaseData.createdAt
        publishedAt = $releaseData.publishedAt
        targetCommitish = $releaseData.targetCommitish
        databaseId = $releaseData.databaseId
        bodyLength = $releaseBody.Length
      }
    }
    $steps.release = [ordered]@{
      ok = ($releaseView.exitCode -eq 0) -and ($releaseData -ne $null)
      command = "gh release view $normalizedTag --repo $Repo"
      exitCode = $releaseView.exitCode
      data = $releaseSummary
      stdoutTail = if (($releaseView.exitCode -eq 0) -and ($releaseData -ne $null)) {
        $null
      } else {
        $releaseView.stdoutTail
      }
      stderrTail = $releaseView.stderrTail
    }
    if (-not $steps.release.ok) {
      Add-Problem $blockers "release-not-found" "Could not load release $normalizedTag from $Repo."
    }
  }

  if ($normalizedState -eq "yanked" -and [string]::IsNullOrWhiteSpace($normalizedReplacement)) {
    Add-Problem $warnings "replacement-missing" "No replacement tag was recorded for a yanked release."
  }
  if (-not [string]::IsNullOrWhiteSpace($normalizedReplacement) -and $normalizedReplacement -eq $normalizedTag) {
    Add-Problem $blockers "replacement-same-as-tag" "Replacement tag must differ from the affected tag."
  }

  if (-not [string]::IsNullOrWhiteSpace($normalizedReplacement) -and $replacementShapeOk) {
    $replacementView = Invoke-Captured $gh @(
      "release",
      "view",
      $normalizedReplacement,
      "--repo",
      $Repo,
      "--json",
      "tagName,name,isDraft,isPrerelease,url,targetCommitish,databaseId"
    )
    $replacementData = ConvertFrom-JsonOrNull $replacementView.stdout
    $steps.replacement = [ordered]@{
      ok = ($replacementView.exitCode -eq 0) -and ($replacementData -ne $null)
      command = "gh release view $normalizedReplacement --repo $Repo"
      exitCode = $replacementView.exitCode
      data = $replacementData
      stdoutTail = if (($replacementView.exitCode -eq 0) -and ($replacementData -ne $null)) {
        $null
      } else {
        $replacementView.stdoutTail
      }
      stderrTail = $replacementView.stderrTail
    }
    if (-not $steps.replacement.ok) {
      Add-Problem $blockers "replacement-release-not-found" "Could not load replacement release $normalizedReplacement from $Repo."
    }
  }

  if ($blockers.Count -eq 0) {
    $releaseName = if ([string]::IsNullOrWhiteSpace($releaseData.name)) { $normalizedTag } else { $releaseData.name }
    $notice = Build-ReleaseNotice $normalizedState $normalizedReason $normalizedReplacement $normalizedOperator $timestamp
    $record = [ordered]@{
      recordedAt = $timestamp
      repo = $Repo
      tag = $normalizedTag
      state = $normalizedState
      reason = $normalizedReason
      replacementTag = if ([string]::IsNullOrWhiteSpace($normalizedReplacement)) { $null } else { $normalizedReplacement }
      operator = $normalizedOperator
      releaseUrl = $releaseData.url
      releaseDatabaseId = $releaseData.databaseId
      targetCommitish = $releaseData.targetCommitish
    }

    if ($normalizedState -eq "superseded") {
      $title = if ($releaseName.StartsWith("[SUPERSEDED] ")) { $releaseName } else { "[SUPERSEDED] $releaseName" }
      Add-Action $plannedActions "edit-release" "Mark the release as superseded and prepend the release-state notice." $gh @(
        "release",
        "edit",
        $normalizedTag,
        "--repo",
        $Repo,
        "--title",
        $title,
        "--notes-file",
        "<generated-notes-file>"
      )
      if (-not [string]::IsNullOrWhiteSpace($normalizedReplacement)) {
        Add-Action $plannedActions "mark-replacement-latest" "Mark the replacement release as latest." $gh @(
          "release",
          "edit",
          $normalizedReplacement,
          "--repo",
          $Repo,
          "--latest"
        )
      }
    } elseif ($normalizedState -eq "known-bad") {
      $title = if ($releaseName.StartsWith("[KNOWN BAD] ")) { $releaseName } else { "[KNOWN BAD] $releaseName" }
      Add-Action $plannedActions "edit-release" "Mark the release known-bad, mark it prerelease, and prepend the release-state notice." $gh @(
        "release",
        "edit",
        $normalizedTag,
        "--repo",
        $Repo,
        "--title",
        $title,
        "--prerelease",
        "--notes-file",
        "<generated-notes-file>"
      )
      if (-not [string]::IsNullOrWhiteSpace($normalizedReplacement)) {
        Add-Action $plannedActions "mark-replacement-latest" "Mark the replacement release as latest." $gh @(
          "release",
          "edit",
          $normalizedReplacement,
          "--repo",
          $Repo,
          "--latest"
        )
      }
    } elseif ($normalizedState -eq "yanked") {
      Add-Action $plannedActions "delete-release-and-tag" "Delete the GitHub release and remote tag." $gh @(
        "release",
        "delete",
        $normalizedTag,
        "--repo",
        $Repo,
        "--cleanup-tag",
        "--yes"
      ) $true
      if (-not [string]::IsNullOrWhiteSpace($normalizedReplacement)) {
        Add-Action $plannedActions "mark-replacement-latest" "Mark the replacement release as latest." $gh @(
          "release",
          "edit",
          $normalizedReplacement,
          "--repo",
          $Repo,
          "--latest"
        )
      }
    }
  }

  if ($Execute -and $blockers.Count -eq 0) {
    $body = if ($null -eq $releaseData.body) { "" } else { [string]$releaseData.body }
    $bodyWithoutExistingNotice = Remove-LeadingReleaseWithdrawalNotice $body
    $notesPath = $null
    if ($normalizedState -eq "superseded" -or $normalizedState -eq "known-bad") {
      $notesPath = [System.IO.Path]::GetTempFileName()
      $notesBody = if ([string]::IsNullOrWhiteSpace($bodyWithoutExistingNotice)) {
        $notice
      } else {
        "$notice`r`n`r`n$bodyWithoutExistingNotice"
      }
      [System.IO.File]::WriteAllText($notesPath, $notesBody, [System.Text.UTF8Encoding]::new($false))
      $steps.releaseNotice = [ordered]@{
        existingNoticeRemoved = $bodyWithoutExistingNotice -ne $body
        originalBodyLength = $body.Length
        updatedBodyLength = $notesBody.Length
      }
    }

    try {
      if ($normalizedState -eq "yanked") {
        $preActionRecord = Copy-WithdrawalRecord $record
        $preActionRecord.event = "pre-yank"
        $preActionRecord.auditNote = "Recorded before destructive release and tag deletion."
        $recordWrittenPath = Write-WithdrawalRecord $preActionRecord $RecordPath
        [void]$recordEventsWritten.Add("pre-yank")
      }

      foreach ($action in $plannedActions) {
        $args = @($action.args)
        if ($notesPath) {
          $args = @($args | ForEach-Object {
            if ($_ -eq "<generated-notes-file>") {
              $notesPath
            } else {
              $_
            }
          })
        }

        if ($action.destructive) {
          [Console]::Error.WriteLine("DESTRUCTIVE ACTION: $(Format-DisplayCommand $action.file $args)")
        }

        $result = Invoke-Captured $action.file $args
        [void]$executedActions.Add([ordered]@{
          kind = $action.kind
          destructive = $action.destructive
          display = Format-DisplayCommand $action.file $args
          exitCode = $result.exitCode
          stdoutTail = $result.stdoutTail
          stderrTail = $result.stderrTail
        })
        if ($result.exitCode -ne 0) {
          Add-Problem $blockers "action-failed" "Action $($action.kind) failed with exit code $($result.exitCode)."
          break
        }
      }

      if ($normalizedState -ne "yanked" -and $blockers.Count -eq 0) {
        $appliedRecord = Copy-WithdrawalRecord $record
        $appliedRecord.event = "applied"
        $recordWrittenPath = Write-WithdrawalRecord $appliedRecord $RecordPath
        [void]$recordEventsWritten.Add("applied")
      } elseif ($normalizedState -eq "yanked") {
        $outcomeRecord = New-WithdrawalExecutionRecord $record "post-yank" $executedActions $blockers
        $recordWrittenPath = Write-WithdrawalRecord $outcomeRecord $RecordPath
        [void]$recordEventsWritten.Add("post-yank")
      }
    } finally {
      if ($notesPath -and (Test-Path -LiteralPath $notesPath)) {
        Remove-Item -LiteralPath $notesPath -Force
      }
    }
  }

  $ok = ($blockers.Count -eq 0)
  $payload = [ordered]@{
    ok = $ok
    command = "release-withdrawal"
    dryRun = -not $Execute
    repoRoot = $repoRoot.Path
    repo = $Repo
    tag = if ($normalizedTag) { $normalizedTag } else { $null }
    state = if ($normalizedState) { $normalizedState } else { $null }
    reason = if ($normalizedReason) { $normalizedReason } else { $null }
    replacementTag = if ($normalizedReplacement) { $normalizedReplacement } else { $null }
    operator = if ($normalizedOperator) { $normalizedOperator } else { $null }
    recordPath = $RecordPath
    recordWrittenPath = $recordWrittenPath
    recordEventsWritten = @($recordEventsWritten)
    blockers = @($blockers)
    warnings = @($warnings)
    plannedActions = @($plannedActions)
    executedActions = @($executedActions)
    record = $record
    steps = $steps
    commands = $commands
  }

  $payload | ConvertTo-Json -Depth 40
  if ($ok) {
    exit 0
  }
  exit 1
} catch {
  $payload = [ordered]@{
    ok = $false
    command = "release-withdrawal"
    dryRun = -not $Execute
    repoRoot = $repoRoot.Path
    repo = $Repo
    error = $_.Exception.Message
    blockers = @($blockers)
    warnings = @($warnings)
    plannedActions = @($plannedActions)
    executedActions = @($executedActions)
    recordEventsWritten = @($recordEventsWritten)
    record = $record
    steps = $steps
    commands = $commands
  }
  $payload | ConvertTo-Json -Depth 40
  exit 1
}
