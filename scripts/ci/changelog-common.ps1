$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Shared changelog ledger primitives.
#
# Both scripts/ci/check-changelog.ps1 and scripts/release.ps1 read
# docs/changelog.md, and release.ps1 rewrites it. When each carried its own
# copy of these helpers the copies drifted, and the row splitters disagreed
# about a cell containing an unescaped pipe: the validator accepted a row with
# too many columns while the stamper rejoined it into different text, silently
# altering published copy nobody wrote.
#
# Only the parts that are genuinely common live here -- the row splitter, the
# column-count rule, the cell reader, and the vocabulary. Writing a cell back
# out is release.ps1's concern alone and stays there.

$script:ChangelogRequiredColumns = @("Release", "Category", "Magnitude", "Summary", "Details")

$script:ChangelogAllowedCategories = @(
    "Player Experience",
    "Competitive Play",
    "Server Hosting",
    "Gameplay and Balance",
    "Maps and Content",
    "Fixes",
    "Documentation and Packaging",
    "Internal Maintenance"
)

$script:ChangelogAllowedMagnitudes = @("major", "minor", "patch")

function Get-ChangelogRequiredColumns {
    return $script:ChangelogRequiredColumns
}

function Get-ChangelogAllowedCategories {
    return $script:ChangelogAllowedCategories
}

function Get-ChangelogAllowedMagnitudes {
    return $script:ChangelogAllowedMagnitudes
}

function Split-MarkdownTableRow {
    # Split on unescaped pipes only, so a cell may contain a literal \| --
    # which is what a C++ || inside a code span needs.
    param([string]$Line)

    $inner = $Line.Trim()
    if ($inner.StartsWith("|")) {
        $inner = $inner.Substring(1)
    }
    if ($inner.EndsWith("|")) {
        $inner = $inner.Substring(0, $inner.Length - 1)
    }

    return @([regex]::Split($inner, '(?<!\\)\|') | ForEach-Object {
        ($_ -replace '\\\|', '|').Trim()
    })
}

function Assert-ChangelogRowShape {
    # One place decides what a malformed row is. A row with the wrong column
    # count must be rejected by every reader, because the one that accepts it
    # is the one that corrupts it.
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]]$Cells,
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]]$Headers,
        [Parameter(Mandatory)][int]$LineNumber
    )

    if ($Cells.Count -ne $Headers.Count) {
        throw "Changelog row $LineNumber has $($Cells.Count) columns, but the table header has $($Headers.Count). Escape any literal pipe inside a cell as \|, including one inside a code span."
    }
}

function ConvertFrom-ChangelogTableCell {
    # Flatten a cell to comparable prose for validation and duplicate checks.
    param([AllowNull()][string]$Text)

    if ($null -eq $Text) {
        return ""
    }

    return (($Text -replace '<br\s*/?>', ' ') -replace '\s+', ' ').Trim()
}

# check-changelog.ps1 knew this function under a shorter name. Kept as an
# alias rather than renamed at the call sites, so the two scripts share one
# implementation without a rename rippling through the validator.
Set-Alias -Name ConvertFrom-ChangelogCell -Value ConvertFrom-ChangelogTableCell
