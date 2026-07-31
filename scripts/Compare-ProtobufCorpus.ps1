[CmdletBinding()]
param(
  [Parameter(Mandatory)]
  [string]$CandidatePath,

  [Parameter(Mandatory)]
  [string]$TrackedPath,

  [Parameter(Mandatory)]
  [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Normalize-Identifier([string]$Value) {
  return ($Value -replace '_', '').ToLowerInvariant()
}

function Get-MatchingBrace([string]$Text, [int]$OpenIndex) {
  $depth = 0
  for ($index = $OpenIndex; $index -lt $Text.Length; $index++) {
    switch ($Text[$index]) {
      '{' { $depth++ }
      '}' {
        $depth--
        if ($depth -eq 0) {
          return $index
        }
      }
    }
  }
  throw "Unbalanced protobuf declaration at character $OpenIndex"
}

function Get-ProtobufDefinitions([string]$Text, [string]$Source, [string]$ParentName = '') {
  $definitions = [System.Collections.Generic.List[object]]::new()
  $cursor = 0
  $depth = 0

  while ($cursor -lt $Text.Length) {
    if ($Text[$cursor] -eq '{') {
      $depth++
      $cursor++
      continue
    }
    if ($Text[$cursor] -eq '}') {
      $depth--
      $cursor++
      continue
    }

    if ($depth -eq 0) {
      $remaining = $Text.Substring($cursor)
      $match = [regex]::Match($remaining, '(?m)^[ \t]*(message|enum)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{')
      if ($match.Success) {
        $absoluteStart = $cursor + $match.Index
        for ($scan = $cursor; $scan -lt $absoluteStart; $scan++) {
          if ($Text[$scan] -eq '{') { $depth++ }
          elseif ($Text[$scan] -eq '}') { $depth-- }
        }
        if ($depth -ne 0) {
          $cursor = $absoluteStart + $match.Length
          continue
        }

        $openIndex = $absoluteStart + $match.Value.LastIndexOf('{')
        $closeIndex = Get-MatchingBrace $Text $openIndex
        $kind = $match.Groups[1].Value
        $name = $match.Groups[2].Value
        $qualifiedName = $(if ([string]::IsNullOrWhiteSpace($ParentName)) { $name } else { "$ParentName.$name" })
        $body = $Text.Substring($openIndex + 1, $closeIndex - $openIndex - 1)
        $definitions.Add([pscustomobject]@{
          Kind = $kind
          Name = $name
          QualifiedName = $qualifiedName
          Body = $body
          Source = $Source
        })
        if ($kind -eq 'message') {
          foreach ($nestedDefinition in @(Get-ProtobufDefinitions $body $Source $qualifiedName)) {
            $definitions.Add($nestedDefinition)
          }
        }
        $cursor = $closeIndex + 1
        continue
      }
    }

    $cursor++
  }

  return $definitions
}

function Get-MessageFields([string]$Body) {
  $fields = @{}
  $depth = 0
  $oneOfDepth = -1
  $oneOfName = $null
  $nestedDepth = -1

  foreach ($line in ($Body -split "`r?`n")) {
    $trimmed = ($line -replace '//.*$', '').Trim()
    if ($trimmed -match '^(message|enum)\s+[A-Za-z_][A-Za-z0-9_]*\s*\{') {
      $nestedDepth = $depth + 1
    } elseif ($nestedDepth -lt 0 -and $trimmed -match '^oneof\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{') {
      $oneOfName = Normalize-Identifier $matches[1]
      $oneOfDepth = $depth + 1
    }

    $fieldDepthAccepted = $nestedDepth -lt 0 -and
      ($depth -eq 0 -or ($oneOfDepth -gt 0 -and $depth -eq $oneOfDepth))
    if ($fieldDepthAccepted -and
        $trimmed -match '^(?:(optional|required|repeated)\s+)?(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(\d+)(?:\s*\[[^\]]+\])?\s*;') {
      $label = $(if ($matches[1]) { $matches[1].ToLowerInvariant() } else { 'singular' })
      $type = (($matches[2] -replace '\s+', '') -replace '^\.', '').ToLowerInvariant()
      $type = [regex]::Replace($type, '(?:\b[a-z_][a-z0-9_]*\.)+([a-z_][a-z0-9_]*)', '$1')
      $id = [int]$matches[4]
      $fields[$id] = [pscustomobject]@{
        Id = $id
        Name = Normalize-Identifier $matches[3]
        Type = $type
        Label = $label
        OneOf = $(if ($oneOfDepth -gt 0 -and $depth -eq $oneOfDepth) { $oneOfName } else { $null })
      }
    }

    $opens = ([regex]::Matches($trimmed, '\{')).Count
    $closes = ([regex]::Matches($trimmed, '\}')).Count
    $depth += $opens - $closes
    if ($nestedDepth -gt 0 -and $depth -lt $nestedDepth) {
      $nestedDepth = -1
    }
    if ($oneOfDepth -gt 0 -and $depth -lt $oneOfDepth) {
      $oneOfDepth = -1
      $oneOfName = $null
    }
  }

  return $fields
}

function Get-EnumValues([string]$Body) {
  $values = @{}
  foreach ($match in [regex]::Matches($Body, '(?m)^[ \t]*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(-?\d+)')) {
    $id = [int64]$match.Groups[2].Value
    $values[$id] = Normalize-Identifier $match.Groups[1].Value
  }
  return $values
}

function Get-Namespace([string]$Text) {
  $match = [regex]::Match($Text, 'option\s+csharp_namespace\s*=\s*"([^"]*)"\s*;')
  if ($match.Success) {
    return $match.Groups[1].Value
  }
  $package = [regex]::Match($Text, '(?m)^[ \t]*package\s+([^;]+)\s*;')
  if ($package.Success) {
    return $package.Groups[1].Value.Trim()
  }
  return ''
}

function Add-CorpusFile([hashtable]$Corpus, [System.IO.FileInfo]$File) {
  $text = Get-Content -LiteralPath $File.FullName -Raw
  $namespace = Get-Namespace $text
  foreach ($definition in Get-ProtobufDefinitions $text $File.FullName) {
    $key = "$(Normalize-Identifier $namespace).$(Normalize-Identifier $definition.QualifiedName)"
    $values = $(if ($definition.Kind -eq 'enum') {
      Get-EnumValues $definition.Body
    } else {
      Get-MessageFields $definition.Body
    })
    if ($Corpus.ContainsKey($key)) {
      throw "Duplicate protobuf declaration '$($definition.QualifiedName)' in namespace '$namespace'."
    }
    $Corpus[$key] = [pscustomobject]@{
      Namespace = $namespace
      Kind = $definition.Kind
      Name = $definition.QualifiedName
      SimpleName = $definition.Name
      Source = $definition.Source
      Complete = $definition.Body -notmatch '=\s*\?\s*;'
      Values = $values
    }
  }
}

$candidateCorpus = @{}
$trackedCorpus = @{}
Get-ChildItem -LiteralPath $CandidatePath -Filter '*.proto' -File | ForEach-Object {
  Add-CorpusFile $candidateCorpus $_
}
Get-ChildItem -LiteralPath $TrackedPath -Filter '*.proto' -File | ForEach-Object {
  Add-CorpusFile $trackedCorpus $_
}

$trackedNamespaces = @{}
foreach ($trackedType in $trackedCorpus.Values) {
  $trackedNamespaces[(Normalize-Identifier $trackedType.Namespace)] = $true
}
foreach ($key in @($candidateCorpus.Keys)) {
  if (-not $trackedNamespaces.ContainsKey((Normalize-Identifier $candidateCorpus[$key].Namespace))) {
    $candidateCorpus.Remove($key)
  }
}

$changes = [System.Collections.Generic.List[object]]::new()
$matchedTrackedKeys = @{}
foreach ($key in ($candidateCorpus.Keys | Sort-Object)) {
  $candidate = $candidateCorpus[$key]
  $trackedKey = $key
  $tracked = $(if ($trackedCorpus.ContainsKey($key)) {
    $trackedCorpus[$key]
  } else {
    $sameName = @($trackedCorpus.GetEnumerator() | Where-Object {
      (Normalize-Identifier $_.Value.Name) -eq (Normalize-Identifier $candidate.Name)
    })
    if ($sameName.Count -eq 1) {
      $trackedKey = $sameName[0].Key
      $sameName[0].Value
    } else {
      $null
    }
  })

  if ($null -eq $tracked) {
    $changes.Add([pscustomobject]@{
      Change = 'type-added'
      Namespace = $candidate.Namespace
      Type = $candidate.Name
      Detail = $candidate.Kind
    })
    continue
  }
  $matchedTrackedKeys[$trackedKey] = $true

  if ($candidate.Kind -ne $tracked.Kind) {
    $changes.Add([pscustomobject]@{
      Change = 'type-kind-changed'
      Namespace = $candidate.Namespace
      Type = $candidate.Name
      Detail = "$($tracked.Kind) -> $($candidate.Kind)"
    })
    continue
  }

  foreach ($id in ($candidate.Values.Keys | Sort-Object)) {
    if (-not $tracked.Values.ContainsKey($id)) {
      $changes.Add([pscustomobject]@{
        Change = $(if ($candidate.Kind -eq 'enum') { 'enum-value-added' } else { 'field-added' })
        Namespace = $candidate.Namespace
        Type = $candidate.Name
        Detail = "id=$id"
      })
      continue
    }

    $candidateValue = $candidate.Values[$id]
    $trackedValue = $tracked.Values[$id]
    if ($candidate.Kind -eq 'enum') {
      $enumName = Normalize-Identifier $candidate.SimpleName
      $candidateComparable = $(if ($candidateValue.StartsWith($enumName)) {
        $candidateValue.Substring($enumName.Length)
      } else {
        $candidateValue
      })
      $trackedComparable = $(if ($trackedValue.StartsWith($enumName)) {
        $trackedValue.Substring($enumName.Length)
      } else {
        $trackedValue
      })
      if ($candidateComparable -ne $trackedComparable) {
        $changes.Add([pscustomobject]@{
          Change = 'enum-value-renamed'
          Namespace = $candidate.Namespace
          Type = $candidate.Name
          Detail = "id=$id $trackedValue -> $candidateValue"
        })
      }
    } elseif ($candidateValue.Type -ne $trackedValue.Type -or
              $candidateValue.Label -ne $trackedValue.Label -or
              $candidateValue.OneOf -ne $trackedValue.OneOf) {
      $changes.Add([pscustomobject]@{
        Change = 'field-shape-changed'
        Namespace = $candidate.Namespace
        Type = $candidate.Name
        Detail = "id=$id $($trackedValue.Label) $($trackedValue.Type) -> $($candidateValue.Label) $($candidateValue.Type)"
      })
    } elseif ($candidateValue.Name -ne $trackedValue.Name) {
      $changes.Add([pscustomobject]@{
        Change = 'field-renamed'
        Namespace = $candidate.Namespace
        Type = $candidate.Name
        Detail = "id=$id $($trackedValue.Name) -> $($candidateValue.Name)"
      })
    }
  }

  if (-not $candidate.Complete) {
    $changes.Add([pscustomobject]@{
      Change = 'type-incomplete'
      Namespace = $candidate.Namespace
      Type = $candidate.Name
      Detail = 'One or more field numbers were unavailable in IL2CPP metadata; removals were suppressed.'
    })
    continue
  }

  foreach ($id in ($tracked.Values.Keys | Sort-Object)) {
    if ($candidate.Values.ContainsKey($id)) {
      continue
    }
    if ($candidate.Kind -eq 'enum' -and $id -eq 0 -and
        $tracked.Values[$id] -eq "$(Normalize-Identifier $tracked.SimpleName)none") {
      continue
    }
    $changes.Add([pscustomobject]@{
      Change = $(if ($candidate.Kind -eq 'enum') { 'enum-value-not-emitted' } else { 'field-not-emitted' })
      Namespace = $candidate.Namespace
      Type = $candidate.Name
      Detail = "id=$id; absence may reflect inheritance or unreachable generated members"
    })
  }
}

foreach ($key in ($trackedCorpus.Keys | Sort-Object)) {
  if (-not $matchedTrackedKeys.ContainsKey($key)) {
    $tracked = $trackedCorpus[$key]
    $changes.Add([pscustomobject]@{
      Change = 'type-not-emitted'
      Namespace = $tracked.Namespace
      Type = $tracked.Name
      Detail = 'Retained schema type was not reachable from current generated messages.'
    })
  }
}

$report = [ordered]@{
  generatedAtUtc = [DateTime]::UtcNow.ToString('o')
  candidatePath = [System.IO.Path]::GetFullPath($CandidatePath)
  trackedPath = [System.IO.Path]::GetFullPath($TrackedPath)
  candidateTypeCount = $candidateCorpus.Count
  trackedTypeCount = $trackedCorpus.Count
  changeCount = $changes.Count
  actionableChangeCount = @($changes | Where-Object Change -notin @(
    'field-not-emitted',
    'enum-value-not-emitted',
    'type-incomplete',
    'type-not-emitted'
  )).Count
  changesByKind = [ordered]@{}
  changes = $changes
}
foreach ($group in ($changes | Group-Object Change | Sort-Object Name)) {
  $report.changesByKind[$group.Name] = $group.Count
}

$reportDirectory = Split-Path -Parent $ReportPath
if ($reportDirectory) {
  New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM

Write-Host "Compared $($candidateCorpus.Count) extracted types with $($trackedCorpus.Count) tracked types."
Write-Host "Detected $($report.actionableChangeCount) actionable changes and $($changes.Count - $report.actionableChangeCount) retention observations."
$report.changesByKind.GetEnumerator() | ForEach-Object {
  Write-Host ("  {0}: {1}" -f $_.Key, $_.Value)
}
