[CmdletBinding()]
param(
  [string]$Repo = "Guffawaffle/stfc-mod",
  [string]$BaseBranch = "main",
  [string]$TargetRef = "",
  [string]$Tag = "",
  [int]$RunLimit = 50,
  [switch]$SmokeAcknowledged,
  [switch]$PushTag,
  [switch]$AllowDirty,
  [switch]$SkipBuildCheck,
  [switch]$SkipArtifactCheck
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$stableForkTagPattern = '^v\d+\.\d+\.\d+-guffa\.\d+$'
$expectedArtifacts = @(
  "stfc-community-mod",
  "stfc-community-mod-macos-universal",
  "stfc-community-mod-installer"
)

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

function Get-RemoteForkTags {
  param(
    [string]$Git,
    [string]$Remote = "origin"
  )

  $result = Invoke-Captured $Git @("ls-remote", "--tags", $Remote, "v*-guffa.*")
  if ($result.exitCode -ne 0) {
    throw "git ls-remote failed: $($result.stderr)"
  }

  $tags = New-Object System.Collections.Generic.HashSet[string]
  foreach ($line in ($result.stdout -split "`r?`n")) {
    if ([string]::IsNullOrWhiteSpace($line)) {
      continue
    }

    $parts = $line -split "\s+"
    if ($parts.Count -lt 2) {
      continue
    }

    $ref = $parts[1]
    if (-not $ref.StartsWith("refs/tags/")) {
      continue
    }

    $name = $ref.Substring("refs/tags/".Length)
    if ($name.EndsWith("^{}")) {
      $name = $name.Substring(0, $name.Length - 3)
    }

    [void]$tags.Add($name)
  }

  return @($tags)
}

function Resolve-NextForkTag {
  param(
    [string[]]$Tags
  )

  $stable = @()
  foreach ($candidate in $Tags) {
    if ($candidate -match '^v(?<major>\d+)\.(?<minor>\d+)\.(?<patch>\d+)-guffa\.(?<fork>\d+)$') {
      $stable += [pscustomobject]@{
        tag = $candidate
        major = [int]$Matches.major
        minor = [int]$Matches.minor
        patch = [int]$Matches.patch
        fork = [int]$Matches.fork
      }
    }
  }

  if (-not $stable) {
    throw "No stable vX.Y.Z-guffa.N tags found on origin."
  }

  $latest = $stable | Sort-Object major, minor, patch, fork | Select-Object -Last 1
  return [ordered]@{
    previousTag = $latest.tag
    nextTag = "v$($latest.major).$($latest.minor).$($latest.patch)-guffa.$($latest.fork + 1)"
    baseVersion = "$($latest.major).$($latest.minor).$($latest.patch)"
  }
}

$blockers = [System.Collections.ArrayList]::new()
$warnings = [System.Collections.ArrayList]::new()
$steps = [ordered]@{}
$commands = [ordered]@{}
$tagAction = [ordered]@{
  attempted = $false
  createdLocalTag = $false
  pushedTag = $false
}

try {
  $git = Resolve-FirstCommand @("git")
  $gh = Resolve-FirstCommand @("gh")
  $commands.git = $git
  $commands.gh = $gh

  if ([string]::IsNullOrWhiteSpace($TargetRef)) {
    $TargetRef = "origin/$BaseBranch"
  }

  $fetchBase = Invoke-Captured $git @(
    "fetch",
    "origin",
    "+refs/heads/$($BaseBranch):refs/remotes/origin/$($BaseBranch)",
    "--prune"
  )
  $steps.fetchBase = [ordered]@{
    ok = ($fetchBase.exitCode -eq 0)
    command = "git fetch origin +refs/heads/$($BaseBranch):refs/remotes/origin/$($BaseBranch) --prune"
    exitCode = $fetchBase.exitCode
    stdoutTail = $fetchBase.stdoutTail
    stderrTail = $fetchBase.stderrTail
  }
  if (-not $steps.fetchBase.ok) {
    Add-Problem $blockers "fetch-base-failed" "Could not fetch origin/$BaseBranch before resolving the release target."
  }

  $repoView = Invoke-Captured $gh @("repo", "view", "--json", "nameWithOwner,defaultBranchRef")
  $repoData = ConvertFrom-JsonOrNull $repoView.stdout
  $steps.repository = [ordered]@{
    ok = ($repoView.exitCode -eq 0) -and ($repoData -ne $null)
    command = "gh repo view --json nameWithOwner,defaultBranchRef"
    exitCode = $repoView.exitCode
    data = $repoData
    stdoutTail = $repoView.stdoutTail
    stderrTail = $repoView.stderrTail
  }

  if (-not $steps.repository.ok) {
    Add-Problem $blockers "repo-view-failed" "Could not resolve GitHub repository metadata."
  } else {
    if ($repoData.nameWithOwner -ne $Repo) {
      Add-Problem $blockers "wrong-repository" "Expected $Repo but current GitHub repo is $($repoData.nameWithOwner)."
    }
    if ($repoData.defaultBranchRef.name -ne $BaseBranch) {
      Add-Problem $warnings "unexpected-default-branch" "Expected default branch $BaseBranch but repo default is $($repoData.defaultBranchRef.name)."
    }
  }

  $targetResolve = Invoke-Captured $git @("rev-parse", "$TargetRef^{commit}")
  $targetSha = $targetResolve.stdout.Trim()
  $steps.target = [ordered]@{
    ok = ($targetResolve.exitCode -eq 0) -and ($targetSha -match '^[0-9a-f]{40}$')
    targetRef = $TargetRef
    targetSha = if ($targetSha) { $targetSha } else { $null }
    command = "git rev-parse $TargetRef^{commit}"
    exitCode = $targetResolve.exitCode
    stdoutTail = $targetResolve.stdoutTail
    stderrTail = $targetResolve.stderrTail
  }

  if (-not $steps.target.ok) {
    Add-Problem $blockers "target-ref-unresolved" "Could not resolve target ref $TargetRef."
  }

  if ($TargetRef -ne "origin/$BaseBranch") {
    Add-Problem $warnings "target-override" "Target ref is $TargetRef, not origin/$BaseBranch."
  }

  $baseResolve = Invoke-Captured $git @("rev-parse", "origin/$BaseBranch^{commit}")
  $baseSha = $baseResolve.stdout.Trim()
  $steps.base = [ordered]@{
    ok = ($baseResolve.exitCode -eq 0) -and ($baseSha -match '^[0-9a-f]{40}$')
    baseRef = "origin/$BaseBranch"
    baseSha = if ($baseSha) { $baseSha } else { $null }
    exitCode = $baseResolve.exitCode
    stderrTail = $baseResolve.stderrTail
  }

  if (-not $steps.base.ok) {
    Add-Problem $blockers "base-ref-unresolved" "Could not resolve origin/$BaseBranch."
  } elseif ($targetSha -and $targetSha -ne $baseSha) {
    if ($PushTag) {
      Add-Problem $blockers "target-not-base-head" "Refusing to push a release tag for $targetSha because origin/$BaseBranch is $baseSha."
    } else {
      Add-Problem $warnings "target-not-base-head" "Target SHA $targetSha does not match origin/$BaseBranch $baseSha."
    }
  }

  $status = Invoke-Captured $git @("status", "--porcelain")
  $dirtyLines = @()
  if (-not [string]::IsNullOrWhiteSpace($status.stdout)) {
    $dirtyLines = @($status.stdout -split "`r?`n" | Where-Object { $_ -ne "" })
  }
  $steps.worktree = [ordered]@{
    ok = ($status.exitCode -eq 0)
    dirty = ($dirtyLines.Count -gt 0)
    dirtyCount = $dirtyLines.Count
    dirtyFiles = $dirtyLines
  }
  if ($dirtyLines.Count -gt 0) {
    if ($PushTag -and -not $AllowDirty) {
      Add-Problem $blockers "dirty-worktree" "Working tree is dirty; use -AllowDirty only if this is intentional."
    } else {
      Add-Problem $warnings "dirty-worktree" "Working tree is dirty; dry-run can continue, but tag push should use a clean tree."
    }
  }

  $remoteTags = Get-RemoteForkTags $git
  $tagPlan = Resolve-NextForkTag $remoteTags
  if ([string]::IsNullOrWhiteSpace($Tag)) {
    $Tag = $tagPlan.nextTag
  }

  $tagShapeOk = $Tag -match $stableForkTagPattern
  $localTag = Invoke-Captured $git @("tag", "--list", $Tag)
  $localTagExists = -not [string]::IsNullOrWhiteSpace($localTag.stdout)
  $remoteTagExists = $remoteTags -contains $Tag
  $steps.tag = [ordered]@{
    ok = $tagShapeOk -and -not $remoteTagExists -and -not $localTagExists
    proposedTag = $Tag
    previousStableForkTag = $tagPlan.previousTag
    inferredBaseVersion = $tagPlan.baseVersion
    expectedPattern = $stableForkTagPattern
    shapeOk = $tagShapeOk
    localTagExists = $localTagExists
    remoteTagExists = $remoteTagExists
  }
  if (-not $tagShapeOk) {
    Add-Problem $blockers "tag-shape-invalid" "Tag $Tag is not a stable fork tag shaped like vX.Y.Z-guffa.N."
  }
  if ($localTagExists -or $remoteTagExists) {
    Add-Problem $blockers "tag-exists" "Tag $Tag already exists locally or remotely."
  }

  if (-not $SmokeAcknowledged) {
    Add-Problem $blockers "smoke-not-acknowledged" "Production artifact smoke has not been acknowledged."
  }
  if ($PushTag -and $SkipBuildCheck) {
    Add-Problem $blockers "push-tag-skip-build-check" "Refusing to push a release tag while -SkipBuildCheck is set."
  }
  if ($PushTag -and $SkipArtifactCheck) {
    Add-Problem $blockers "push-tag-skip-artifact-check" "Refusing to push a release tag while -SkipArtifactCheck is set."
  }

  $matchingBuild = $null
  if ($SkipBuildCheck) {
    $steps.buildRun = [ordered]@{
      ok = $true
      skipped = $true
      reason = "SkipBuildCheck requested"
    }
  } else {
    $runs = Invoke-Captured $gh @(
      "run",
      "list",
      "--repo",
      $Repo,
      "--workflow",
      "Build",
      "--limit",
      [string]$RunLimit,
      "--json",
      "databaseId,headSha,headBranch,status,conclusion,createdAt,url,workflowName,event"
    )
    $runsData = ConvertFrom-JsonOrNull $runs.stdout
    $runRows = @()
    if ($runsData) {
      $runRows = @($runsData)
    }
    $matchingBuild = $runRows | Where-Object {
      $_.headSha -eq $targetSha -and $_.status -eq "completed" -and $_.conclusion -eq "success"
    } | Select-Object -First 1
    $matchingBuildIsReleaseBuild = ($matchingBuild -ne $null) -and
      ($matchingBuild.event -eq "push") -and
      ($matchingBuild.headBranch -eq $BaseBranch)

    $steps.buildRun = [ordered]@{
      ok = ($runs.exitCode -eq 0) -and $matchingBuildIsReleaseBuild
      command = "gh run list --repo $Repo --workflow Build --limit $RunLimit"
      exitCode = $runs.exitCode
      inspectedRunCount = $runRows.Count
      matchingRun = $matchingBuild
      expectedEvent = "push"
      expectedHeadBranch = $BaseBranch
      stdoutTail = $runs.stdoutTail
      stderrTail = $runs.stderrTail
    }

    if (($runs.exitCode -eq 0) -and ($matchingBuild -ne $null) -and (-not $matchingBuildIsReleaseBuild)) {
      Add-Problem $blockers "matching-build-not-main-push" "Matching Build run $($matchingBuild.databaseId) is not a push build for $BaseBranch."
    } elseif (-not $steps.buildRun.ok) {
      Add-Problem $blockers "matching-build-missing" "No successful Build workflow run found for target SHA $targetSha."
    }
  }

  if ($SkipArtifactCheck) {
    $steps.artifacts = [ordered]@{
      ok = $true
      skipped = $true
      reason = "SkipArtifactCheck requested"
    }
  } elseif ($matchingBuild -ne $null) {
    $artifacts = Invoke-Captured $gh @(
      "api",
      "/repos/$Repo/actions/runs/$($matchingBuild.databaseId)/artifacts"
    )
    $artifactData = ConvertFrom-JsonOrNull $artifacts.stdout
    $artifactRows = @()
    if ($artifactData -and $artifactData.artifacts) {
      $artifactRows = @($artifactData.artifacts)
    }

    $artifactNames = @($artifactRows | ForEach-Object { $_.name })
    $missingArtifacts = @($expectedArtifacts | Where-Object { $artifactNames -notcontains $_ })
    $expiredArtifacts = @($artifactRows | Where-Object { $_.expired -eq $true } | ForEach-Object { $_.name })
    $artifactsOk = ($artifacts.exitCode -eq 0) -and ($missingArtifacts.Count -eq 0) -and ($expiredArtifacts.Count -eq 0)
    $steps.artifacts = [ordered]@{
      ok = $artifactsOk
      command = "gh api /repos/$Repo/actions/runs/$($matchingBuild.databaseId)/artifacts"
      exitCode = $artifacts.exitCode
      expected = $expectedArtifacts
      found = $artifactNames
      missing = $missingArtifacts
      expired = $expiredArtifacts
      stderrTail = $artifacts.stderrTail
    }

    foreach ($missing in $missingArtifacts) {
      Add-Problem $blockers "artifact-missing" "Build run $($matchingBuild.databaseId) is missing artifact $missing."
    }
    foreach ($expired in $expiredArtifacts) {
      Add-Problem $blockers "artifact-expired" "Build run $($matchingBuild.databaseId) artifact $expired is expired."
    }
  } else {
    $steps.artifacts = [ordered]@{
      ok = $false
      skipped = $true
      reason = "No matching build run available for artifact lookup"
    }
    Add-Problem $blockers "artifact-check-unavailable" "Could not verify artifacts because no matching Build workflow run was available."
  }

  $readyToTag = ($blockers.Count -eq 0)

  if ($PushTag) {
    $tagAction.attempted = $true
    if (-not $readyToTag) {
      $tagAction.skippedReason = "preflight blockers present"
    } else {
      $createTag = Invoke-Captured $git @("tag", "-a", $Tag, $targetSha, "-m", $Tag)
      $tagAction.createTag = $createTag
      if ($createTag.exitCode -eq 0) {
        $tagAction.createdLocalTag = $true
        $push = Invoke-Captured $git @("push", "origin", $Tag)
        $tagAction.push = $push
        if ($push.exitCode -eq 0) {
          $tagAction.pushedTag = $true
        } else {
          Add-Problem $blockers "tag-push-failed" "Created local tag $Tag but failed to push it."
        }
      } else {
        Add-Problem $blockers "tag-create-failed" "Failed to create local tag $Tag."
      }
    }
  }

  $ok = ($blockers.Count -eq 0)
  $payload = [ordered]@{
    ok = $ok
    command = "release-preflight"
    repoRoot = $repoRoot.Path
    repo = $Repo
    baseBranch = $BaseBranch
    targetRef = $TargetRef
    targetSha = if ($targetSha) { $targetSha } else { $null }
    proposedTag = $Tag
    dryRun = -not $PushTag
    smokeAcknowledged = [bool]$SmokeAcknowledged
    readyToTag = $readyToTag
    blockers = @($blockers)
    warnings = @($warnings)
    steps = $steps
    tagAction = $tagAction
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
    command = "release-preflight"
    repoRoot = $repoRoot.Path
    repo = $Repo
    error = $_.Exception.Message
    blockers = @($blockers)
    warnings = @($warnings)
    steps = $steps
    tagAction = $tagAction
    commands = $commands
  }
  $payload | ConvertTo-Json -Depth 40
  exit 1
}
