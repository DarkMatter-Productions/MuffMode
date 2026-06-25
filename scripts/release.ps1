[CmdletBinding()]
param(
    [ValidateSet("auto", "major", "minor", "patch", "latch")]
    [string]$VersionMode = "auto",

    [string]$Version,
    [string]$PreviousTag,

    [ValidateSet("alpha", "beta", "rc", "stable")]
    [string]$Channel = "beta",

    [string]$AssetRoot = "packaging/release-assets",
    [string]$OutputRoot = "dist/release",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$UpdaterProject = "updater/MuffMode.Updater/MuffMode.Updater.csproj",
    [string]$UpdaterRuntime = "win-x64",
    [string]$InstallerScript = "packaging/installer/muffmode-installer.iss",
    [string]$InnoSetupCompiler,
    [string]$ChangelogPath = "docs/changelog.md",
    [AllowEmptyString()][string]$ReleaseIntro,

    [switch]$SkipBuild,
    [switch]$SkipUpdaterBuild,
    [switch]$SkipInstaller,
    [switch]$UpdateVersionFiles,
    [switch]$VersionOnly,
    [switch]$CreateGitHubRelease,
    [switch]$Prerelease,
    [switch]$RequireCopilot,
    [switch]$NoIntroPrompt,
    [switch]$AllowDirtyPackage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$ReleaseRepo = "DarkMatter-Productions/MuffMode"
$script:ChangelogRequiredColumns = @("Release", "Category", "Magnitude", "Summary", "Details")
$script:ChangelogCategories = @(
    "Player Experience",
    "Competitive Play",
    "Server Hosting",
    "Gameplay and Balance",
    "Maps and Content",
    "Fixes",
    "Documentation and Packaging",
    "Internal Maintenance"
)
$script:ChangelogMagnitudes = @("major", "minor", "patch")

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Resolve-RepoPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return (Join-Path $RepoRoot $Path)
}

function Resolve-FullPath {
    param([string]$Path)
    return [System.IO.Path]::GetFullPath((Resolve-RepoPath $Path))
}

function Assert-PathUnderDirectory {
    param(
        [string]$Path,
        [string]$ParentPath,
        [string]$Description
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $fullParent = [System.IO.Path]::GetFullPath($ParentPath)
    $separator = [System.IO.Path]::DirectorySeparatorChar.ToString()
    $parentPrefix = if ($fullParent.EndsWith($separator)) {
        $fullParent
    }
    else {
        "$fullParent$separator"
    }

    if (-not $fullPath.StartsWith($parentPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay under '$fullParent', but resolved to '$fullPath'."
    }
}

function Assert-Command {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' was not found on PATH."
    }
}

function Test-GitRevisionExists {
    param([string]$Revision)

    git -C $RepoRoot rev-parse "$Revision^{commit}" *> $null
    return $LASTEXITCODE -eq 0
}

function Remove-AnsiSequences {
    param([string]$Text)
    return ($Text -replace "`e\[[0-9;?]*[ -/]*[@-~]", "")
}

function Limit-Text {
    param(
        [AllowNull()][string]$Text,
        [int]$MaxCharacters = 60000
    )

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return ""
    }

    if ($Text.Length -le $MaxCharacters) {
        return $Text.Trim()
    }

    return "$($Text.Substring(0, $MaxCharacters).TrimEnd())`n`n[Context truncated after $MaxCharacters characters.]"
}

function Split-MarkdownTableRow {
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

function ConvertTo-ChangelogTableCell {
    param([AllowNull()][string]$Text)

    if ($null -eq $Text) {
        return ""
    }

    return (($Text -replace '\r?\n', "<br>").Trim() -replace '\|', '\|')
}

function ConvertFrom-ChangelogTableCell {
    param([AllowNull()][string]$Text)

    if ($null -eq $Text) {
        return ""
    }

    return (($Text -replace '<br\s*/?>', ' ') -replace '\s+', ' ').Trim()
}

function Get-CanonicalChangelogRelease {
    param([AllowNull()][string]$Release)

    $value = ConvertFrom-ChangelogTableCell $Release
    if ([string]::IsNullOrWhiteSpace($value) -or $value -ieq "Unreleased") {
        return "Unreleased"
    }

    if ($value -match '^v?(\d+\.\d+\.\d+)$') {
        return $Matches[1]
    }

    throw "Changelog release value '$value' must be 'Unreleased' or a semantic version such as 0.36.21."
}

function Get-CanonicalChangelogCategory {
    param([string]$Category)

    $value = ConvertFrom-ChangelogTableCell $Category
    foreach ($allowedCategory in $script:ChangelogCategories) {
        if ($allowedCategory -ieq $value) {
            return $allowedCategory
        }
    }

    throw "Changelog category '$value' is not valid. Use one of: $($script:ChangelogCategories -join ', ')."
}

function Get-CanonicalChangelogMagnitude {
    param([string]$Magnitude)

    $value = (ConvertFrom-ChangelogTableCell $Magnitude).ToLowerInvariant()
    if ($script:ChangelogMagnitudes -contains $value) {
        return $value
    }

    throw "Changelog magnitude '$value' is not valid. Use one of: $($script:ChangelogMagnitudes -join ', ')."
}

function ConvertFrom-ChangelogLedger {
    param([string]$Path = $ChangelogPath)

    $ledgerPath = Resolve-RepoPath $Path
    if (-not (Test-Path -LiteralPath $ledgerPath -PathType Leaf)) {
        throw "Central changelog ledger was not found: $ledgerPath"
    }

    $lines = @(Get-Content -LiteralPath $ledgerPath)
    $headerIndex = -1
    $headers = @()
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].TrimStart().StartsWith("|")) {
            $candidateHeaders = Split-MarkdownTableRow $lines[$i]
            $missing = @($script:ChangelogRequiredColumns | Where-Object { $candidateHeaders -notcontains $_ })
            if ($missing.Count -eq 0) {
                $headerIndex = $i
                $headers = $candidateHeaders
                break
            }
        }
    }

    if ($headerIndex -lt 0) {
        throw "Central changelog ledger must contain a Markdown table with columns: $($script:ChangelogRequiredColumns -join ', ')."
    }

    if ($headerIndex + 1 -ge $lines.Count -or $lines[$headerIndex + 1] -notmatch '^\s*\|?\s*:?-{3,}:?\s*\|') {
        throw "Central changelog ledger table is missing the Markdown separator row after the header."
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
        if ($cells.Count -lt $headers.Count) {
            throw "Changelog row $($i + 1) has $($cells.Count) columns, but the table header has $($headers.Count)."
        }

        $summary = ConvertFrom-ChangelogTableCell $cells[$columnIndex["Summary"]]
        $details = ConvertFrom-ChangelogTableCell $cells[$columnIndex["Details"]]
        if ([string]::IsNullOrWhiteSpace($summary)) {
            throw "Changelog row $($i + 1) is missing a summary."
        }
        if ($details.Length -lt 30) {
            throw "Changelog row $($i + 1) needs a detailed description of at least 30 characters."
        }

        $entries.Add([pscustomobject]@{
            Release = Get-CanonicalChangelogRelease $cells[$columnIndex["Release"]]
            Category = Get-CanonicalChangelogCategory $cells[$columnIndex["Category"]]
            Magnitude = Get-CanonicalChangelogMagnitude $cells[$columnIndex["Magnitude"]]
            Summary = $summary
            Details = $details
            LineNumber = $i + 1
        })
    }

    if ($entries.Count -eq 0) {
        throw "Central changelog ledger does not contain any change rows."
    }

    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $entries) {
        $key = "$($entry.Release)|$($entry.Category)|$($entry.Summary)"
        if (-not $seen.Add($key)) {
            throw "Duplicate changelog summary '$($entry.Summary)' for $($entry.Release) / $($entry.Category). Merge related rows into one detailed change."
        }
    }

    return @($entries | ForEach-Object { $_ })
}

function Get-ChangelogEntriesForRelease {
    param([AllowNull()][string]$TargetVersion)

    $entries = @(ConvertFrom-ChangelogLedger -Path $ChangelogPath)
    if ([string]::IsNullOrWhiteSpace($TargetVersion)) {
        return @($entries | Where-Object { $_.Release -eq "Unreleased" })
    }

    $target = (Get-CanonicalChangelogRelease $TargetVersion)
    $targetEntries = @($entries | Where-Object { $_.Release -eq $target })
    if ($targetEntries.Count -gt 0) {
        return $targetEntries
    }

    return @($entries | Where-Object { $_.Release -eq "Unreleased" })
}

function Update-ChangelogReleaseVersions {
    param(
        [string]$TargetVersion,
        [string]$Path = $ChangelogPath
    )

    $ledgerPath = Resolve-RepoPath $Path
    $lines = @(Get-Content -LiteralPath $ledgerPath)
    $headerIndex = -1
    $headers = @()
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].TrimStart().StartsWith("|")) {
            $candidateHeaders = Split-MarkdownTableRow $lines[$i]
            $missing = @($script:ChangelogRequiredColumns | Where-Object { $candidateHeaders -notcontains $_ })
            if ($missing.Count -eq 0) {
                $headerIndex = $i
                $headers = $candidateHeaders
                break
            }
        }
    }

    if ($headerIndex -lt 0) {
        throw "Central changelog ledger must contain the canonical Markdown table before release versions can be stamped."
    }

    $releaseColumn = [array]::IndexOf($headers, "Release")
    $updatedRows = 0
    for ($i = $headerIndex + 2; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ([string]::IsNullOrWhiteSpace($line) -or -not $line.TrimStart().StartsWith("|")) {
            break
        }

        $cells = Split-MarkdownTableRow $line
        if ($cells.Count -lt $headers.Count) {
            throw "Changelog row $($i + 1) has $($cells.Count) columns, but the table header has $($headers.Count)."
        }

        $release = Get-CanonicalChangelogRelease $cells[$releaseColumn]
        if ($release -eq "Unreleased") {
            $cells[$releaseColumn] = $TargetVersion
            $escapedCells = @($cells | ForEach-Object { ConvertTo-ChangelogTableCell $_ })
            $lines[$i] = "| $($escapedCells -join ' | ') |"
            $updatedRows++
        }
    }

    if ($updatedRows -gt 0) {
        Set-Content -LiteralPath $ledgerPath -Value ($lines -join "`n") -Encoding utf8
        Write-Step "Stamped $updatedRows changelog row(s) with release version $TargetVersion"
    }
    else {
        Write-Warning "No Unreleased changelog rows were found to stamp with $TargetVersion."
    }
}

