param(
    [Parameter(Mandatory)][string]$DllPath,
    [Parameter(Mandatory)][string]$ReleaseContextPath,
    [Parameter(Mandatory)][ValidatePattern('\A[0-9a-f]{40}\z')][string]$SourceSha,
    [Parameter(Mandatory)][ValidatePattern('\A[1-9][0-9]*\z')][string]$BuildRunId,
    [Parameter(Mandatory)][ValidatePattern('\A[0-9a-f]{64}\z')][string]$ExpectedUnsignedSha256,
    [Parameter(Mandatory)][string]$OutputPath,
    [string]$GitHubOutput
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$dll = (Resolve-Path -LiteralPath $DllPath).Path
$output = [IO.Path]::GetFullPath($OutputPath)
if ([string]::Equals($dll, $output, [StringComparison]::OrdinalIgnoreCase) -or
    ($GitHubOutput -and [string]::Equals($dll, [IO.Path]::GetFullPath($GitHubOutput), [StringComparison]::OrdinalIgnoreCase))) {
    throw 'Identity outputs must be separate from the selected DLL'
}
$before = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
if ($before -cne $ExpectedUnsignedSha256) { throw 'The selected unsigned DLL changed before identity preparation' }
$context = Get-Content -LiteralPath $ReleaseContextPath -Raw -Encoding utf8 | ConvertFrom-Json
if ($context.schema_version -cne 'stfc-mac-release-context/v1' -or
    $context.channel -cnotin @('pre', 'release') -or
    [string]$context.run_number -cnotmatch '\A[1-9][0-9]*\z' -or
    [string]$context.run_attempt -cnotmatch '\A[1-9][0-9]*\z') {
    throw 'Expected the verified signed Pre/Release CI context'
}
$version = [Diagnostics.FileVersionInfo]::GetVersionInfo($dll)
if (-not $version.FileVersion) { throw 'Selected DLL has no fixed version resource' }
$baseVersion = '{0}.{1}.{2}.{3}' -f $version.FileMajorPart, $version.FileMinorPart, $version.FileBuildPart, $version.FilePrivatePart
$arguments = @((Join-Path $PSScriptRoot 'mac-build-identity.py'), 'describe', '--base-version', $baseVersion,
    '--channel', $context.channel, '--source', $SourceSha, '--run-id', $BuildRunId,
    '--run-number', [string]$context.run_number, '--run-attempt', [string]$context.run_attempt,
    '--output', $output)
if ($context.release_tag) { $arguments += @('--release-tag', [string]$context.release_tag) }
& python @arguments | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Canonical display identity generation failed' }
$identity = Get-Content -LiteralPath $output -Raw -Encoding utf8 | ConvertFrom-Json
$label = if ($context.channel -ceq 'pre') { 'Pre' } else { 'Release' }
$expectedDisplay = if ($context.release_tag) {
    $tag = [string]$context.release_tag
    $tag.Substring(1) + ' · ' + $label
} else {
    $baseVersion + ' · Pre b' + [string]$context.run_number + '.' + [string]$context.run_attempt
}
if ($identity.STFCBaseVersion -cne $baseVersion -or $identity.STFCBuildChannel -cne $context.channel -or
    $identity.STFCSourceCommit -cne $SourceSha -or $identity.STFCBuildRunID -cne $BuildRunId -or
    [string]$identity.STFCBuildRunNumber -cne [string]$context.run_number -or
    [string]$identity.STFCBuildRunAttempt -cne [string]$context.run_attempt -or
    $identity.STFCReleaseTag -cne [string]$context.release_tag -or $identity.STFCDisplayVersion -cne $expectedDisplay) {
    throw 'Generated identity differs from the selected version and CI context'
}
if ((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant() -cne $before) {
    throw 'Identity preparation must not change any unsigned DLL bytes'
}
$description = 'STFC Community Mod | ' + $identity.STFCDisplayVersion
if ($GitHubOutput) {
    [IO.File]::AppendAllText([IO.Path]::GetFullPath($GitHubOutput), 'signing_description=' + $description + "`n",
        [Text.UTF8Encoding]::new($false))
}
Write-Output $description
