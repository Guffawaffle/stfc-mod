# Run locally as a repository administrator after reviewing this script.
# Creates one read-only AAI deploy key and stores its private half only in the
# branch/tag-restricted Actions environment. Never prints the private key.
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$modRepository = 'Guffawaffle/stfc-mod'
$sourceRepository = 'Guffawaffle/stfc-aai'
$environmentName = 'stfc-dev-source'
$temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ('stfc-dev-source-' + [guid]::NewGuid())
$keyId = $null
$stored = $false

try {
    # Refuse to silently replace an existing environment's restrictions or key.
    $environments = gh api "repos/$modRepository/environments" | ConvertFrom-Json
    if ($environments.environments.name -contains $environmentName) {
        throw "$environmentName already exists. Inspect its restrictions and secret before changing it."
    }
    New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
    $keyPath = Join-Path $temporaryDirectory 'aai-source'
    ssh-keygen -t ed25519 -N '' -C 'stfc-mod tagged DEV read-only source' -f $keyPath -q
    $keyBody = @{
        title = 'stfc-mod tagged DEV source (read only)'
        key = (Get-Content -LiteralPath ($keyPath + '.pub') -Raw).Trim()
        read_only = $true
    } | ConvertTo-Json -Compress
    $keyResult = $keyBody | gh api --method POST "repos/$sourceRepository/keys" --input - | ConvertFrom-Json
    $keyId = $keyResult.id
    if (-not $keyResult.read_only) { throw 'GitHub did not create a read-only deploy key.' }

    '{"deployment_branch_policy":{"protected_branches":false,"custom_branch_policies":true}}' |
        gh api --method PUT "repos/$modRepository/environments/$environmentName" --input - | Out-Null
    foreach ($policy in @(@{name='play-dev'; type='branch'}, @{name='v*-dev.*'; type='tag'})) {
        $policy | ConvertTo-Json -Compress |
            gh api --method POST "repos/$modRepository/environments/$environmentName/deployment-branch-policies" --input - | Out-Null
    }
    Get-Content -LiteralPath $keyPath -Raw |
        gh secret set AAI_SOURCE_READ_KEY --repo $modRepository --env $environmentName
    $stored = $true
    Write-Host "Configured read-only AAI deploy key $keyId and restricted environment $environmentName."
} finally {
    if ($keyId -and -not $stored) {
        gh api --method DELETE "repos/$sourceRepository/keys/$keyId" | Out-Null
    }
    # Only remove the exact freshly created temporary directory.
    if (Test-Path -LiteralPath $temporaryDirectory) {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