function Get-ChangelogMagnitudeRank {
    param([string]$Magnitude)

    switch ($Magnitude) {
        "major" { return 0 }
        "minor" { return 1 }
        "patch" { return 2 }
        default { return 3 }
    }
}

function Select-HighlightChangelogEntries {
    param([object[]]$Entries)

    $majorEntries = @($Entries | Where-Object { $_.Magnitude -eq "major" })
    if ($majorEntries.Count -gt 0) {
        return @($majorEntries)
    }

    $visibleEntries = @($Entries | Where-Object { $_.Category -ne "Internal Maintenance" })
    if ($visibleEntries.Count -eq 0) {
        $visibleEntries = @($Entries)
    }

    return @($visibleEntries |
        Sort-Object @{ Expression = { Get-ChangelogMagnitudeRank -Magnitude $_.Magnitude } }, Category, Summary |
        Select-Object -First 3)
}

function Get-PlainReleaseText {
    param(
        [AllowNull()][string]$Text,
        [int]$MaxCharacters = 180
    )

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return ""
    }

    $plain = $Text `
        -replace '\[([^\]]+)\]\([^)]+\)', '$1' `
        -replace '`([^`]+)`', '$1' `
        -replace '\*\*([^*]+)\*\*', '$1' `
        -replace '\s+', ' '
    $plain = $plain.Trim()
    if ($plain.Length -le $MaxCharacters) {
        return $plain
    }

    $cut = $plain.Substring(0, $MaxCharacters - 3).TrimEnd()
    $lastSpace = $cut.LastIndexOf(" ")
    if ($lastSpace -gt [int]($MaxCharacters * 0.6)) {
        $cut = $cut.Substring(0, $lastSpace).TrimEnd()
    }

    return "$cut..."
}

function New-ReleaseIntroMessage {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [object[]]$Entries
    )

    $channelName = Get-ChannelDisplayName -Channel $Channel
    $label = if ($channelName) { "Muff Mode v$TargetVersion $channelName" } else { "Muff Mode v$TargetVersion" }
    $headlineEntries = @(Select-HighlightChangelogEntries -Entries $Entries)
    $headline = $headlineEntries | Select-Object -First 1
    if ($headline) {
        $summary = Get-PlainReleaseText -Text $headline.Summary -MaxCharacters 120
        $details = Get-PlainReleaseText -Text $headline.Details -MaxCharacters 180
        return "$label is live. The headline change is ${summary}: $details"
    }

    return "$label is live with a focused set of fixes and release polish. Grab the build, try it in real matches, and send feedback from actual server play."
}

function Test-CanShowReleaseIntroDialog {
    if ($NoIntroPrompt) {
        return $false
    }
    if ($env:GITHUB_ACTIONS -or $env:CI) {
        return $false
    }
    if ($env:OS -ne "Windows_NT") {
        return $false
    }

    return [System.Environment]::UserInteractive
}

function Show-ReleaseIntroDialog {
    param([string]$SuggestedIntro)

    try {
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing

        $form = New-Object System.Windows.Forms.Form
        $form.Text = "Muff Mode Release Intro"
        $form.Size = New-Object System.Drawing.Size(640, 360)
        $form.StartPosition = "CenterScreen"
        $form.MinimizeBox = $false
        $form.MaximizeBox = $false

        $label = New-Object System.Windows.Forms.Label
        $label.Location = New-Object System.Drawing.Point(12, 12)
        $label.Size = New-Object System.Drawing.Size(600, 42)
        $label.Text = "Edit the Discord/release intro, or leave the generated text unchanged."
        $form.Controls.Add($label)

        $textBox = New-Object System.Windows.Forms.TextBox
        $textBox.Location = New-Object System.Drawing.Point(12, 58)
        $textBox.Size = New-Object System.Drawing.Size(600, 190)
        $textBox.Multiline = $true
        $textBox.ScrollBars = "Vertical"
        $textBox.Text = $SuggestedIntro
        $form.Controls.Add($textBox)

        $okButton = New-Object System.Windows.Forms.Button
        $okButton.Location = New-Object System.Drawing.Point(412, 266)
        $okButton.Size = New-Object System.Drawing.Size(95, 30)
        $okButton.Text = "Use Intro"
        $okButton.DialogResult = [System.Windows.Forms.DialogResult]::OK
        $form.Controls.Add($okButton)

        $generatedButton = New-Object System.Windows.Forms.Button
        $generatedButton.Location = New-Object System.Drawing.Point(517, 266)
        $generatedButton.Size = New-Object System.Drawing.Size(95, 30)
        $generatedButton.Text = "Generated"
        $generatedButton.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
        $form.Controls.Add($generatedButton)

        $form.AcceptButton = $okButton
        $form.CancelButton = $generatedButton
        $result = $form.ShowDialog()
        if ($result -eq [System.Windows.Forms.DialogResult]::OK -and -not [string]::IsNullOrWhiteSpace($textBox.Text)) {
            return $textBox.Text.Trim()
        }
    }
    catch {
        Write-Warning "Could not show release intro dialog; using generated intro. $($_.Exception.Message)"
    }

    return $null
}

function Resolve-ReleaseIntroMessage {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [object[]]$Entries,
        [AllowNull()][string]$ManualIntro
    )

    if (-not [string]::IsNullOrWhiteSpace($ManualIntro)) {
        return $ManualIntro.Trim()
    }
    if (-not [string]::IsNullOrWhiteSpace($env:MUFFMODE_RELEASE_INTRO)) {
        return $env:MUFFMODE_RELEASE_INTRO.Trim()
    }

    $generated = New-ReleaseIntroMessage -TargetVersion $TargetVersion -Channel $Channel -Entries $Entries
    if (Test-CanShowReleaseIntroDialog) {
        $dialogIntro = Show-ReleaseIntroDialog -SuggestedIntro $generated
        if (-not [string]::IsNullOrWhiteSpace($dialogIntro)) {
            return $dialogIntro
        }
    }

    return $generated
}

function Format-ChangelogBullet {
    param([object]$Entry)

    return "- **$($Entry.Summary)** _($($Entry.Magnitude))_ - $($Entry.Details)"
}

function New-LedgerReleaseChangelog {
    param(
        [string]$TargetVersion,
        [string]$PreviousTag,
        [string]$Channel,
        [string]$OutputPath,
        [object[]]$Entries,
        [string]$Intro
    )

    $compareUrl = "https://github.com/$ReleaseRepo/compare/$PreviousTag...v$TargetVersion"
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "Muff Mode v$TargetVersion $channelName" } else { "Muff Mode v$TargetVersion" }
    $majorHighlights = @($Entries | Where-Object { $_.Magnitude -eq "major" })
    $highlightEntries = @(Select-HighlightChangelogEntries -Entries $Entries)
    $usingFallbackHighlights = $majorHighlights.Count -eq 0

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# $releaseLabel Changelog")
    $lines.Add("")
    if (-not [string]::IsNullOrWhiteSpace($Intro)) {
        $lines.Add($Intro.Trim())
        $lines.Add("")
    }
    $lines.Add("These are the centrally logged changes since $PreviousTag for the $Channel release channel.")
    $lines.Add("")
    $lines.Add("Compare: [$PreviousTag...v$TargetVersion]($compareUrl)")

    if ($highlightEntries.Count -gt 0) {
        $lines.Add("")
        $lines.Add("## Highlights")
        if ($usingFallbackHighlights) {
            $lines.Add("")
            $lines.Add("_No major changes are logged for this release, so these highlights call out the most relevant smaller changes._")
        }
        foreach ($entry in $highlightEntries) {
            $lines.Add((Format-ChangelogBullet $entry))
        }
    }

    foreach ($category in $script:ChangelogCategories) {
        $categoryEntries = @($Entries | Where-Object { $_.Category -eq $category })
        if ($categoryEntries.Count -eq 0) {
            continue
        }

        $lines.Add("")
        $lines.Add("## $category")
        foreach ($entry in ($categoryEntries | Sort-Object @{ Expression = { Get-ChangelogMagnitudeRank -Magnitude $_.Magnitude } }, Summary)) {
            $lines.Add((Format-ChangelogBullet $entry))
        }
    }

    Set-Content -LiteralPath $OutputPath -Value ($lines -join "`n") -Encoding utf8
}

function Invoke-GitHubCopilot {
    param(
        [string]$Prompt,
        [string]$Purpose,
        [string[]]$AllowedTools = @()
    )

    Assert-Command "copilot"

    $promptDir = Join-Path (Resolve-RepoPath $OutputRoot) "copilot-prompts"
    New-Item -ItemType Directory -Force -Path $promptDir | Out-Null

    $safePurpose = ($Purpose.ToLowerInvariant() -replace '[^a-z0-9]+', '-').Trim('-')
    if ([string]::IsNullOrWhiteSpace($safePurpose)) {
        $safePurpose = "release"
    }

    $promptPath = Join-Path $promptDir ("{0}-{1}.md" -f (Get-Date -Format "yyyyMMddHHmmssfff"), $safePurpose)
    Set-Content -LiteralPath $promptPath -Value $Prompt -Encoding utf8

    $driverPrompt = @"
Read the complete release automation prompt at this path:
$promptPath

Follow that file exactly. Return only the requested artifact. Do not edit files.
"@

    $args = @("-p", $driverPrompt, "--silent", "--no-ask-user", "--no-color", "--stream=off")
    foreach ($tool in $AllowedTools) {
        $args += "--allow-tool=$tool"
    }

    $output = & copilot @args 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub Copilot failed while $Purpose.`n$output"
    }

    return (Remove-AnsiSequences $output).Trim()
}

function Assert-GitHubCopilot {
    Assert-Command "copilot"

    $output = & copilot --help 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub Copilot CLI is required for release README generation. Install @github/copilot and authenticate it so 'copilot' can run in non-interactive mode.`n$output"
    }
}

function Test-GitHubCopilotCommand {
    return [bool](Get-Command "copilot" -ErrorAction SilentlyContinue)
}

function ConvertTo-HtmlText {
    param([AllowNull()][string]$Text)
    if ($null -eq $Text) {
        return ""
    }
    return [System.Net.WebUtility]::HtmlEncode($Text)
}

