[CmdletBinding(PositionalBinding = $false)]
param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [object[]]$ForwardArgs
)

$ErrorActionPreference = "Stop"

$publicRoot = Split-Path -Parent $PSCommandPath
$repoRoot = Split-Path -Parent $publicRoot
$privateRoot = Join-Path $repoRoot ".ax-priv"
$privateScript = Join-Path $privateRoot "ax.ps1"

if (Test-Path -LiteralPath $privateScript) {
  & $privateScript @ForwardArgs
  exit $LASTEXITCODE
}

$requestedCommand = if ($ForwardArgs.Count -gt 0) { [string]$ForwardArgs[0] } else { "" }
$normalizedCommand = $requestedCommand.ToLowerInvariant()
$helpCommands = @("", "help", "--help", "-help", "-h", "-?", "/?")

if ($helpCommands -contains $normalizedCommand) {
  @(
    "Public .ax facade for D:\dev\stfc-mod",
    "",
    "This tracked wrapper delegates to .ax-priv\ax.ps1 when the private AX repo is present.",
    "AXF imports this path from axf.workspace.json and exposes repo-owned capabilities from manifests\.",
    "",
    "Repo pointers:",
    "  .ax\README.md",
    "  axf.workspace.json",
    "  scripts\axf\README.md",
    "",
    "Private implementation expected at:",
    "  $privateScript"
  ) -join [Environment]::NewLine | Write-Output
  exit 0
}

$payload = [ordered]@{
  ok = $false
  error = [ordered]@{
    message = "Private AX implementation not found."
  }
  data = [ordered]@{
    requestedCommand = if ($requestedCommand) { $requestedCommand } else { $null }
    publicFacadePath = $PSCommandPath
    privateAxRoot = $privateRoot
    privateAxScript = $privateScript
    axfWorkspace = Join-Path $repoRoot "axf.workspace.json"
    manifestsRoot = Join-Path $repoRoot "manifests"
    docs = @(
      Join-Path $publicRoot "README.md"
      Join-Path $repoRoot "scripts\axf\README.md"
    )
  }
  hints = @(
    "This tracked .ax folder is a public AXF facade, not the private implementation.",
    "Restore or clone the private AX repo to .ax-priv to execute local AX commands.",
    "When AXF is available, inspect and run the repo-owned global.stfc-mod.* capabilities instead of calling the facade directly."
  )
}

$payload | ConvertTo-Json -Depth 6 | Write-Output
exit 1
