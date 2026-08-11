[CmdletBinding()]
param(
    [string]$ChangedSince
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location -LiteralPath $repoRoot

. (Join-Path $PSScriptRoot "changelog-common.ps1")

$ledgerPath = Join-Path $repoRoot "docs/changelog.md"
$requiredColumns = Get-ChangelogRequiredColumns
$allowedCategories = Get-ChangelogAllowedCategories
$allowedMagnitudes = Get-ChangelogAllowedMagnitudes

function Test-ImplementationPath {
    param([string]$Path)

    $normalised = $Path -replace '\\', '/'
    if ($normalised -eq "docs/changelog.md") {
        return $false
    }
    if ($normalised -match '^(src|updater|scripts|packaging|docs|\.github/workflows)/') {
        return $true
    }
    if ($normalised -in @("README.md", "VERSION", "THIRD_PARTY_NOTICES.md")) {
        return $true
    }

    return $false
}

if (-not (Test-Path -LiteralPath $ledgerPath -PathType Leaf)) {
    throw "Central changelog ledger is missing: docs/changelog.md"
}

$lines = @(Get-Content -LiteralPath $ledgerPath)
$headerIndex = -1
$headers = @()
for ($i = 0; $i -lt $lines.Count; $i++) {
    if (-not $lines[$i].TrimStart().StartsWith("|")) {
        continue
    }

    $candidateHeaders = Split-MarkdownTableRow $lines[$i]
    $missing = @($requiredColumns | Where-Object { $candidateHeaders -notcontains $_ })
    if ($missing.Count -eq 0) {
        $headerIndex = $i
        $headers = $candidateHeaders
        break
    }
}

if ($headerIndex -lt 0) {
    throw "docs/changelog.md must contain a Markdown table with columns: $($requiredColumns -join ', ')."
}
if ($headerIndex + 1 -ge $lines.Count -or $lines[$headerIndex + 1] -notmatch '^\s*\|?\s*:?-{3,}:?\s*\|') {
    throw "docs/changelog.md table is missing the separator row after the header."
}

$columnIndex = @{}
for ($i = 0; $i -lt $headers.Count; $i++) {
    $columnIndex[$headers[$i]] = $i
}

$entries = New-Object System.Collections.Generic.List[object]
for ($i = $headerIndex + 2; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    if ([string]::IsNullOrWhiteSpace($line) -or -not $line.TrimStart().StartsWith("|")) {
        break
    }

    $cells = Split-MarkdownTableRow $line
    Assert-ChangelogRowShape -Cells $cells -Headers $headers -LineNumber ($i + 1)

    $release = ConvertFrom-ChangelogCell $cells[$columnIndex["Release"]]
    $category = ConvertFrom-ChangelogCell $cells[$columnIndex["Category"]]
    $magnitude = (ConvertFrom-ChangelogCell $cells[$columnIndex["Magnitude"]]).ToLowerInvariant()
    $summary = ConvertFrom-ChangelogCell $cells[$columnIndex["Summary"]]
    $details = ConvertFrom-ChangelogCell $cells[$columnIndex["Details"]]

    if ([string]::IsNullOrWhiteSpace($release)) {
        $release = "Unreleased"
    }
    if ($release -ine "Unreleased" -and $release -notmatch '^v?\d+\.\d+\.\d+$') {
        throw "Changelog row $($i + 1) release must be Unreleased or a semantic version."
    }
    if (-not ($allowedCategories | Where-Object { $_ -ieq $category })) {
        throw "Changelog row $($i + 1) has invalid category '$category'."
    }
    if ($allowedMagnitudes -notcontains $magnitude) {
        throw "Changelog row $($i + 1) has invalid magnitude '$magnitude'."
    }
    if ([string]::IsNullOrWhiteSpace($summary)) {
        throw "Changelog row $($i + 1) is missing a summary."
    }
    if ($details.Length -lt 30) {
        throw "Changelog row $($i + 1) needs a detailed description of at least 30 characters."
    }

    $entries.Add([pscustomobject]@{
        Release = $release
        Category = $category
        Magnitude = $magnitude
        Summary = $summary
        Details = $details
        LineNumber = $i + 1
    })
}

if ($entries.Count -eq 0) {
    throw "docs/changelog.md does not contain any change rows."
}

$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $entries) {
    $key = "$($entry.Release)|$($entry.Category)|$($entry.Summary)"
    if (-not $seen.Add($key)) {
        throw "Duplicate changelog summary '$($entry.Summary)' for $($entry.Release) / $($entry.Category). Merge related rows."
    }
}

$comparisonRef = $ChangedSince
if ([string]::IsNullOrWhiteSpace($comparisonRef) -and $env:GITHUB_BASE_REF) {
    $baseRef = "refs/remotes/origin/$($env:GITHUB_BASE_REF)"
    $refSpec = "+refs/heads/$($env:GITHUB_BASE_REF):$baseRef"
    git fetch --no-tags --depth=1 origin $refSpec
    if ($LASTEXITCODE -ne 0) {
        throw "Could not fetch changelog comparison ref $baseRef."
    }
    $comparisonRef = $baseRef
}

if (-not [string]::IsNullOrWhiteSpace($comparisonRef) -and $comparisonRef -match '^0+$') {
    throw "Changelog comparison ref is an all-zero new-branch sentinel. Resolve a real merge base before running this gate."
}

if (-not [string]::IsNullOrWhiteSpace($comparisonRef)) {
    git rev-parse --verify --quiet "$comparisonRef^{commit}" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Changelog comparison ref is not available: $comparisonRef"
    }

    $diffFiles = @(git diff --name-only --diff-filter=ACMRT $comparisonRef HEAD)
    if ($LASTEXITCODE -ne 0) {
        throw "Could not compare changelog coverage against $comparisonRef."
    }
    $implementationFiles = @($diffFiles | Where-Object { Test-ImplementationPath $_ })
    if ($implementationFiles.Count -gt 0) {
        $changedChangelog = @($diffFiles | Where-Object { ($_ -replace '\\', '/') -eq "docs/changelog.md" }).Count -gt 0
        if (-not $changedChangelog) {
            throw "Implementation-facing files changed without docs/changelog.md. Add or update a grouped Unreleased changelog row."
        }

        $unreleasedRows = @($entries | Where-Object { $_.Release -ieq "Unreleased" })
        if ($unreleasedRows.Count -eq 0) {
            throw "docs/changelog.md changed, but no Unreleased rows are present for this implementation."
        }
    }
}

Write-Host "Central changelog ledger is valid."