function Convert-InlineMarkdownToHtml {
    param([AllowNull()][string]$Text)

    $encoded = ConvertTo-HtmlText $Text
    $encoded = [regex]::Replace($encoded, '\[([^\]]+)\]\((https?://[^)]+)\)', '<a href="$2">$1</a>')
    $encoded = [regex]::Replace($encoded, '`([^`]+)`', '<code>$1</code>')
    $encoded = [regex]::Replace($encoded, '\*\*([^*]+)\*\*', '<strong>$1</strong>')
    $encoded = [regex]::Replace($encoded, '_\((major|minor|patch)\)_', '<em>($1)</em>')
    return $encoded
}

function Convert-SimpleMarkdownToHtml {
    param([string]$Markdown)

    $builder = [System.Text.StringBuilder]::new()
    $inList = $false

    foreach ($line in ($Markdown -split "`r?`n")) {
        if ($line -match '^\s*$') {
            if ($inList) {
                [void]$builder.AppendLine("</ul>")
                $inList = $false
            }
            continue
        }

        if ($line -match '^###\s+(.+)$') {
            if ($inList) { [void]$builder.AppendLine("</ul>"); $inList = $false }
            [void]$builder.AppendLine("<h3>$(Convert-InlineMarkdownToHtml $Matches[1])</h3>")
            continue
        }
        if ($line -match '^##\s+(.+)$') {
            if ($inList) { [void]$builder.AppendLine("</ul>"); $inList = $false }
            [void]$builder.AppendLine("<h2>$(Convert-InlineMarkdownToHtml $Matches[1])</h2>")
            continue
        }
        if ($line -match '^#\s+(.+)$') {
            if ($inList) { [void]$builder.AppendLine("</ul>"); $inList = $false }
            [void]$builder.AppendLine("<h1>$(Convert-InlineMarkdownToHtml $Matches[1])</h1>")
            continue
        }
        if ($line -match '^\s*-\s+(.+)$') {
            if (-not $inList) {
                [void]$builder.AppendLine("<ul>")
                $inList = $true
            }
            [void]$builder.AppendLine("<li>$(Convert-InlineMarkdownToHtml $Matches[1])</li>")
            continue
        }

        if ($inList) {
            [void]$builder.AppendLine("</ul>")
            $inList = $false
        }
        [void]$builder.AppendLine("<p>$(Convert-InlineMarkdownToHtml $line)</p>")
    }

    if ($inList) {
        [void]$builder.AppendLine("</ul>")
    }

    return $builder.ToString()
}

function New-DeterministicHtmlReadme {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string]$ChangelogPath,
        [string]$OutputPath
    )

    Write-Step "Generating deterministic end-user README.html"
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "MuffMode v$TargetVersion $channelName" } else { "MuffMode v$TargetVersion" }
    $changelogMarkdown = Get-Content -Raw -LiteralPath $ChangelogPath
    $changelogHtml = Convert-SimpleMarkdownToHtml $changelogMarkdown
    $encodedLabel = ConvertTo-HtmlText $releaseLabel

    $html = @"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>$encodedLabel End-User README</title>
  <style>
    :root {
      color-scheme: dark;
      --gunmetal: #151b1f;
      --steel: #232c31;
      --panel: #2e3737;
      --slime: #9ccc2f;
      --slime-bright: #c5f44e;
      --rust: #b65a2b;
      --amber: #e0aa45;
      --concrete: #b8c0b7;
      --muted: #879186;
      --line: rgba(197, 244, 78, 0.22);
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: radial-gradient(circle at top left, rgba(156, 204, 47, 0.12), transparent 34rem), linear-gradient(135deg, #101417, var(--gunmetal));
      color: #eef3e9;
      font: 16px/1.55 "Segoe UI", Arial, sans-serif;
    }
    a { color: var(--slime-bright); }
    code { color: var(--amber); background: rgba(0, 0, 0, 0.28); padding: 0.08rem 0.28rem; border-radius: 4px; }
    header, main { width: min(1120px, calc(100% - 32px)); margin: 0 auto; }
    header { padding: 3rem 0 1.5rem; border-bottom: 1px solid var(--line); }
    .eyebrow { color: var(--slime); text-transform: uppercase; letter-spacing: 0.08em; font-weight: 700; }
    h1, h2, h3 { line-height: 1.15; margin: 0 0 0.8rem; }
    h1 { font-size: clamp(2rem, 6vw, 4rem); }
    h2 { color: var(--slime-bright); margin-top: 2rem; }
    h3 { color: var(--amber); margin-top: 1.2rem; }
    .lede { max-width: 760px; color: var(--concrete); font-size: 1.1rem; }
    .support-copy { max-width: 760px; color: #dfe7dc; }
    .support-actions { display: flex; flex-wrap: wrap; gap: 0.75rem; margin-top: 1rem; }
    .support-button {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-height: 2.7rem;
      padding: 0.65rem 0.95rem;
      border: 1px solid rgba(238, 243, 233, 0.22);
      border-radius: 8px;
      color: #ffffff;
      background: linear-gradient(135deg, #c13a86, var(--rust));
      box-shadow: 0 10px 26px rgba(0, 0, 0, 0.24);
      font-weight: 800;
      text-decoration: none;
    }
    .support-button.kofi { background: linear-gradient(135deg, #d84c46, var(--amber)); color: #151b1f; }
    .support-button:hover, .support-button:focus { outline: 2px solid var(--slime-bright); outline-offset: 3px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 1rem; margin: 1.5rem 0; }
    .card {
      background: linear-gradient(180deg, rgba(46, 55, 55, 0.96), rgba(28, 34, 35, 0.96));
      border: 1px solid rgba(224, 170, 69, 0.24);
      border-radius: 8px;
      padding: 1rem;
      box-shadow: 0 18px 50px rgba(0, 0, 0, 0.24);
    }
    .card strong { color: #ffffff; }
    .tag { display: inline-block; color: #18200b; background: var(--slime); border-radius: 999px; padding: 0.18rem 0.55rem; font-weight: 800; }
    section { padding: 1.2rem 0; }
    ul, ol { padding-left: 1.25rem; }
    li { margin: 0.35rem 0; }
    table { width: 100%; border-collapse: collapse; margin: 1rem 0; }
    th, td { border-bottom: 1px solid rgba(184, 192, 183, 0.18); padding: 0.65rem; text-align: left; vertical-align: top; }
    th { color: var(--slime); }
    .changelog {
      background: rgba(0, 0, 0, 0.2);
      border-left: 4px solid var(--rust);
      padding: 1rem;
      border-radius: 0 8px 8px 0;
    }
    footer { color: var(--muted); padding: 2rem 0 3rem; }
  </style>
</head>
<body>
  <header>
    <div class="eyebrow">Quake II Rerelease multiplayer mod</div>
    <h1>$encodedLabel</h1>
    <p class="lede">A practical release package for casual games, competitive matches, and the server hosts keeping MuffMode sessions running. This package is flagged as <span class="tag">$Channel</span>.</p>
    <p class="support-copy">Muff Mode is free for players and server hosts. Optional donations help keep future development moving by supporting the time, testing, tooling, and release work behind the mod.</p>
    <div class="support-actions" aria-label="Support the MuffMode authors">
      <a class="support-button" href="https://github.com/sponsors/themuffinator">Sponsor themuffinator</a>
      <a class="support-button kofi" href="https://ko-fi.com/ozy24">Support ozy on Ko-fi</a>
    </div>
  </header>
  <main>
    <section>
      <h2>Install</h2>
      <div class="grid">
        <article class="card">
          <h3>Windows Installer</h3>
          <p>Use the installer for the cleanest setup. It shows detected Steam, Epic Games Store, GOG, and Xbox app / Microsoft Store installs, keeps an other-location option available, shows the resolved target before install, requires a real Quake II folder with a known launcher executable, rejects unsafe system, special-folder, and extracted-package targets, verifies the copied DLL, updater, docs, legal notices, original-map readmes, custom map BSPs, selected entity overrides, version manifest, and exact server/gametype config contents, writes an install receipt, backs up an existing game DLL when needed, and can create updater, launcher, guide, changelog, and server config shortcuts.</p>
        </article>
        <article class="card">
          <h3>Zip Package</h3>
          <p>Extract the zip into the outer <code>Quake 2</code> folder, not directly into <code>rerelease</code> or <code>baseq2</code>. Allow file replacement when prompted.</p>
        </article>
        <article class="card">
          <h3>Included Files</h3>
          <p>The installable package contains <code>game_x64.dll</code>, the <code>MuffModeUpdater.exe</code> updater and launcher, version marker files, this README, the release changelog, and preserved original map readmes under <code>rerelease/baseq2/docs/muffmode/maps/original-readmes</code>.</p>
        </article>
      </div>
    </section>
    <section>
      <h2>First Use</h2>
      <ul>
        <li>Launch Quake II normally after installing.</li>
        <li>Players can use the game menu for team joining, voting, server info, and common actions.</li>
        <li>Useful player commands include <code>team auto</code>, <code>readyup</code>, <code>maplist</code>, <code>motd</code>, <code>callvote</code>, and <code>vote yes</code> / <code>vote no</code>.</li>
        <li>For offhand hook servers, try <code>alias +hook hook</code>, <code>alias -hook unhook</code>, then <code>bind mouse2 +hook</code>.</li>
      </ul>
    </section>
    <section>
      <h2>Server Hosting</h2>
      <ul>
        <li>Execute the bundled server config with <code>exec muff-sv.cfg</code> when it is included in the package.</li>
        <li>Start casual servers with open voting, clear map rotation, and a short MOTD.</li>
        <li>Start competitive servers with ready-up, controlled voting, known gametypes, known rulesets, captain/admin tools, and timeout rules.</li>
        <li>Run <code>doctor</code> after changing server settings to catch risky cvar combinations.</li>
      </ul>
    </section>
    <section>
      <h2>Gametype Overview</h2>
      <div class="grid">
        <article class="card"><strong>Quick public games:</strong> FFA, Instagib, NadeFest, and Horde are easy drop-in choices.</article>
        <article class="card"><strong>Competitive matches:</strong> Duel, TDM, CTF, Clan Arena, and Capture Strike benefit most from a locked map pool and a known ruleset.</article>
        <article class="card"><strong>Community nights:</strong> Red Rover, LMS, Capture Strike, Vampiric Damage, Weapons Frenzy, and Quad Hog change the rhythm without requiring a different install.</article>
      </div>
    </section>
    <section>
      <h2>Ruleset Cheat Sheet</h2>
      <p>Rulesets change the feel of the same map: starts, weapon specs, ammo, armor, health, powerups, knockback, and a few movement details. Players can vote with <code>callvote ruleset &lt;shortname&gt;</code> when the server allows it.</p>
      <table>
        <thead>
          <tr><th>Pick</th><th>Feel</th><th>What players should notice</th></tr>
        </thead>
        <tbody>
          <tr><td><code>q2re</code></td><td>Quake II Rerelease</td><td>Closest rerelease baseline: stock starts, weapon feel, item economy, and fire-rate-only Haste.</td></tr>
          <tr><td><code>mm</code></td><td>Muff Mode</td><td>House balance with smoother rockets, shorter Plasma Beam range, tighter slug economy, stronger powerup flow, and Q2 movement identity.</td></tr>
          <tr><td><code>q3a</code></td><td>Quake III Arena style</td><td>Q3-style starts, weapon specs, ammo, armor, health, splash, knockback, and firing projection using existing Muff Mode assets. Double jumps remain intact.</td></tr>
          <tr><td><code>q2reb</code></td><td>Q2RE Balanced</td><td>Conservative competitive tuning: capped health and armor, softer MG/CG/Rail, faster HyperBlaster, and readable powerup sounds.</td></tr>
          <tr><td><code>q</code></td><td>Quake style</td><td>Shotgun and Axe-style starts, classic rocket emphasis, raised ammo caps, stronger armor replacement, and remapped map weapon slots.</td></tr>
          <tr><td><code>qc</code></td><td>Quake Champions style</td><td>Random opening weapon, tighter health/armor caps, modernized weapon tuning, and no warmup grant of every visible map weapon.</td></tr>
        </tbody>
      </table>
      <p>Q3A keeps Muff Mode's no-custom-assets rule: Gauntlet uses Chainfist, Lightning Gun uses Plasma Beam, Plasma Gun uses HyperBlaster, and Nailgun uses Ion Ripper. Super Shotgun is removed; the regular Shotgun carries Q3 shotgun specs. Because cells are shared across Q3A energy weapons, BFG costs <code>10</code> cells per shot.</p>
    </section>
    <section>
      <h2>Included Custom Maps</h2>
      <p>The source-side <a href="https://github.com/DarkMatter-Productions/MuffMode/blob/main/docs/maps/index.md">Muff Mode Map Guide</a> tracks the current final <code>mm-*</code> remaster and port set, with original-map history, original release dates where found, preserved original readmes/BSPs, separate source-map links, recommended gametypes, and item registers. The GitHub release also publishes separate map-source and original-map archives for players and map authors who want the historical material.</p>
      <table>
        <thead>
          <tr><th>Map</th><th>File</th><th>Status</th><th>Good fits</th></tr>
        </thead>
        <tbody>
          <tr><td>Aerowalk</td><td><code>mm-aerow</code></td><td>Final</td><td>Duel, small FFA, 2v2, Clan Arena</td></tr>
          <tr><td>Bio Rust</td><td><code>mm-biorust</code></td><td>Final</td><td>Duel, small FFA, 2v2</td></tr>
          <tr><td>Conventional</td><td><code>mm-conven</code></td><td>Final</td><td>FFA, 2v2, TDM, Quad Hog</td></tr>
          <tr><td>The Crucible</td><td><code>mm-crucible</code></td><td>Final</td><td>Duel, FFA, 2v2</td></tr>
          <tr><td>Cold Zero</td><td><code>mm-czero</code></td><td>Final</td><td>FFA, 2v2, TDM, Instagib</td></tr>
          <tr><td>Degeneration</td><td><code>mm-degen</code></td><td>Final</td><td>FFA, 2v2, TDM</td></tr>
          <tr><td>The Flesh Refinery</td><td><code>mm-fleshref</code></td><td>Final</td><td>Duel, small FFA, Power Screen experiment</td></tr>
          <tr><td>Grind</td><td><code>mm-grind</code></td><td>Final</td><td>Duel, 2v2, FFA</td></tr>
          <tr><td>Iron Oxide</td><td><code>mm-ironox</code></td><td>Final</td><td>Duel, small FFA, 2v2</td></tr>
          <tr><td>The Killing Machine</td><td><code>mm-kmach</code></td><td>Final</td><td>FFA, 2v2, casual Duel</td></tr>
          <tr><td>Lava Lamp</td><td><code>mm-llamp</code></td><td>Final</td><td>FFA, TDM, party server</td></tr>
          <tr><td>The Longest Yard</td><td><code>mm-longyd</code></td><td>Final</td><td>FFA, Instagib, Clan Arena, jump-pad chaos</td></tr>
          <tr><td>Mortal Coil</td><td><code>mm-mcoil</code></td><td>Final</td><td>Duel, small FFA, 2v2</td></tr>
          <tr><td>Negative Impulse</td><td><code>mm-negimp</code></td><td>Final</td><td>FFA, 2v2, TDM</td></tr>
          <tr><td>The Oppressor</td><td><code>mm-oppress</code></td><td>Final</td><td>FFA, TDM, 2v2</td></tr>
          <tr><td>Painkiller</td><td><code>mm-pkill</code></td><td>Final</td><td>Duel, small FFA, Clan Arena</td></tr>
          <tr><td>The Rage</td><td><code>mm-rage</code></td><td>Final</td><td>Duel, FFA, 2v2</td></tr>
          <tr><td>Railgun 101</td><td><code>mm-rail101</code></td><td>Final</td><td>Instagib, rail practice, aim warmups</td></tr>
          <tr><td>Reclamation</td><td><code>mm-reclam</code></td><td>Final</td><td>Duel, small FFA</td></tr>
          <tr><td>Thunderstruck</td><td><code>mm-thunders</code></td><td>Final</td><td>Duel, Clan Arena, Instagib, rail/rocket practice</td></tr>
          <tr><td>Unknown Domain</td><td><code>mm-undom</code></td><td>Final</td><td>FFA, TDM, large public play</td></tr>
          <tr><td>Wicked</td><td><code>mm-wicked</code></td><td>Final</td><td>Duel, Clan Arena, small FFA</td></tr>
          <tr><td>Window Pain</td><td><code>mm-winpain</code></td><td>Final</td><td>Clan Arena, Instagib, FFA warmups</td></tr>
        </tbody>
      </table>
    </section>
    <section>
      <h2>Changelog</h2>
      <div class="changelog">
        $changelogHtml
      </div>
    </section>
    <footer>
      Generated from the MuffMode release documentation and release changelog.
    </footer>
  </main>
</body>
</html>
"@

    Set-Content -LiteralPath $OutputPath -Value $html -Encoding utf8
}

function ConvertTo-SemVer {
    param([string]$Value)
    $text = $Value.Trim()
    if ($text -notmatch '^v?(\d+)\.(\d+)\.(\d+)(?:[-+].*)?$') {
        throw "Invalid semantic version '$Value'. Expected MAJOR.MINOR.PATCH, such as 0.22.16."
    }
    [pscustomobject]@{
        Major = [int]$Matches[1]
        Minor = [int]$Matches[2]
        Patch = [int]$Matches[3]
        Text = "$([int]$Matches[1]).$([int]$Matches[2]).$([int]$Matches[3])"
    }
}

function Compare-SemVer {
    param($Left, $Right)
    foreach ($part in @("Major", "Minor", "Patch")) {
        if ($Left.$part -gt $Right.$part) { return 1 }
        if ($Left.$part -lt $Right.$part) { return -1 }
    }
    return 0
}

function Get-BumpedVersion {
    param($BaseVersion, [string]$Mode)

    switch ($Mode) {
        "major" { return "$($BaseVersion.Major + 1).0.0" }
        "minor" { return "$($BaseVersion.Major).$($BaseVersion.Minor + 1).0" }
        "patch" { return "$($BaseVersion.Major).$($BaseVersion.Minor).$($BaseVersion.Patch + 1)" }
        "latch" { return "$($BaseVersion.Major).$($BaseVersion.Minor).$($BaseVersion.Patch + 1)" }
    }
}

function Resolve-AutoVersionMode {
    param([string]$ChangeStartTag)

    $range = "$ChangeStartTag..HEAD"
    $log = git -C $RepoRoot log --date=short --pretty=format:'%s%n%b' $range 2>$null | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inspect git history for automatic versioning range $range."
    }
    if ([string]::IsNullOrWhiteSpace($log)) {
        throw "No commits found in $range. Refusing to auto-version an empty release."
    }

    $changedFiles = git -C $RepoRoot diff --find-renames --name-status $range 2>$null | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inspect changed files for automatic versioning range $range."
    }

    $text = "$log`n$changedFiles".ToLowerInvariant()

    if ($text -match '(?m)(breaking change|breaking-change|breaks compatibility|^[a-z]+(\([^)]+\))?!:|^.*!:)') {
        return "major"
    }

    $ledgerEntries = @()
    try {
        $ledgerEntries = @(Get-ChangelogEntriesForRelease -TargetVersion $null)
    }
    catch {
        Write-Warning "Could not inspect the central changelog ledger for auto-versioning; falling back to git history heuristics. $($_.Exception.Message)"
    }

    if ($ledgerEntries.Count -gt 0) {
        if (@($ledgerEntries | Where-Object { $_.Magnitude -in @("major", "minor") }).Count -gt 0) {
            return "minor"
        }

        return "patch"
    }

    if ($text -match '(?m)^(feat|feature)(\(|:)|\badd(ed|s)?\b|\bnew\b|\bintroduce(d|s)?\b|\binstaller\b|\bupdater\b|\bgametype\b|\bruleset\b|\bweapon\b|\bentity\b|\bcvar\b|\bcommand\b|\bmap\b|\bvoting\b|\bmenu\b|\bpackage\b|\bworkflow\b') {
        return "minor"
    }

    return "patch"
}

function Get-ChannelDisplayName {
    param([string]$Channel)
    switch ($Channel) {
        "alpha" { return "Alpha" }
        "beta" { return "Beta" }
        "rc" { return "Release Candidate" }
        "stable" { return "" }
    }
}

function Test-IsPrereleaseChannel {
    param([string]$Channel)
    return $Channel -ne "stable"
}

function Get-ReleasePackageName {
    param(
        [string]$TargetVersion,
        [string]$Channel
    )

    $suffix = if ($Channel -eq "stable") { "" } else { "-$Channel" }
    return "muffmode-$TargetVersion$suffix"
}

function Get-CurrentSourceVersion {
    $versionFile = Join-Path $RepoRoot "VERSION"
    if (Test-Path -LiteralPath $versionFile) {
        return (Get-Content -Raw -LiteralPath $versionFile).Trim()
    }

    $localHeader = Join-Path $RepoRoot "src/sgame/g_local.h"
    if (Test-Path -LiteralPath $localHeader) {
        $content = Get-Content -Raw -LiteralPath $localHeader
        if ($content -match 'GAMEMOD_VERSION\s*=\s*"([^"]+)"') {
            return $Matches[1]
        }
    }

    throw "Could not determine the current source version. Add VERSION or update src/sgame/g_local.h."
}

function Get-LatestReleaseTag {
    if (Get-Command "gh" -ErrorAction SilentlyContinue) {
        try {
            $json = gh release list --repo $ReleaseRepo --limit 50 --json tagName,isLatest,publishedAt 2>$null
            $releases = $json | ConvertFrom-Json
            if ($releases.Count -gt 0) {
                $latest = $releases | Sort-Object publishedAt -Descending | Select-Object -First 1
                if ($latest.tagName) {
                    if (Test-GitRevisionExists $latest.tagName) {
                        return $latest.tagName
                    }

                    Write-Warning "Latest GitHub release tag '$($latest.tagName)' is not available locally; falling back to local git tags."
                }
            }
        }
        catch {
            Write-Warning "Could not query GitHub releases through gh; falling back to local git tags."
        }
    }
    else {
        Write-Warning "GitHub CLI was not found; falling back to local git tags for version resolution."
    }

    $tag = git -C $RepoRoot tag --list "v*" --sort=-version:refname | Select-Object -First 1
    if (-not $tag) {
        throw "Could not find a previous release tag."
    }
    return $tag
}

function Resolve-TargetVersion {
    param([string]$LatestTag, [string]$ChangeStartTag)

    if ($Version) {
        return (ConvertTo-SemVer $Version).Text
    }

    $latest = ConvertTo-SemVer $LatestTag
    $current = ConvertTo-SemVer (Get-CurrentSourceVersion)

    switch ($VersionMode) {
        "major" { return Get-BumpedVersion -BaseVersion $latest -Mode "major" }
        "minor" { return Get-BumpedVersion -BaseVersion $latest -Mode "minor" }
        "patch" { return Get-BumpedVersion -BaseVersion $latest -Mode "patch" }
        "latch" { return Get-BumpedVersion -BaseVersion $latest -Mode "patch" }
        "auto" {
            $autoMode = Resolve-AutoVersionMode -ChangeStartTag $ChangeStartTag
            $candidate = ConvertTo-SemVer (Get-BumpedVersion -BaseVersion $latest -Mode $autoMode)
            Write-Step "Auto version mode selected: $autoMode"

            if ((Compare-SemVer $current $candidate) -ge 0) {
                return $current.Text
            }

            return $candidate.Text
        }
    }
}

function Update-VersionFiles {
    param([string]$TargetVersion)

    $versionFile = Join-Path $RepoRoot "VERSION"
    Set-Content -LiteralPath $versionFile -Value $TargetVersion -Encoding utf8

    $localHeader = Join-Path $RepoRoot "src/sgame/g_local.h"
    $content = Get-Content -Raw -LiteralPath $localHeader
    $replacement = "constexpr const char *GAMEMOD_VERSION = `"$TargetVersion`";"
    $updated = [regex]::Replace(
        $content,
        'constexpr\s+const\s+char\s+\*GAMEMOD_VERSION\s*=\s*"[^"]+";',
        $replacement,
        1
    )
    if ($updated -eq $content) {
        if ($content -match "GAMEMOD_VERSION\s*=\s*`"$([regex]::Escape($TargetVersion))`"") {
            return
        }
        throw "Could not update GAMEMOD_VERSION in src/sgame/g_local.h."
    }
    Set-Content -LiteralPath $localHeader -Value $updated -Encoding utf8
}

function Assert-VersionFilesMatch {
    param([string]$TargetVersion)

    $sourceVersion = ConvertTo-SemVer (Get-CurrentSourceVersion)
    $target = ConvertTo-SemVer $TargetVersion
    if ((Compare-SemVer $sourceVersion $target) -ne 0) {
        throw "Source version is $($sourceVersion.Text), but target version is $($target.Text). Re-run with -UpdateVersionFiles, commit the change, then create the GitHub release."
    }

    $localHeader = Join-Path $RepoRoot "src/sgame/g_local.h"
    $content = Get-Content -Raw -LiteralPath $localHeader
    if ($content -notmatch "GAMEMOD_VERSION\s*=\s*`"$([regex]::Escape($TargetVersion))`"") {
        throw "src/sgame/g_local.h GAMEMOD_VERSION does not match $TargetVersion."
    }
}

function Resolve-PreviousTag {
    param([string]$TargetVersion, [string]$FallbackLatestTag)

    if ($PreviousTag) {
        if (-not (Test-GitRevisionExists $PreviousTag)) {
            throw "Previous tag '$PreviousTag' does not exist locally. Fetch tags or pass a valid tag."
        }
        return $PreviousTag
    }

    $targetTag = "v$TargetVersion"
    $releaseTag = $FallbackLatestTag
    if ($releaseTag -and $releaseTag -ne $targetTag) {
        return $releaseTag
    }

    $tag = git -C $RepoRoot tag --list "v*" --sort=-version:refname |
        Where-Object { $_ -ne $targetTag } |
        Select-Object -First 1
    if (-not $tag) {
        throw "Could not resolve a previous tag for changelog generation."
    }
    return $tag
}

function Get-GitStatus {
    git -C $RepoRoot status --porcelain
}

function Get-GameDllBuildPath {
    param([string]$Configuration, [string]$Platform)

    return Join-Path $RepoRoot "build\msbuild\$Platform\$Configuration\game_x64.dll"
}

function Build-ReleaseDll {
    param([string]$Configuration, [string]$Platform)

    Assert-Command "msbuild"
    $solution = Join-Path $RepoRoot "projects/msvc/MuffMode.sln"
    Write-Step "Building $Configuration|$Platform"
    $buildOutput = & msbuild $solution "/p:Configuration=$Configuration" "/p:Platform=$Platform" 2>&1
    $buildExitCode = $LASTEXITCODE
    $buildOutput | ForEach-Object { Write-Host $_ }
    if ($buildExitCode -ne 0) {
        throw "MSBuild failed."
    }

    $dll = Get-GameDllBuildPath -Configuration $Configuration -Platform $Platform
    if (-not (Test-Path -LiteralPath $dll)) {
        throw "Expected build output was not found: $dll"
    }
    return $dll
}

function Publish-UpdaterExecutable {
    param(
        [string]$Configuration,
        [string]$Runtime,
        [string]$Project,
        [string]$OutputRoot
    )

    Assert-Command "dotnet"

    $projectPath = Resolve-RepoPath $Project
    if (-not (Test-Path -LiteralPath $projectPath)) {
        throw "Updater project was not found: $projectPath"
    }

    $outputRootAbs = Resolve-RepoPath $OutputRoot
    $publishRoot = Join-Path $outputRootAbs "updater-publish"
    New-Item -ItemType Directory -Force -Path $publishRoot | Out-Null

    Write-Step "Publishing MuffMode updater ($Configuration, $Runtime)"
    $publishOutput = & dotnet publish $projectPath `
        -c $Configuration `
        -r $Runtime `
        --self-contained true `
        -p:PublishSingleFile=true `
        -p:IncludeNativeLibrariesForSelfExtract=true `
        -p:DebugType=embedded `
        -p:DebugSymbols=false `
        -o $publishRoot 2>&1

    $publishExitCode = $LASTEXITCODE
    $publishOutput | ForEach-Object { Write-Host $_ }

    if ($publishExitCode -ne 0) {
        throw "dotnet publish failed for the MuffMode updater."
    }

    $updaterExe = Join-Path $publishRoot "MuffModeUpdater.exe"
    if (-not (Test-Path -LiteralPath $updaterExe)) {
        throw "Expected updater executable was not found: $updaterExe"
    }

    return $updaterExe
}

function Resolve-ExistingUpdaterExecutable {
    param(
        [string]$Configuration,
        [string]$Runtime
    )

    $updaterExe = Join-Path $RepoRoot "updater/MuffMode.Updater/bin/$Configuration/net8.0-windows/$Runtime/publish/MuffModeUpdater.exe"
    if (-not (Test-Path -LiteralPath $updaterExe)) {
        throw "-SkipUpdaterBuild was supplied, but MuffModeUpdater.exe does not exist at $updaterExe."
    }
    return $updaterExe
}

function Resolve-InnoSetupCompiler {
    param([string]$CompilerPath)

    if ($CompilerPath) {
        $resolved = Resolve-RepoPath $CompilerPath
        if (-not (Test-Path -LiteralPath $resolved)) {
            throw "Inno Setup compiler was not found: $resolved"
        }
        return $resolved
    }

    $command = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
    if (-not $command) {
        $command = Get-Command "iscc" -ErrorAction SilentlyContinue
    }
    if ($command) {
        return $command.Source
    }

    $candidates = @()
    if (${env:ProgramFiles(x86)}) {
        $candidates += (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe")
    }
    if ($env:ProgramFiles) {
        $candidates += (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "Inno Setup compiler (ISCC.exe) was not found. Install Inno Setup 6, add ISCC.exe to PATH, pass -InnoSetupCompiler, or pass -SkipInstaller."
}

function New-ReleaseChangelog {
    param(
        [string]$TargetVersion,
        [string]$PreviousTag,
        [string]$Channel,
        [string]$OutputPath,
        [object[]]$Entries,
        [string]$Intro
    )

    Write-Step "Compiling release changelog from central ledger $ChangelogPath"
    if ($null -eq $Entries -or $Entries.Count -eq 0) {
        $Entries = @(Get-ChangelogEntriesForRelease -TargetVersion $TargetVersion)
    }

    if ($Entries.Count -eq 0) {
        throw "No central changelog rows are marked Unreleased or $TargetVersion. Add a grouped row to $ChangelogPath before releasing."
    }

    New-LedgerReleaseChangelog `
        -TargetVersion $TargetVersion `
        -PreviousTag $PreviousTag `
        -Channel $Channel `
        -OutputPath $OutputPath `
        -Entries $Entries `
        -Intro $Intro
}

function Get-ReadmeSourceMarkdown {
    $files = @(
        "README.md",
        "docs/player-guide.md",
        "docs/server-host-guide.md",
        "docs/gameplay-reference.md",
        "docs/rulesets.md",
        "docs/maps/index.md",
        "docs/configuration-reference.md",
        "docs/level-design-guide.md"
    )

    $chunks = New-Object System.Collections.Generic.List[string]
    foreach ($file in $files) {
        $path = Join-Path $RepoRoot $file
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $content = Get-Content -Raw -LiteralPath $path

        if ($file -eq "docs/configuration-reference.md") {
            $content = ($content -split "(?m)^## Debug-Only Weapon Balance Cvars")[0]
        }

        $chunks.Add("----- $file -----")
        $chunks.Add($content.Trim())
    }
    return ($chunks -join "`n`n")
}

function Convert-CopilotOutputToHtml {
    param([string]$Output)

    $clean = Remove-AnsiSequences $Output
    $fence = [regex]::Match($clean, '(?is)```(?:html)?\s*(.*?)\s*```')
    if ($fence.Success) {
        $clean = $fence.Groups[1].Value.Trim()
    }
    else {
        $doctypeIndex = $clean.IndexOf("<!DOCTYPE", [System.StringComparison]::OrdinalIgnoreCase)
        $htmlIndex = $clean.IndexOf("<html", [System.StringComparison]::OrdinalIgnoreCase)
        $start = -1
        if ($doctypeIndex -ge 0) { $start = $doctypeIndex }
        elseif ($htmlIndex -ge 0) { $start = $htmlIndex }
        if ($start -ge 0) {
            $clean = $clean.Substring($start).Trim()
        }
    }

    if ($clean -notmatch '(?is)<html\b' -or $clean -notmatch '(?is)</html>') {
        throw "GitHub Copilot did not return a complete HTML document."
    }

    return $clean
}

function New-CopilotHtmlReadme {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string]$ChangelogPath,
        [string]$OutputPath
    )

    $docs = Get-ReadmeSourceMarkdown
    $changelog = Get-Content -Raw -LiteralPath $ChangelogPath
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "MuffMode v$TargetVersion $channelName" } else { "MuffMode v$TargetVersion" }

    $prompt = @"
You are GitHub Copilot helping prepare a public release package for $releaseLabel.

Create a complete standalone HTML document for end users. Use the Markdown documentation and changelog below as source material.

Audience and scope:
- Primary audience: Quake II Rerelease players and server hosts installing this release.
- This project is currently in $Channel channel. Make that release state visible but not alarming.
- Include installation, first-use guidance, player usage, voting, common host setup, gametype overview, a player-focused ruleset guide, offhand hook bind, debugging pointer, package contents, and the changelog.
- Use docs/rulesets.md as the authoritative ruleset source. Make every ruleset's unique feel and tradeoffs clear, including Q3A's existing-asset weapon mappings, preserved double jumps, Super Shotgun removal, regular Shotgun Q3 specs, and shared-cell BFG ammo cost.
- Include a compact "Included Custom Maps" section using the source map guide. Show map title, filename, release status, and good gametype fits, and link to the full Muff Mode Map Guide for history, original release dates, preserved original readmes/BSPs, separate remaster source-map links, and item registers.
- Explain that original map readmes are included in the main installer/manual zip under rerelease/baseq2/docs/muffmode/maps/original-readmes, while source maps and original BSPs are published as separate supplemental release archives.
- Explain that most Windows users can use the installer, which presents detected Steam, Epic Games Store, GOG, and Xbox app / Microsoft Store installs, keeps an other-location choice available, shows the resolved target before install, requires a real Quake II folder with a known launcher executable, rejects unsafe system, special-folder, and extracted-package targets, verifies the copied DLL, updater, docs, legal notices, original-map readmes, custom map BSPs, selected entity overrides, version manifest, and exact server/gametype config contents, writes an install receipt, backs up an existing game DLL when needed, and offers Desktop/Start menu shortcuts for the updater, launcher, install guide, changelog, and server config guide. Also include the zip/manual extraction path for users who prefer it.
- Include elegant support buttons near the top for the authors: themuffinator at https://github.com/sponsors/themuffinator and ozy at https://ko-fi.com/ozy24. Frame donations as optional support that helps promote future development and offsets the real time, testing, tooling, and release costs involved in maintaining the mod.
- Do not include build instructions, source compilation steps, contributor notes, GitHub badges, or repository development workflow.
- Keep it polished, friendly, and practical. Avoid marketing fluff.

Visual design:
- Full standalone HTML with embedded CSS only.
- Quake II inspired palette: gunmetal, dark steel, slime green, rust, amber lights, muted concrete.
- Make it elegant and readable: responsive layout, high contrast, cards/tables where helpful, clear headings, no external images or fonts.

Output requirements:
- Return only the HTML document. Start with <!DOCTYPE html>.
- Do not wrap the output in Markdown fences.
- Use "$releaseLabel" in the title/header.

SOURCE MARKDOWN:
$docs

CHANGELOG:
$changelog
"@

    if (Test-GitHubCopilotCommand) {
        try {
            Write-Step "Generating end-user README.html with GitHub Copilot"
            $output = Invoke-GitHubCopilot -Prompt $prompt -Purpose "generating README.html"

            $html = Convert-CopilotOutputToHtml $output
            Set-Content -LiteralPath $OutputPath -Value $html -Encoding utf8
            return
        }
        catch {
            if ($RequireCopilot) {
                throw
            }
            Write-Warning "GitHub Copilot README generation failed; using deterministic HTML README instead. $($_.Exception.Message)"
        }
    }
    else {
        if ($RequireCopilot) {
            throw "GitHub Copilot CLI is required because -RequireCopilot was supplied, but 'copilot' was not found on PATH."
        }
        Write-Warning "GitHub Copilot CLI was not found; using deterministic HTML README instead."
    }

    New-DeterministicHtmlReadme `
        -TargetVersion $TargetVersion `
        -Channel $Channel `
        -ChangelogPath $ChangelogPath `
        -OutputPath $OutputPath
}

function Test-AllowedPackageRelativePath {
    param([string]$RelativePath)

    $normalized = $RelativePath.Replace('/', '\')
    if ([string]::IsNullOrWhiteSpace($normalized) -or [System.IO.Path]::IsPathRooted($normalized)) {
        return $false
    }

    if (($normalized -split '\\') -contains '..') {
        return $false
    }

    foreach ($topLevelFile in @("README.html", "README.md", "CHANGELOG.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "MuffModeUpdater.exe", "MuffMode.version", "VERSION")) {
        if ([string]::Equals($normalized, $topLevelFile, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    if (-not $normalized.StartsWith("rerelease\", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }

    $expectedDll = "rerelease\baseq2\game_x64.dll"
    $extension = [System.IO.Path]::GetExtension($normalized).ToLowerInvariant()
    $blockedExecutableExtensions = @(".bat", ".cmd", ".com", ".dll", ".exe", ".hta", ".jar", ".js", ".lnk", ".msi", ".pif", ".ps1", ".scr", ".vbs", ".wsf")
    if ($blockedExecutableExtensions -contains $extension) {
        return [string]::Equals($normalized, $expectedDll, [System.StringComparison]::OrdinalIgnoreCase)
    }

    return $true
}

function Assert-ReleasePackageContents {
    param([string]$PackageRoot)

    if (-not (Test-Path -LiteralPath $PackageRoot -PathType Container)) {
        throw "Release package root does not exist: $PackageRoot"
    }

    foreach ($requiredFile in @(
        "README.html",
        "README.md",
        "CHANGELOG.md",
        "LICENSE",
        "THIRD_PARTY_NOTICES.md",
        "MuffModeUpdater.exe",
        "rerelease\baseq2\game_x64.dll",
        "rerelease\baseq2\muffmode-version.json",
        "rerelease\baseq2\muffmode.version",
        "rerelease\baseq2\CONFIGS_README.md",
        "rerelease\baseq2\server-base.cfg",
        "rerelease\baseq2\gt-FFA.cfg",
        "rerelease\baseq2\gt-DUEL.cfg",
        "rerelease\baseq2\gt-TDM.cfg",
        "rerelease\baseq2\gt-CTF.cfg",
        "rerelease\baseq2\gt-CA.cfg",
        "rerelease\baseq2\gt-REDROVER.cfg",
        "rerelease\baseq2\gt-HORDE.cfg",
        "rerelease\baseq2\gt-INSTAGIB.cfg",
        "rerelease\baseq2\gt-NADEFEST.cfg",
        "rerelease\baseq2\gt-STRIKE.cfg",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\README.md",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\2box4-readme.txt",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\aerowalk-readme.txt",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\broken2-readme.txt",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\fleshref-readme.txt",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\grind-readme.txt",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\ztn2dm1-readme.txt",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\ztn2dm2-readme.txt",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\ztn2dm3-readme.txt",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\ztn2dm5-readme.txt",
        "rerelease\maps\mm-aerowalk.bsp",
        "rerelease\maps\mm-coldzero.bsp",
        "rerelease\maps\mm-crucible.bsp",
        "rerelease\maps\mm-kmachine.bsp",
        "rerelease\maps\mm-powertrip.bsp",
        "rerelease\maps\mm-rage.bsp",
        "rerelease\maps\mm-railgun101.bsp",
        "rerelease\maps\mm-reclamation.bsp",
        "rerelease\maps\mm-recycler.bsp",
        "rerelease\maps\2box4.ent",
        "rerelease\maps\aerowalk.ent",
        "rerelease\maps\grom_dm3.ent",
        "rerelease\maps\kmachine.ent",
        "rerelease\maps\koldduel1.ent",
        "rerelease\maps\paradm4.ent",
        "rerelease\maps\trdm04a.ent",
        "rerelease\maps\vd6dm2.ent",
        "rerelease\maps\ven_dm2.ent",
        "rerelease\maps\ztn2dm5.ent"
    )) {
        $path = Join-Path $PackageRoot $requiredFile
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Release package is missing required file: $requiredFile"
        }
    }

    $fullPackageRoot = [System.IO.Path]::GetFullPath($PackageRoot)
    $separator = [System.IO.Path]::DirectorySeparatorChar.ToString()
    $packageRootPrefix = if ($fullPackageRoot.EndsWith($separator)) {
        $fullPackageRoot
    }
    else {
        "$fullPackageRoot$separator"
    }

    $files = @(Get-ChildItem -LiteralPath $PackageRoot -File -Recurse)
    if ($files.Count -eq 0) {
        throw "Release package does not contain any files."
    }

    foreach ($file in $files) {
        Assert-PathUnderDirectory -Path $file.FullName -ParentPath $PackageRoot -Description "Release package file"
        $relativePath = $file.FullName.Substring($packageRootPrefix.Length)
        if (-not (Test-AllowedPackageRelativePath $relativePath)) {
            throw "Release package contains an unexpected or unsafe file path: $relativePath"
        }
    }
}

function Copy-ReleaseAssets {
    param([string]$AssetRoot, [string]$PackageRoot)

    $assetRootAbs = Resolve-RepoPath $AssetRoot
    if (-not (Test-Path -LiteralPath $assetRootAbs)) {
        Write-Warning "Asset root '$AssetRoot' does not exist. Package will contain only generated files and the DLL."
        return
    }

    $items = Get-ChildItem -LiteralPath $assetRootAbs -Force |
        Where-Object { $_.Name -ne ".gitkeep" }

    foreach ($item in $items) {
        Copy-Item -LiteralPath $item.FullName -Destination $PackageRoot -Recurse -Force
    }
}

function Copy-DirectoryContents {
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )

    if (-not (Test-Path -LiteralPath $SourcePath -PathType Container)) {
        throw "Required directory was not found: $SourcePath"
    }

    New-Item -ItemType Directory -Force -Path $DestinationPath | Out-Null
    $items = @(Get-ChildItem -LiteralPath $SourcePath -Force)
    if ($items.Count -eq 0) {
        throw "Required directory is empty: $SourcePath"
    }

    foreach ($item in $items) {
        Copy-Item -LiteralPath $item.FullName -Destination $DestinationPath -Recurse -Force
    }
}

function Copy-OriginalMapReadmesToPackage {
    param([string]$PackageRoot)

    $readmeSource = Resolve-RepoPath "docs/maps/original-readmes"
    $readmeDestination = Join-Path $PackageRoot "rerelease/baseq2/docs/muffmode/maps/original-readmes"

    Copy-DirectoryContents -SourcePath $readmeSource -DestinationPath $readmeDestination

    $txtReadmes = @(Get-ChildItem -LiteralPath $readmeDestination -File -Filter "*.txt")
    if ($txtReadmes.Count -eq 0) {
        throw "No original map readme text files were copied to the release package."
    }
}

function Compress-StagedDirectory {
    param(
        [string]$SourceRoot,
        [string]$ZipPath
    )

    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }

    Compress-Archive -LiteralPath $SourceRoot -DestinationPath $ZipPath -CompressionLevel Optimal
}

function New-MapSupplementArchives {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string]$OutputRoot
    )

    $packageName = Get-ReleasePackageName -TargetVersion $TargetVersion -Channel $Channel
    $outputRootAbs = Resolve-FullPath $OutputRoot
    $stagingRoot = Join-Path $outputRootAbs "staging"
    $mapDocsRoot = Resolve-RepoPath "docs/maps"

    $sourceMapsRoot = Join-Path $mapDocsRoot "source-maps"
    $devSourceMapsRoot = Join-Path $mapDocsRoot "dev-source-maps"
    $originalBspsRoot = Join-Path $mapDocsRoot "original-bsps"
    $originalReadmesRoot = Join-Path $mapDocsRoot "original-readmes"

    foreach ($requiredPath in @($sourceMapsRoot, $devSourceMapsRoot, $originalBspsRoot, $originalReadmesRoot)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Container)) {
            throw "Required map documentation asset directory was not found: $requiredPath"
        }
    }

    $sourceArchiveRoot = Join-Path $stagingRoot "$packageName-map-sources"
    $originalArchiveRoot = Join-Path $stagingRoot "$packageName-original-maps"
    foreach ($archiveRoot in @($sourceArchiveRoot, $originalArchiveRoot)) {
        Assert-PathUnderDirectory -Path $archiveRoot -ParentPath $stagingRoot -Description "Map archive staging directory"
        if (Test-Path -LiteralPath $archiveRoot) {
            Remove-Item -LiteralPath $archiveRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $archiveRoot | Out-Null
    }

    Copy-DirectoryContents -SourcePath $sourceMapsRoot -DestinationPath (Join-Path $sourceArchiveRoot "final-source-maps")
    Copy-DirectoryContents -SourcePath $devSourceMapsRoot -DestinationPath (Join-Path $sourceArchiveRoot "development-source-maps")
    Copy-DirectoryContents -SourcePath $originalReadmesRoot -DestinationPath (Join-Path $sourceArchiveRoot "original-readmes")
    Copy-Item -LiteralPath (Join-Path $mapDocsRoot "index.md") -Destination (Join-Path $sourceArchiveRoot "MAP_GUIDE.md") -Force
    Set-Content -LiteralPath (Join-Path $sourceArchiveRoot "README.md") -Encoding utf8 -Value @"
# Muff Mode Map Sources

This archive is a supplemental release asset for Muff Mode v$TargetVersion.

- `final-source-maps` contains final Muff Mode remaster/port `.map` sources.
- `development-source-maps` contains selected in-development `.map` sources.
- `original-readmes` preserves original map readmes where they were located.
- `MAP_GUIDE.md` is a snapshot of the source-side map guide.

These files are for reference and map authors. They are intentionally not installed into the playable `rerelease/maps` folder.
"@

    Copy-DirectoryContents -SourcePath $originalBspsRoot -DestinationPath (Join-Path $originalArchiveRoot "original-bsps")
    Copy-DirectoryContents -SourcePath $originalReadmesRoot -DestinationPath (Join-Path $originalArchiveRoot "original-readmes")
    Copy-Item -LiteralPath (Join-Path $mapDocsRoot "index.md") -Destination (Join-Path $originalArchiveRoot "MAP_GUIDE.md") -Force
    Set-Content -LiteralPath (Join-Path $originalArchiveRoot "README.md") -Encoding utf8 -Value @"
# Muff Mode Original Maps

This archive is a supplemental release asset for Muff Mode v$TargetVersion.

- `original-bsps` contains preserved original community BSPs used for comparison and historical research.
- `original-readmes` contains matching original map readmes where they were located.
- `MAP_GUIDE.md` is a snapshot of the source-side map guide.

These are historical originals, not the Muff Mode remaster BSPs. They are intentionally zipped separately from the installable Muff Mode package and from the source-map archive.
"@

    $sourceZipPath = Join-Path $outputRootAbs "$packageName-map-sources.zip"
    $originalZipPath = Join-Path $outputRootAbs "$packageName-original-maps.zip"

    Write-Step "Creating map source archive $sourceZipPath"
    Compress-StagedDirectory -SourceRoot $sourceArchiveRoot -ZipPath $sourceZipPath

    Write-Step "Creating original map archive $originalZipPath"
    Compress-StagedDirectory -SourceRoot $originalArchiveRoot -ZipPath $originalZipPath

    return [pscustomobject]@{
        SourceMapsZipPath = $sourceZipPath
        OriginalMapsZipPath = $originalZipPath
    }
}

function New-ReleasePackage {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string]$DllPath,
        [string]$UpdaterPath,
        [string]$ChangelogPath,
        [string]$ReadmeHtmlPath,
        [string]$OutputRoot,
        [string]$AssetRoot
    )

    $packageName = Get-ReleasePackageName -TargetVersion $TargetVersion -Channel $Channel
    $outputRootAbs = Resolve-FullPath $OutputRoot
    $stagingRoot = Join-Path $outputRootAbs "staging"
    $packageRoot = Join-Path $stagingRoot $packageName
    Assert-PathUnderDirectory -Path $packageRoot -ParentPath $stagingRoot -Description "Package staging directory"

    if (Test-Path -LiteralPath $packageRoot) {
        Remove-Item -LiteralPath $packageRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

    Copy-ReleaseAssets -AssetRoot $AssetRoot -PackageRoot $packageRoot

    $baseq2 = Join-Path $packageRoot "rerelease/baseq2"
    New-Item -ItemType Directory -Force -Path $baseq2 | Out-Null

    Copy-Item -LiteralPath $DllPath -Destination (Join-Path $baseq2 "game_x64.dll") -Force
    Copy-Item -LiteralPath (Resolve-RepoPath "LICENSE") -Destination (Join-Path $packageRoot "LICENSE") -Force
    Copy-Item -LiteralPath (Resolve-RepoPath "THIRD_PARTY_NOTICES.md") -Destination (Join-Path $packageRoot "THIRD_PARTY_NOTICES.md") -Force
    Copy-Item -LiteralPath $UpdaterPath -Destination (Join-Path $packageRoot "MuffModeUpdater.exe") -Force
    Copy-Item -LiteralPath $ReadmeHtmlPath -Destination (Join-Path $packageRoot "README.html") -Force
    Copy-Item -LiteralPath $ChangelogPath -Destination (Join-Path $packageRoot "CHANGELOG.md") -Force
    Copy-OriginalMapReadmesToPackage -PackageRoot $packageRoot

    $versionManifest = [ordered]@{
        Version = $TargetVersion
        TagName = "v$TargetVersion"
        Repository = $ReleaseRepo
        Channel = $Channel
        ReleaseUrl = "https://github.com/$ReleaseRepo/releases/tag/v$TargetVersion"
        AssetName = "$packageName.zip"
        PackagedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    }
    Set-Content -LiteralPath (Join-Path $baseq2 "muffmode-version.json") -Value ($versionManifest | ConvertTo-Json) -Encoding utf8
    Set-Content -LiteralPath (Join-Path $baseq2 "muffmode.version") -Value $TargetVersion -Encoding utf8

    Assert-ReleasePackageContents -PackageRoot $packageRoot

    $zipPath = Join-Path $outputRootAbs "$packageName.zip"
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }

    Write-Step "Creating package $zipPath"
    Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal
    return [pscustomobject]@{
        Name = $packageName
        Root = $packageRoot
        ZipPath = $zipPath
    }
}

function New-WindowsInstaller {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string]$PackageRoot,
        [string]$OutputRoot,
        [string]$InstallerScript,
        [string]$InnoSetupCompiler
    )

    $scriptPath = Resolve-RepoPath $InstallerScript
    if (-not (Test-Path -LiteralPath $scriptPath)) {
        throw "Installer script was not found: $scriptPath"
    }
    Assert-ReleasePackageContents -PackageRoot $PackageRoot

    $packageName = Get-ReleasePackageName -TargetVersion $TargetVersion -Channel $Channel
    $outputRootAbs = Resolve-FullPath $OutputRoot
    $compiler = Resolve-InnoSetupCompiler -CompilerPath $InnoSetupCompiler
    $launcherIconFile = Resolve-FullPath "updater/MuffMode.Updater/Assets/MuffModeLauncher.ico"
    if (-not (Test-Path -LiteralPath $launcherIconFile -PathType Leaf)) {
        throw "Launcher icon file was not found: $launcherIconFile"
    }
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "Muff Mode v$TargetVersion $channelName" } else { "Muff Mode v$TargetVersion" }
    $installerBaseName = "$packageName-windows-installer"
    $installerPath = Join-Path $outputRootAbs "$installerBaseName.exe"

    if (Test-Path -LiteralPath $installerPath) {
        Remove-Item -LiteralPath $installerPath -Force
    }

    Write-Step "Creating Windows installer $installerPath"
    $installerOutput = & $compiler `
        "/DAppVersion=$TargetVersion" `
        "/DChannel=$Channel" `
        "/DReleaseLabel=$releaseLabel" `
        "/DPackageRoot=$PackageRoot" `
        "/DOutputDir=$outputRootAbs" `
        "/DInstallerBaseName=$installerBaseName" `
        "/DLauncherIconFile=$launcherIconFile" `
        $scriptPath 2>&1

    $installerExitCode = $LASTEXITCODE
    $installerOutput | ForEach-Object { Write-Host $_ }

    if ($installerExitCode -ne 0) {
        throw "Inno Setup failed while creating the Windows installer."
    }
    if (-not (Test-Path -LiteralPath $installerPath)) {
        throw "Expected installer was not created: $installerPath"
    }

    return $installerPath
}

function Publish-GitHubRelease {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string[]]$AssetPaths,
        [string]$ReleaseNotesPath,
        [bool]$Prerelease
    )

    Assert-Command "gh"
    $dirty = Get-GitStatus
    if ($dirty) {
        throw "Working tree is dirty. Commit release/version changes before publishing a GitHub release."
    }

    $args = @(
        "release", "create", "v$TargetVersion"
    )
    $args += $AssetPaths
    $args += @(
        "--repo", $ReleaseRepo,
        "--title", $(if ((Get-ChannelDisplayName -Channel $Channel)) { "MuffMode v$TargetVersion $(Get-ChannelDisplayName -Channel $Channel)" } else { "MuffMode v$TargetVersion" }),
        "--notes-file", $ReleaseNotesPath,
        "--fail-on-no-commits"
    )
    if ($Prerelease) {
        $args += @("--prerelease", "--latest=false")
    }
    else {
        $args += "--latest"
    }

    $releaseState = if ($Prerelease) { "prerelease" } else { "latest stable release" }
    Write-Step "Publishing GitHub release v$TargetVersion as $releaseState"
    & gh @args
    if ($LASTEXITCODE -ne 0) {
        throw "gh release create failed."
    }
}

Push-Location $RepoRoot
try {
    Assert-Command "git"

    $dirty = Get-GitStatus
    if ($dirty -and -not $AllowDirtyPackage -and -not $UpdateVersionFiles) {
        throw "Working tree is dirty. Commit changes or pass -AllowDirtyPackage for a local package build."
    }

    $latestReleaseTag = Get-LatestReleaseTag
    $changeStartTag = if ($PreviousTag) { $PreviousTag } else { $latestReleaseTag }
    $targetVersion = Resolve-TargetVersion -LatestTag $latestReleaseTag -ChangeStartTag $changeStartTag
    $previousReleaseTag = Resolve-PreviousTag -TargetVersion $targetVersion -FallbackLatestTag $latestReleaseTag
    $isPrerelease = (Test-IsPrereleaseChannel -Channel $Channel) -or [bool]$Prerelease

    Write-Step "Latest release tag: $latestReleaseTag"
    Write-Step "Previous changelog tag: $previousReleaseTag"
    Write-Step "Target version: $targetVersion ($Channel; GitHub prerelease: $isPrerelease)"

    if ($UpdateVersionFiles) {
        Write-Step "Updating VERSION and src/sgame/g_local.h"
        Update-VersionFiles -TargetVersion $targetVersion
        Update-ChangelogReleaseVersions -TargetVersion $targetVersion -Path $ChangelogPath
    }
    Assert-VersionFilesMatch -TargetVersion $targetVersion
    [void](ConvertFrom-ChangelogLedger -Path $ChangelogPath)

    if ($VersionOnly) {
        Write-Step "Version files are ready for $targetVersion"
        return
    }

    if ($CreateGitHubRelease) {
        $dirtyAfterVersion = Get-GitStatus
        if ($dirtyAfterVersion) {
            throw "Version files or other files are uncommitted. Commit them before publishing. You can still build the package without -CreateGitHubRelease."
        }
    }

    if ($SkipBuild) {
        $dllPath = Get-GameDllBuildPath -Configuration $Configuration -Platform $Platform
        if (-not (Test-Path -LiteralPath $dllPath)) {
            throw "-SkipBuild was supplied, but game_x64.dll does not exist at $dllPath."
        }
    }
    else {
        $dllPath = Build-ReleaseDll -Configuration $Configuration -Platform $Platform
    }

    $outputRootAbs = Resolve-RepoPath $OutputRoot
    New-Item -ItemType Directory -Force -Path $outputRootAbs | Out-Null

    if ($SkipUpdaterBuild) {
        $updaterPath = Resolve-ExistingUpdaterExecutable -Configuration $Configuration -Runtime $UpdaterRuntime
    }
    else {
        $updaterPath = Publish-UpdaterExecutable `
            -Configuration $Configuration `
            -Runtime $UpdaterRuntime `
            -Project $UpdaterProject `
            -OutputRoot $OutputRoot
    }

    $releaseNotesPath = Join-Path $outputRootAbs "muffmode-$targetVersion-release-notes.md"
    $readmeHtmlPath = Join-Path $outputRootAbs "README-$targetVersion.html"

    $releaseEntries = @(Get-ChangelogEntriesForRelease -TargetVersion $targetVersion)
    if ($releaseEntries.Count -eq 0) {
        throw "No central changelog rows are marked Unreleased or $targetVersion. Add grouped release notes to $ChangelogPath before packaging."
    }
    $resolvedReleaseIntro = Resolve-ReleaseIntroMessage `
        -TargetVersion $targetVersion `
        -Channel $Channel `
        -Entries $releaseEntries `
        -ManualIntro $ReleaseIntro

    New-ReleaseChangelog `
        -TargetVersion $targetVersion `
        -PreviousTag $previousReleaseTag `
        -Channel $Channel `
        -OutputPath $releaseNotesPath `
        -Entries $releaseEntries `
        -Intro $resolvedReleaseIntro
    New-CopilotHtmlReadme -TargetVersion $targetVersion -Channel $Channel -ChangelogPath $releaseNotesPath -OutputPath $readmeHtmlPath

    $package = New-ReleasePackage `
        -TargetVersion $targetVersion `
        -Channel $Channel `
        -DllPath $dllPath `
        -UpdaterPath $updaterPath `
        -ChangelogPath $releaseNotesPath `
        -ReadmeHtmlPath $readmeHtmlPath `
        -OutputRoot $OutputRoot `
        -AssetRoot $AssetRoot

    $releaseAssetPaths = New-Object System.Collections.Generic.List[string]
    $releaseAssetPaths.Add($package.ZipPath)

    $mapArchives = New-MapSupplementArchives `
        -TargetVersion $targetVersion `
        -Channel $Channel `
        -OutputRoot $OutputRoot
    $releaseAssetPaths.Add($mapArchives.SourceMapsZipPath)
    $releaseAssetPaths.Add($mapArchives.OriginalMapsZipPath)

    $installerPath = $null
    if ($SkipInstaller) {
        Write-Host "Windows installer not created because -SkipInstaller was supplied."
    }
    else {
        $installerPath = New-WindowsInstaller `
            -TargetVersion $targetVersion `
            -Channel $Channel `
            -PackageRoot $package.Root `
            -OutputRoot $OutputRoot `
            -InstallerScript $InstallerScript `
            -InnoSetupCompiler $InnoSetupCompiler
        $releaseAssetPaths.Add($installerPath)
    }

    $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $package.ZipPath
    $sourceMapsHash = Get-FileHash -Algorithm SHA256 -LiteralPath $mapArchives.SourceMapsZipPath
    $originalMapsHash = Get-FileHash -Algorithm SHA256 -LiteralPath $mapArchives.OriginalMapsZipPath
    Write-Step "Package ready"
    Write-Host "Package: $($package.ZipPath)"
    Write-Host "SHA256:  $($hash.Hash.ToLowerInvariant())"
    Write-Host "Map sources: $($mapArchives.SourceMapsZipPath)"
    Write-Host "SHA256:      $($sourceMapsHash.Hash.ToLowerInvariant())"
    Write-Host "Original maps: $($mapArchives.OriginalMapsZipPath)"
    Write-Host "SHA256:        $($originalMapsHash.Hash.ToLowerInvariant())"
    if ($installerPath) {
        $installerHash = Get-FileHash -Algorithm SHA256 -LiteralPath $installerPath
        Write-Host "Installer: $installerPath"
        Write-Host "SHA256:    $($installerHash.Hash.ToLowerInvariant())"
    }
    Write-Host "Notes:   $releaseNotesPath"
    Write-Host "README:  $readmeHtmlPath"
    Write-Host "Updater: $updaterPath"

    if ($CreateGitHubRelease) {
        Publish-GitHubRelease -TargetVersion $targetVersion -Channel $Channel -AssetPaths $releaseAssetPaths.ToArray() -ReleaseNotesPath $releaseNotesPath -Prerelease $isPrerelease
    }
    else {
        Write-Host "GitHub release not created. Re-run with -CreateGitHubRelease to publish with --latest."
    }
}
finally {
    Pop-Location
}
