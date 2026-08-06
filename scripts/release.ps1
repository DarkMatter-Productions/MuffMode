#requires -Version 7.0

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
    [string]$BuildReceiptPath,
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
$script:ReadmeLanguageEntries = @(
    [pscustomobject]@{
        Code = "en"
        HtmlLang = "en"
        EnglishName = "English"
        NativeName = "English"
        PackageFileName = "README.html"
        SwitchLabel = "Language options"
    },
    [pscustomobject]@{
        Code = "de"
        HtmlLang = "de"
        EnglishName = "German"
        NativeName = "Deutsch"
        PackageFileName = "README.de.html"
        SwitchLabel = "Sprachen"
    },
    [pscustomobject]@{
        Code = "pl"
        HtmlLang = "pl"
        EnglishName = "Polish"
        NativeName = "Polski"
        PackageFileName = "README.pl.html"
        SwitchLabel = "Języki"
    },
    [pscustomobject]@{
        Code = "fr"
        HtmlLang = "fr"
        EnglishName = "French"
        NativeName = "Français"
        PackageFileName = "README.fr.html"
        SwitchLabel = "Langues"
    },
    [pscustomobject]@{
        Code = "hu"
        HtmlLang = "hu"
        EnglishName = "Hungarian"
        NativeName = "Magyar"
        PackageFileName = "README.hu.html"
        SwitchLabel = "Nyelvek"
    },
    [pscustomobject]@{
        Code = "bg"
        HtmlLang = "bg"
        EnglishName = "Bulgarian"
        NativeName = "Български"
        PackageFileName = "README.bg.html"
        SwitchLabel = "Езици"
    }
)

function Get-ReadmeLanguageEntries {
    return @($script:ReadmeLanguageEntries)
}

function Get-ReadmeTranslationLanguages {
    return @(Get-ReadmeLanguageEntries | Where-Object { $_.Code -ne "en" })
}

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

function Assert-WindowsExecutableImage {
    param(
        [string]$Path,
        [string]$Description,
        [UInt16[]]$AllowedMachines = @([UInt16]0x014c, [UInt16]0x8664, [UInt16]0xaa64),
        [ValidateSet("Either", "Executable", "Dll")]
        [string]$ImageKind = "Either"
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description was not found: $Path"
    }

    $file = Get-Item -LiteralPath $Path
    if ($file.Length -lt 64) {
        throw "$Description is too small to be a Windows executable image: $Path"
    }

    $stream = [System.IO.File]::Open($file.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $reader = $null
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) {
            throw "$Description does not have a valid DOS executable header: $Path"
        }

        $stream.Position = 0x3c
        $peOffset = [Int64]$reader.ReadUInt32()
        if ($peOffset -lt 0x40 -or $peOffset -gt ($file.Length - 24)) {
            throw "$Description has an invalid or out-of-bounds PE header offset: $Path"
        }

        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "$Description does not have a valid PE signature: $Path"
        }

        $machine = $reader.ReadUInt16()
        $sectionCount = $reader.ReadUInt16()
        $reader.ReadUInt32() | Out-Null # TimeDateStamp
        $reader.ReadUInt32() | Out-Null # PointerToSymbolTable
        $reader.ReadUInt32() | Out-Null # NumberOfSymbols
        $optionalHeaderSize = $reader.ReadUInt16()
        $characteristics = $reader.ReadUInt16()

        if ($AllowedMachines.Count -eq 0 -or $AllowedMachines -notcontains $machine) {
            $actualMachine = "0x{0:X4}" -f $machine
            $expectedMachines = ($AllowedMachines | ForEach-Object { "0x{0:X4}" -f $_ }) -join ", "
            throw "$Description targets unsupported Windows machine $actualMachine (expected: $expectedMachines): $Path"
        }

        if ($sectionCount -eq 0) {
            throw "$Description does not contain any PE sections: $Path"
        }

        if (($characteristics -band 0x0002) -eq 0) {
            throw "$Description is not marked as an executable PE image: $Path"
        }

        $isDll = ($characteristics -band 0x2000) -ne 0
        if ($ImageKind -eq "Dll" -and -not $isDll) {
            throw "$Description is not marked as a PE DLL image: $Path"
        }
        if ($ImageKind -eq "Executable" -and $isDll) {
            throw "$Description is marked as a PE DLL instead of an executable: $Path"
        }

        $optionalHeaderOffset = $peOffset + 24
        $sectionTableOffset = $optionalHeaderOffset + [Int64]$optionalHeaderSize
        $sectionTableEnd = $sectionTableOffset + ([Int64]$sectionCount * 40)
        if ($optionalHeaderSize -lt 2 -or $sectionTableOffset -gt $file.Length -or $sectionTableEnd -gt $file.Length) {
            throw "$Description has an out-of-bounds PE optional header or section table: $Path"
        }

        $stream.Position = $optionalHeaderOffset
        $optionalHeaderMagic = $reader.ReadUInt16()
        $expectedOptionalHeaderMagic = if ($machine -eq 0x014c) { 0x010b } else { 0x020b }
        if ($optionalHeaderMagic -ne $expectedOptionalHeaderMagic) {
            $actualMagic = "0x{0:X4}" -f $optionalHeaderMagic
            $expectedMagic = "0x{0:X4}" -f $expectedOptionalHeaderMagic
            throw "$Description has PE optional-header magic $actualMagic, expected $expectedMagic for its machine: $Path"
        }
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        else {
            $stream.Dispose()
        }
    }
}

function Test-GitTagExists {
    param([string]$TagName)

    git -C $RepoRoot rev-parse --verify --end-of-options "refs/tags/${TagName}^{commit}" *> $null
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
        throw "GitHub Copilot CLI is required for release README generation and translation. Install @github/copilot and authenticate it so 'copilot' can run in non-interactive mode.`n$output"
    }
}

function Test-GitHubCopilotCommand {
    return [bool](Get-Command "copilot" -ErrorAction SilentlyContinue)
}

function Test-ReleaseCopilotUserToken {
    return -not [string]::IsNullOrWhiteSpace($env:COPILOT_GITHUB_TOKEN)
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
    $encodedChannel = ConvertTo-HtmlText $Channel

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
      --void: #050506;
      --black: #090909;
      --gunmetal: #121416;
      --steel: #262a2c;
      --panel: #161719;
      --panel-2: #242629;
      --blue-void: #071121;
      --blue-deep: #0d2846;
      --blue-soft: #1e6fb2;
      --gold: #f2c64a;
      --gold-bright: #ffe071;
      --gold-deep: #b8780a;
      --red: #ef1010;
      --red-bright: #ff3a2f;
      --red-deep: #8c0303;
      --chrome: #c6c8c5;
      --chrome-bright: #f4f4ee;
      --muted: #9a9b95;
      --line: rgba(242, 198, 74, 0.28);
      --red-line: rgba(239, 16, 16, 0.26);
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background:
        linear-gradient(180deg, rgba(3, 4, 8, 0.08), rgba(3, 4, 8, 0.76)),
        linear-gradient(116deg, transparent 0 9%, rgba(30, 111, 178, 0.24) 9% 34%, transparent 34% 68%, rgba(239, 16, 16, 0.13) 68% 76%, transparent 76%),
        linear-gradient(244deg, rgba(239, 16, 16, 0.14) 0 12%, transparent 12% 54%, rgba(30, 111, 178, 0.18) 54% 78%, transparent 78%),
        repeating-linear-gradient(135deg, rgba(244, 244, 238, 0.035) 0 1px, transparent 1px 58px),
        repeating-linear-gradient(90deg, rgba(30, 111, 178, 0.075) 0 1px, transparent 1px 118px),
        linear-gradient(180deg, var(--void), var(--blue-void) 24%, var(--blue-deep) 48%, #08090c 100%);
      color: #eef3e9;
      font: 16px/1.55 "Segoe UI", Arial, sans-serif;
      background-attachment: fixed;
    }
    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      background:
        linear-gradient(90deg, rgba(239, 16, 16, 0.08), transparent 18% 82%, rgba(239, 16, 16, 0.08)),
        repeating-linear-gradient(180deg, rgba(255, 255, 255, 0.028) 0 1px, transparent 1px 5px);
      opacity: 0.34;
    }
    a { color: var(--gold-bright); }
    code {
      color: var(--gold-bright);
      background: rgba(0, 0, 0, 0.38);
      border: 1px solid rgba(242, 198, 74, 0.26);
      padding: 0.08rem 0.28rem;
      border-radius: 4px;
      white-space: nowrap;
    }
    header, main { width: min(1120px, calc(100% - 32px)); margin: 0 auto; }
    header {
      position: relative;
      overflow: hidden;
      padding: 3.4rem 0 1.6rem;
      border-bottom: 1px solid var(--line);
      background:
        linear-gradient(90deg, rgba(239, 16, 16, 0.12), transparent 24% 70%, rgba(242, 198, 74, 0.08)),
        linear-gradient(180deg, rgba(13, 40, 70, 0.52), rgba(5, 5, 6, 0));
    }
    header::before {
      content: "";
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 4px;
      background: linear-gradient(90deg, var(--gold-bright), var(--gold) 32%, var(--red) 70%, var(--red-deep));
      box-shadow: 0 0 24px rgba(239, 16, 16, 0.38), 0 0 18px rgba(242, 198, 74, 0.28);
    }
    header::after {
      content: "ARENA STATUS // RELEASE PACKAGE";
      position: absolute;
      right: 0;
      bottom: 1.2rem;
      color: rgba(244, 244, 238, 0.13);
      font-size: 0.9rem;
      font-weight: 900;
    }
    .eyebrow { color: var(--gold-bright); text-transform: uppercase; letter-spacing: 0; font-weight: 900; }
    h1, h2, h3 { line-height: 1.15; margin: 0 0 0.8rem; }
    h1 {
      color: var(--chrome-bright);
      background: linear-gradient(96deg, #fff0a2 0 18%, var(--gold-bright) 18% 34%, var(--gold) 34% 45%, var(--red-bright) 58%, var(--red) 76%, var(--red-deep) 100%);
      -webkit-background-clip: text;
      background-clip: text;
      -webkit-text-fill-color: transparent;
      -webkit-text-stroke: 1px rgba(0, 0, 0, 0.82);
      filter: drop-shadow(0 3px 0 rgba(0, 0, 0, 0.86)) drop-shadow(0 0 16px rgba(239, 16, 16, 0.26));
      font-size: 3.8rem;
      font-weight: 900;
      letter-spacing: 0;
      margin-bottom: 0.95rem;
      text-transform: uppercase;
    }
    h2 {
      color: var(--gold-bright);
      margin-top: 2rem;
      padding-left: 0.75rem;
      border-left: 4px solid var(--red);
      text-transform: uppercase;
    }
    h3 { color: var(--gold); margin-top: 1.2rem; }
    .lede { max-width: 760px; color: var(--chrome); font-size: 1.1rem; }
    .hud-rail {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
      margin: 1.35rem 0;
      border: 1px solid var(--red-line);
      background: linear-gradient(135deg, rgba(36, 38, 41, 0.94), rgba(7, 7, 8, 0.94));
      box-shadow: 0 18px 52px rgba(0, 0, 0, 0.34), inset 0 1px 0 rgba(255, 255, 255, 0.06);
    }
    .hud-cell {
      padding: 0.75rem 0.9rem;
      border-left: 1px solid rgba(242, 198, 74, 0.18);
    }
    .hud-cell:first-child { border-left: 0; }
    .hud-cell strong {
      display: block;
      color: var(--red-bright);
      font-size: 0.78rem;
      font-weight: 900;
      text-transform: uppercase;
    }
    .hud-cell span { color: var(--chrome-bright); font-weight: 800; }
    .quick-nav { display: flex; flex-wrap: wrap; gap: 0.5rem; margin: 1.2rem 0; }
    .quick-nav a {
      border: 1px solid rgba(242, 198, 74, 0.28);
      border-radius: 6px;
      color: #eef3e9;
      background: linear-gradient(180deg, rgba(36, 38, 41, 0.82), rgba(9, 9, 9, 0.82));
      padding: 0.45rem 0.68rem;
      text-decoration: none;
      text-transform: uppercase;
      font-weight: 800;
    }
    .support-copy { max-width: 760px; color: #deded7; }
    .support-actions { display: flex; flex-wrap: wrap; gap: 0.75rem; margin-top: 1rem; }
    .support-button {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-height: 2.7rem;
      padding: 0.65rem 0.95rem;
      border: 1px solid rgba(238, 243, 233, 0.22);
      border-radius: 6px;
      color: #ffffff;
      background: linear-gradient(135deg, var(--red-deep), var(--red-bright));
      box-shadow: 0 14px 32px rgba(0, 0, 0, 0.34), 0 0 20px rgba(239, 16, 16, 0.20);
      font-weight: 800;
      text-decoration: none;
      text-transform: uppercase;
    }
    .support-button.kofi { background: linear-gradient(135deg, var(--gold), var(--gold-bright)); color: #14110a; }
    .support-button:hover, .support-button:focus, .quick-nav a:hover, .quick-nav a:focus { outline: 2px solid var(--gold-bright); outline-offset: 3px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 1rem; margin: 1.5rem 0; }
    .card {
      position: relative;
      overflow: hidden;
      background:
        linear-gradient(180deg, rgba(36, 38, 41, 0.96), rgba(10, 10, 11, 0.96)),
        linear-gradient(90deg, rgba(242, 198, 74, 0.11), transparent);
      border: 1px solid rgba(242, 198, 74, 0.20);
      border-radius: 8px;
      padding: 1rem;
      box-shadow: 0 18px 50px rgba(0, 0, 0, 0.30), inset 0 1px 0 rgba(255, 255, 255, 0.05);
    }
    .card::before {
      content: "";
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 3px;
      background: linear-gradient(90deg, var(--gold-bright), var(--red), var(--red-deep));
    }
    .card strong { color: var(--chrome-bright); }
    .tag {
      display: inline-block;
      color: #140e03;
      background: linear-gradient(90deg, var(--gold-bright), var(--gold) 55%, var(--red-bright));
      border-radius: 6px;
      padding: 0.18rem 0.55rem;
      font-weight: 900;
      text-transform: uppercase;
    }
    .notice {
      border-left: 4px solid var(--gold);
      border-right: 1px solid rgba(242, 198, 74, 0.22);
      background: linear-gradient(90deg, rgba(242, 198, 74, 0.15), rgba(239, 16, 16, 0.08));
      border-radius: 0 8px 8px 0;
      padding: 0.85rem 1rem;
      color: #f8ecd4;
    }
    .steps { counter-reset: step; list-style: none; padding-left: 0; }
    .steps li { counter-increment: step; margin: 0.55rem 0; padding-left: 2.2rem; position: relative; }
    .steps li::before {
      content: counter(step);
      position: absolute;
      left: 0;
      top: 0.05rem;
      width: 1.5rem;
      height: 1.5rem;
      border-radius: 6px;
      background: linear-gradient(135deg, var(--gold-bright), var(--red-bright));
      color: #120f0b;
      display: inline-grid;
      place-items: center;
      font-weight: 900;
    }
    section { padding: 1.2rem 0; }
    ul, ol { padding-left: 1.25rem; }
    li { margin: 0.35rem 0; }
    .table-wrap { overflow-x: auto; }
    table {
      width: 100%;
      border-collapse: collapse;
      margin: 1rem 0;
      border: 1px solid rgba(242, 198, 74, 0.20);
      background: rgba(7, 11, 15, 0.52);
    }
    th, td { border-bottom: 1px solid rgba(184, 192, 183, 0.16); padding: 0.65rem; text-align: left; vertical-align: top; }
    th {
      color: var(--gold-bright);
      background: linear-gradient(90deg, rgba(239, 16, 16, 0.12), rgba(242, 198, 74, 0.10));
      text-transform: uppercase;
    }
    tr:hover td { background: rgba(242, 198, 74, 0.045); }
    .changelog {
      background: linear-gradient(180deg, rgba(12, 18, 24, 0.92), rgba(4, 8, 10, 0.92));
      border-left: 4px solid var(--red);
      border-top: 1px solid rgba(242, 198, 74, 0.18);
      padding: 1rem;
      border-radius: 0 8px 8px 0;
    }
    footer { color: var(--muted); padding: 2rem 0 3rem; }
    @media (max-width: 700px) {
      header, main { width: calc(100% - 24px); }
      h1 {
        font-size: 1.95rem;
        overflow-wrap: anywhere;
        word-break: break-word;
        -webkit-text-stroke-width: 0.75px;
      }
      header::after { display: none; }
      code { white-space: normal; overflow-wrap: anywhere; }
      .hud-rail { grid-template-columns: 1fr; }
      .hud-cell, .hud-cell:first-child { border-left: 0; border-top: 1px solid rgba(242, 198, 74, 0.16); }
      .hud-cell:first-child { border-top: 0; }
      .quick-nav a { flex: 1 1 calc(50% - 0.5rem); text-align: center; }
      .support-actions { display: grid; }
      .support-button { width: 100%; text-align: center; white-space: normal; }
    }
  </style>
</head>
<body>
  <header>
    <div class="eyebrow">Quake II Rerelease multiplayer mod</div>
    <h1>$encodedLabel</h1>
    <p class="lede">Install Muff Mode, join or host a game, and keep the important server files close at hand. This release is flagged as <span class="tag">$encodedChannel</span>.</p>
    <div class="hud-rail" aria-label="Release status">
      <div class="hud-cell"><strong>Target</strong><span>Players + Hosts</span></div>
      <div class="hud-cell"><strong>Payload</strong><span>Game DLL + Configs</span></div>
      <div class="hud-cell"><strong>Channel</strong><span>$encodedChannel</span></div>
    </div>
    <nav class="quick-nav" aria-label="README sections">
      <a href="#install">Install</a>
      <a href="#play">Play</a>
      <a href="#host">Host</a>
      <a href="#modes">Modes</a>
      <a href="#rulesets">Rulesets</a>
      <a href="#maps">Maps</a>
      <a href="#changelog">Changelog</a>
    </nav>
    <p class="support-copy">Muff Mode is free for players and server hosts. Optional donations help keep future development moving by supporting the time, testing, tooling, and release work behind the mod.</p>
    <div class="support-actions" aria-label="Support the MuffMode authors">
      <a class="support-button" href="https://github.com/sponsors/themuffinator">Sponsor themuffinator</a>
      <a class="support-button kofi" href="https://ko-fi.com/ozy24">Support ozy on Ko-fi</a>
    </div>
  </header>
  <main>
    <section id="install">
      <h2>Install</h2>
      <p class="notice">Install into the outer <code>Quake 2</code> folder. The mod files belong under that folder's <code>rerelease/baseq2</code> tree.</p>
      <div class="grid">
        <article class="card">
          <h3>Windows Installer</h3>
          <p>Recommended for most Windows users. It detects Steam, Epic Games Store, GOG, and Xbox app / Microsoft Store installs, keeps an Other location option, verifies the copied files, writes an install receipt, and backs up existing server configs and the Muff Mode DLL before replacing them.</p>
        </article>
        <article class="card">
          <h3>Zip Package</h3>
          <p>Extract the zip into the outer <code>Quake 2</code> folder, not directly into <code>rerelease</code> or <code>baseq2</code>. Allow file replacement when prompted.</p>
        </article>
        <article class="card">
          <h3>What Gets Installed</h3>
          <p>The package contains <code>game_x64.dll</code>, <code>MuffModeUpdater.exe</code>, version markers, this README, the release changelog, server configs, map files, selected entity overrides, and preserved original map readmes.</p>
        </article>
      </div>
    </section>
    <section id="play">
      <h2>First Use</h2>
      <ol class="steps">
        <li>Launch Quake II normally after installing.</li>
        <li>Players can use the game menu for team joining, voting, server info, and common actions.</li>
        <li>Useful player commands include <code>team auto</code>, <code>readyup</code>, <code>maplist</code>, <code>motd</code>, <code>callvote</code>, and <code>vote yes</code> / <code>vote no</code>.</li>
        <li>For offhand hook servers, try <code>alias +hook hook</code>, <code>alias -hook unhook</code>, then <code>bind mouse2 +hook</code>.</li>
      </ol>
    </section>
    <section id="host">
      <h2>Server Hosting</h2>
      <div class="grid">
        <article class="card">
          <h3>Load The Baseline</h3>
          <p>Run <code>exec server-base.cfg</code> for shared safety, voting, entity override, and player-limit defaults.</p>
        </article>
        <article class="card">
          <h3>Choose A Preset</h3>
          <p>Run a mode config such as <code>exec gt-FFA.cfg</code>, <code>exec gt-DUEL.cfg</code>, <code>exec gt-LMS.cfg</code>, <code>exec gt-ARENA.cfg</code>, or another packaged <code>gt-*.cfg</code>.</p>
          <p><code>gt-ARENA.cfg</code> supports separately installed RA2-compatible maps through MuffMode Arena Rooms; RA2 assets are not bundled.</p>
        </article>
        <article class="card">
          <h3>Check Your Setup</h3>
          <p>Run <code>doctor</code> after changing cvars. Use <code>g_muffmode_debug 1</code> only while investigating a server issue, then turn it back off.</p>
        </article>
      </div>
    </section>
    <section id="modes">
      <h2>Gametype Overview</h2>
      <div class="grid">
        <article class="card"><strong>Quick public games:</strong> FFA, Instagib, NadeFest, and Horde are easy drop-in choices.</article>
        <article class="card"><strong>Competitive matches:</strong> Duel, Rocket Arena, TDM, CTF, Clan Arena, and Capture Strike benefit most from a locked map pool and a known ruleset.</article>
        <article class="card"><strong>Community nights:</strong> Last Man Standing offers round-based free-for-all elimination, while Red Rover, Capture Strike, Vampiric Damage, Weapons Frenzy, and Quad Hog change the rhythm without requiring a different install.</article>
      </div>
    </section>
    <section id="rulesets">
      <h2>Ruleset Cheat Sheet</h2>
      <p>Rulesets change the feel of the same map: starts, weapon specs, ammo, armor, health, powerups, knockback, and a few movement details. Players can vote with <code>callvote ruleset &lt;shortname&gt;</code> when the server allows it.</p>
      <div class="table-wrap"><table>
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
      </table></div>
      <p>Q3A keeps Muff Mode's no-custom-assets rule: Gauntlet uses Chainfist, Lightning Gun uses Plasma Beam, Plasma Gun uses HyperBlaster, and Nailgun uses Ion Ripper. Super Shotgun is removed; the regular Shotgun carries Q3 shotgun specs. Because cells are shared across Q3A energy weapons, BFG costs <code>10</code> cells per shot.</p>
    </section>
    <section id="maps">
      <h2>Included Custom Maps</h2>
      <p>The <a href="https://github.com/DarkMatter-Productions/MuffMode/blob/main/docs/maps/index.md">Muff Mode Map Guide</a> tracks the current final <code>mm-*</code> remaster and port set. Original map readmes are included in this package under <code>rerelease/baseq2/docs/muffmode/maps/original-readmes</code>; source maps and original BSPs are published as separate supplemental release archives.</p>
      <div class="table-wrap"><table>
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
      </table></div>
    </section>
    <section id="changelog">
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

    $range = "refs/tags/$ChangeStartTag..HEAD"
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
                    if (Test-GitTagExists $latest.tagName) {
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
        if (-not (Test-GitTagExists $PreviousTag)) {
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

function Get-SourceCommit {
    $commit = (& git -C $RepoRoot rev-parse HEAD 2>$null | Out-String).Trim().ToLowerInvariant()
    if ($LASTEXITCODE -ne 0 -or $commit -cnotmatch "^[0-9a-f]{40}$") {
        throw "Could not resolve the source tree to a lowercase 40-character Git commit."
    }
    return $commit
}

function Get-GameDllBuildPath {
    param([string]$Configuration, [string]$Platform)

    return Join-Path $RepoRoot "build\msbuild\$Platform\$Configuration\game_x64.dll"
}

function Get-ReleaseBuildReceiptPath {
    param(
        [string]$Configuration,
        [string]$Platform,
        [AllowNull()][string]$RequestedPath
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        return Resolve-FullPath $RequestedPath
    }

    return Join-Path $RepoRoot "build\msbuild\$Platform\$Configuration\muffmode-build-receipt.json"
}

function Write-ReleaseBuildReceipt {
    param(
        [string]$Path,
        [string]$SourceCommit,
        [string]$Configuration,
        [string]$Platform,
        [string]$DllPath,
        [bool]$WarningsAsErrors
    )

    $dll = Get-Item -LiteralPath $DllPath -ErrorAction Stop
    $receipt = [ordered]@{
        SchemaVersion = 1
        SourceCommit = $SourceCommit
        Configuration = $Configuration
        Platform = $Platform
        DllName = $dll.Name
        DllSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $dll.FullName).Hash.ToLowerInvariant()
        WarningsAsErrors = $WarningsAsErrors
    }
    $parent = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    $receipt | ConvertTo-Json | Set-Content -LiteralPath $Path -Encoding utf8
}

function Assert-ReleaseBuildReceipt {
    param(
        [string]$Path,
        [string]$SourceCommit,
        [string]$Configuration,
        [string]$Platform,
        [string]$DllPath
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Skipped release builds require a matching build receipt, but none was found at $Path. Build through scripts/release.ps1 or the release workflow before reusing game_x64.dll."
    }

    $receipt = Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
    foreach ($propertyName in @("SchemaVersion", "SourceCommit", "Configuration", "Platform", "DllName", "DllSha256", "WarningsAsErrors")) {
        if ($receipt.PSObject.Properties.Name -cnotcontains $propertyName) {
            throw "Release build receipt is missing required property '$propertyName': $Path"
        }
    }

    $dll = Get-Item -LiteralPath $DllPath -ErrorAction Stop
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $dll.FullName).Hash.ToLowerInvariant()
    if ([int]$receipt.SchemaVersion -ne 1 -or
        [string]$receipt.SourceCommit -cne $SourceCommit -or
        [string]$receipt.Configuration -cne $Configuration -or
        [string]$receipt.Platform -cne $Platform -or
        [string]$receipt.DllName -cne $dll.Name -or
        [string]$receipt.DllSha256 -cnotmatch "^[0-9a-f]{64}$" -or
        [string]$receipt.DllSha256 -cne $actualHash -or
        $receipt.WarningsAsErrors -isnot [bool] -or
        -not [bool]$receipt.WarningsAsErrors) {
        throw "Release build receipt does not match the current source commit, build settings, and game DLL: $Path"
    }
}

function Build-ReleaseDll {
    param([string]$Configuration, [string]$Platform)

    Assert-Command "msbuild"
    $solution = Join-Path $RepoRoot "projects/msvc/MuffMode.sln"
    Write-Step "Building $Configuration|$Platform"
    $buildOutput = & msbuild $solution "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:MMTreatWarningsAsErrors=true" 2>&1
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

    Assert-WindowsExecutableImage -Path $updaterExe -Description "Published MuffMode updater" -AllowedMachines @([UInt16]0x8664) -ImageKind Executable
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
    Assert-WindowsExecutableImage -Path $updaterExe -Description "Existing MuffMode updater" -AllowedMachines @([UInt16]0x8664) -ImageKind Executable
    return $updaterExe
}

function Resolve-InnoSetupCompiler {
    param([string]$CompilerPath)

    if ($CompilerPath) {
        $resolved = Resolve-FullPath $CompilerPath
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
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
        "docs/level-design-guide.md",
        "packaging/release-assets/rerelease/baseq2/CONFIGS_README.md"
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

function Set-HtmlDocumentLanguage {
    param(
        [string]$Html,
        [string]$HtmlLang
    )

    $match = [regex]::Match($Html, '(?is)<html\b([^>]*)>')
    if (-not $match.Success) {
        throw "HTML document is missing an <html> element."
    }

    $attrs = $match.Groups[1].Value
    if ($attrs -match '\blang\s*=') {
        $attrs = [regex]::Replace($attrs, '\s+lang\s*=\s*("[^"]*"|''[^'']*''|[^\s>]+)', " lang=`"$HtmlLang`"", 1)
    }
    else {
        $attrs = " lang=`"$HtmlLang`"$attrs"
    }

    return "$($Html.Substring(0, $match.Index))<html$attrs>$($Html.Substring($match.Index + $match.Length))"
}

function Get-HtmlReadmeLanguageSwitcherStyle {
    return @"

    /* MuffMode generated language switcher */
    .language-switcher {
      width: min(1120px, calc(100% - 32px));
      margin: 0.85rem auto 0;
      display: flex;
      flex-wrap: wrap;
      gap: 0.45rem;
      justify-content: flex-end;
      position: relative;
      z-index: 1;
    }
    .language-switcher a {
      border: 1px solid rgba(242, 198, 74, 0.28);
      border-radius: 6px;
      color: #eef3e9;
      background: linear-gradient(180deg, rgba(36, 38, 41, 0.82), rgba(9, 9, 9, 0.82));
      padding: 0.34rem 0.55rem;
      text-decoration: none;
      font-size: 0.86rem;
      font-weight: 800;
    }
    .language-switcher a[aria-current="page"] {
      color: #140e03;
      background: linear-gradient(90deg, #ffe071, #f2c64a 55%, #ff3a2f);
    }
    .language-switcher a:hover,
    .language-switcher a:focus {
      outline: 2px solid #ffe071;
      outline-offset: 3px;
    }
    @media (max-width: 700px) {
      .language-switcher {
        width: calc(100% - 24px);
        justify-content: flex-start;
      }
      .language-switcher a {
        flex: 1 1 calc(50% - 0.45rem);
        text-align: center;
      }
    }
"@
}

function Add-HtmlReadmeLanguageSwitcher {
    param(
        [string]$Html,
        [string]$CurrentLanguageCode,
        [object[]]$Languages = @()
    )

    if ($Languages.Count -eq 0) {
        $Languages = @(Get-ReadmeLanguageEntries)
    }

    $currentLanguage = $Languages | Where-Object { $_.Code -eq $CurrentLanguageCode } | Select-Object -First 1
    if (-not $currentLanguage) {
        throw "README language code '$CurrentLanguageCode' is not included in the generated package."
    }

    $htmlWithLanguage = Set-HtmlDocumentLanguage -Html $Html -HtmlLang $currentLanguage.HtmlLang
    $htmlWithoutSwitcher = [regex]::Replace(
        $htmlWithLanguage,
        '(?is)\s*<nav\b[^>]*class\s*=\s*["''][^"'']*\blanguage-switcher\b[^"'']*["''][^>]*>.*?</nav>\s*',
        "`n"
    )

    if ($htmlWithoutSwitcher -notmatch 'MuffMode generated language switcher') {
        $style = Get-HtmlReadmeLanguageSwitcherStyle
        if ($htmlWithoutSwitcher -match '(?is)</style>') {
            $htmlWithoutSwitcher = [regex]::Replace($htmlWithoutSwitcher, '(?is)</style>', "$style`n  </style>", 1)
        }
        elseif ($htmlWithoutSwitcher -match '(?is)</head>') {
            $htmlWithoutSwitcher = [regex]::Replace($htmlWithoutSwitcher, '(?is)</head>', "  <style>$style`n  </style>`n</head>", 1)
        }
        else {
            throw "HTML document is missing </head> or </style>; cannot add language switcher styles."
        }
    }

    $links = New-Object System.Collections.Generic.List[string]
    foreach ($language in $Languages) {
        $currentAttrs = if ($language.Code -eq $CurrentLanguageCode) { ' aria-current="page"' } else { "" }
        $links.Add("      <a href=`"$($language.PackageFileName)`" hreflang=`"$($language.HtmlLang)`" lang=`"$($language.HtmlLang)`"$currentAttrs>$(ConvertTo-HtmlText $language.NativeName)</a>")
    }

    $switcher = @"
  <nav class="language-switcher" aria-label="$(ConvertTo-HtmlText $currentLanguage.SwitchLabel)">
$($links -join "`n")
  </nav>
"@

    if ($htmlWithoutSwitcher -notmatch '(?is)<body\b[^>]*>') {
        throw "HTML document is missing a <body> element."
    }

    return [regex]::Replace($htmlWithoutSwitcher, '(?is)<body\b[^>]*>', "`$0`n$switcher", 1)
}

function Get-HtmlCodeTextTokens {
    param([string]$Html)

    return @([regex]::Matches($Html, '(?is)<(?:code|kbd|samp|pre)\b[^>]*>(.*?)</(?:code|kbd|samp|pre)>') | ForEach-Object {
        $text = [regex]::Replace($_.Groups[1].Value, '(?is)<[^>]+>', '')
        ([System.Net.WebUtility]::HtmlDecode($text)).Trim()
    })
}

function Get-HtmlHrefValues {
    param([string]$Html)

    return @([regex]::Matches($Html, '(?is)\bhref\s*=\s*(["''])(.*?)\1') | ForEach-Object {
        $_.Groups[2].Value
    })
}

function Normalize-HtmlReadmeComparisonText {
    param([AllowNull()][string]$Text)

    if ($null -eq $Text) {
        return ""
    }

    $decoded = [System.Net.WebUtility]::HtmlDecode($Text)
    return ([regex]::Replace($decoded, '\s+', ' ')).Trim()
}

function Get-HtmlVisibleTextBlocks {
    param([string]$Html)

    $textHtml = $Html
    $textHtml = [regex]::Replace(
        $textHtml,
        '(?is)<nav\b[^>]*class\s*=\s*["''][^"'']*\blanguage-switcher\b[^"'']*["''][^>]*>.*?</nav>',
        ' '
    )

    foreach ($tag in @("script", "style", "code", "kbd", "samp", "pre")) {
        $pattern = '(?is)<{0}\b[^>]*>.*?</{0}>' -f [regex]::Escape($tag)
        $textHtml = [regex]::Replace($textHtml, $pattern, ' ')
    }

    $textHtml = [regex]::Replace($textHtml, '(?is)<!--.*?-->', ' ')
    $visibleText = [regex]::Replace($textHtml, '(?is)<[^>]+>', "`n")

    $blocks = New-Object System.Collections.Generic.List[string]
    foreach ($line in ($visibleText -split "`r?`n")) {
        $normalized = Normalize-HtmlReadmeComparisonText $line
        if ($normalized.Length -ge 20 -and $normalized -match '\p{L}') {
            $blocks.Add($normalized)
        }
    }

    return $blocks.ToArray()
}

function Assert-HtmlReadmeHasTranslatedProse {
    param(
        [string]$SourceHtml,
        [string]$TranslatedHtml,
        [object]$Language
    )

    $sourceBlocks = @(Get-HtmlVisibleTextBlocks $SourceHtml)
    $translatedBlocks = @(Get-HtmlVisibleTextBlocks $TranslatedHtml)

    if ($translatedBlocks.Count -eq 0) {
        throw "$($Language.EnglishName) README has no visible translated prose to validate."
    }

    $sourceBlockSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)
    foreach ($block in $sourceBlocks) {
        [void]$sourceBlockSet.Add($block)
    }

    $unchangedCount = 0
    foreach ($block in $translatedBlocks) {
        if ($sourceBlockSet.Contains($block)) {
            $unchangedCount++
        }
    }

    $unchangedRatio = [double]$unchangedCount / [double]$translatedBlocks.Count
    if ($sourceBlocks.Count -gt 0 -and $unchangedRatio -ge 0.80) {
        throw "$($Language.EnglishName) README still looks mostly untranslated: $unchangedCount of $($translatedBlocks.Count) visible prose blocks exactly match the English source."
    }
}

function Assert-MatchingStringSequence {
    param(
        [string[]]$Expected,
        [string[]]$Actual,
        [string]$Description
    )

    if ($Expected.Count -ne $Actual.Count) {
        throw "$Description count changed during translation. Expected $($Expected.Count), found $($Actual.Count)."
    }

    for ($i = 0; $i -lt $Expected.Count; $i++) {
        if ($Expected[$i] -ne $Actual[$i]) {
            throw "$Description changed during translation at item $($i + 1). Expected '$($Expected[$i])', found '$($Actual[$i])'."
        }
    }
}

function Assert-TranslatedHtmlReadme {
    param(
        [string]$SourcePath,
        [string]$TranslatedPath,
        [object]$Language
    )

    if (-not (Test-Path -LiteralPath $TranslatedPath -PathType Leaf)) {
        throw "Translated README was not generated: $TranslatedPath"
    }

    $sourceHtml = Get-Content -Raw -LiteralPath $SourcePath
    $translatedHtml = Get-Content -Raw -LiteralPath $TranslatedPath

    if ($translatedHtml -notmatch '(?is)<html\b' -or $translatedHtml -notmatch '(?is)</html>') {
        throw "$($Language.EnglishName) README is not a complete HTML document."
    }
    if ($translatedHtml -notmatch "(?is)<html\b[^>]*\blang\s*=\s*(`"$([regex]::Escape($Language.HtmlLang))`"|'$([regex]::Escape($Language.HtmlLang))'|$([regex]::Escape($Language.HtmlLang)))(\s|>)") {
        throw "$($Language.EnglishName) README does not set html lang='$($Language.HtmlLang)'."
    }

    Assert-MatchingStringSequence `
        -Expected (Get-HtmlCodeTextTokens $sourceHtml) `
        -Actual (Get-HtmlCodeTextTokens $translatedHtml) `
        -Description "$($Language.EnglishName) README protected code/pre text"

    Assert-MatchingStringSequence `
        -Expected (Get-HtmlHrefValues $sourceHtml) `
        -Actual (Get-HtmlHrefValues $translatedHtml) `
        -Description "$($Language.EnglishName) README links"

    $protectedTerms = @(
        "MuffMode",
        "Muff Mode",
        "Quake II",
        "Quake II Rerelease",
        "game_x64.dll",
        "MuffModeUpdater.exe",
        "server-base.cfg",
        "gt-*.cfg",
        "gt-FFA.cfg",
        "gt-DUEL.cfg",
        "gt-LMS.cfg",
        "gt-ARENA.cfg",
        "gt-HORDE.cfg",
        "g_gametype_cfg",
        "g_muffmode_debug",
        "Q3A",
        "BFG"
    )

    foreach ($term in $protectedTerms) {
        if ($sourceHtml.Contains($term) -and -not $translatedHtml.Contains($term)) {
            throw "$($Language.EnglishName) README changed or removed protected term '$term'."
        }
    }

    Assert-HtmlReadmeHasTranslatedProse `
        -SourceHtml $sourceHtml `
        -TranslatedHtml $translatedHtml `
        -Language $Language
}

function New-TranslatedHtmlReadmes {
    param(
        [string]$SourcePath,
        [string]$TargetVersion,
        [string]$Channel,
        [string]$OutputRoot
    )

    $allLanguages = @(Get-ReadmeLanguageEntries)
    $translationLanguages = @(Get-ReadmeTranslationLanguages)
    $canGenerateTranslations =
        (Test-GitHubCopilotCommand) -and (Test-ReleaseCopilotUserToken)
    $packageLanguages = @(
        if ($canGenerateTranslations) {
            $allLanguages
        }
        else {
            $allLanguages | Where-Object { $_.Code -eq "en" }
        }
    )

    Write-Step "Adding README language switcher"
    $sourceHtml = Get-Content -Raw -LiteralPath $SourcePath
    $sourceHtml = Add-HtmlReadmeLanguageSwitcher `
        -Html $sourceHtml `
        -CurrentLanguageCode "en" `
        -Languages $packageLanguages
    Set-Content -LiteralPath $SourcePath -Value $sourceHtml -Encoding utf8

    if ($translationLanguages.Count -eq 0) {
        return @()
    }

    if (-not $canGenerateTranslations) {
        if ($RequireCopilot) {
            if (-not (Test-GitHubCopilotCommand)) {
                throw "GitHub Copilot CLI is required because -RequireCopilot was supplied, but 'copilot' was not found on PATH."
            }
            throw "COPILOT_GITHUB_TOKEN is required because -RequireCopilot was supplied, but no user Copilot token is configured. Localized README files must be real translations, not English copies."
        }

        Write-Warning "GitHub Copilot is not configured; skipping localized README translations (English README only)."
        return @()
    }

    Assert-GitHubCopilot
    $outputRootAbs = Resolve-RepoPath $OutputRoot
    $results = New-Object System.Collections.Generic.List[object]

    foreach ($language in $translationLanguages) {
        $outputPath = Join-Path $outputRootAbs ("README-{0}.{1}.html" -f $TargetVersion, $language.Code)
        $prompt = @"
You are translating the Muff Mode end-user release README from English to $($language.EnglishName).

Translate the HTML below into natural, polished $($language.EnglishName) for players and server hosts.

Critical preservation rules:
- Return one complete standalone HTML document only. Start with <!DOCTYPE html>. Do not wrap the output in Markdown fences.
- Preserve the HTML structure, CSS, class names, ids, URLs, href values, file paths, filenames, and language-switcher links.
- Set the root <html> lang attribute to "$($language.HtmlLang)".
- Translate visible human prose and human-readable title/alt/aria text.
- Do not translate or alter text inside <code>, <pre>, <kbd>, <samp>, <style>, or <script>.
- Do not translate command names, cvar names, config names, ruleset tokens, gametype tokens, map filenames, DLL/EXE names, path fragments, aliases, binds, URLs, or file extensions. Examples include server-base.cfg, gt-*.cfg, gt-FFA.cfg, gt-DUEL.cfg, gt-LMS.cfg, gt-ARENA.cfg, g_gametype_cfg, g_muffmode_debug, team auto, arena create, readyup, callvote, vote yes, vote no, alias +hook hook, alias -hook unhook, bind mouse2 +hook, doctor, q2re, mm, q3a, q2reb, q, qc, game_x64.dll, and MuffModeUpdater.exe.
- Preserve product and project names such as MuffMode, Muff Mode, Quake II, Quake II Rerelease, GitHub, Ko-fi, Steam, Epic Games Store, GOG, Xbox app, and Microsoft Store.
- Keep the tone practical, concise, and friendly.

SOURCE HTML:
$sourceHtml
"@

        try {
            Write-Step "Translating README.html to $($language.EnglishName)"
            $output = Invoke-GitHubCopilot -Prompt $prompt -Purpose "translating README.html to $($language.Code)"
            $translatedHtml = Convert-CopilotOutputToHtml $output
            $translatedHtml = Add-HtmlReadmeLanguageSwitcher `
                -Html $translatedHtml `
                -CurrentLanguageCode $language.Code `
                -Languages $packageLanguages
            Set-Content -LiteralPath $outputPath -Value $translatedHtml -Encoding utf8
            Assert-TranslatedHtmlReadme -SourcePath $SourcePath -TranslatedPath $outputPath -Language $language
            $results.Add([pscustomobject]@{
                Code = $language.Code
                HtmlLang = $language.HtmlLang
                EnglishName = $language.EnglishName
                NativeName = $language.NativeName
                PackageFileName = $language.PackageFileName
                Path = $outputPath
            })
        }
        catch {
            throw "Failed to generate $($language.EnglishName) README translation. $($_.Exception.Message)"
        }
    }

    return $results.ToArray()
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
- Include installation, first-use guidance, player usage, voting, common host setup, packaged server config usage, gametype overview, a player-focused ruleset guide, offhand hook bind, debugging pointer, package contents, and the changelog.
- Explain that hosts should load server-base.cfg first, then a packaged gt-*.cfg preset such as gt-FFA.cfg, gt-DUEL.cfg, or gt-ARENA.cfg; g_gametype_cfg then auto-executes matching gametype configs on later mode changes.
- Explain that gt-ARENA.cfg supports separately installed RA2-compatible maps through MuffMode Arena Rooms, and that RA2 assets are not bundled.
- Use docs/rulesets.md as the authoritative ruleset source. Make every ruleset's unique feel and tradeoffs clear, including Q3A's existing-asset weapon mappings, preserved double jumps, Super Shotgun removal, regular Shotgun Q3 specs, and shared-cell BFG ammo cost.
- Include a compact "Included Custom Maps" section using the source map guide. Show map title, filename, release status, and good gametype fits, and link to the full Muff Mode Map Guide for history, original release dates, preserved original readmes/BSPs, separate remaster source-map links, and item registers.
- Explain that original map readmes are included in the main installer/manual zip under rerelease/baseq2/docs/muffmode/maps/original-readmes, while source maps and original BSPs are published as separate supplemental release archives.
- Explain that most Windows users can use the installer. Keep this concise: it detects Steam, Epic Games Store, GOG, and Xbox app / Microsoft Store installs, keeps an other-location choice available, verifies installed files, writes an install receipt, backs up existing server configs and the Muff Mode DLL before replacing them, and offers useful shortcuts. Also include the zip/manual extraction path for users who prefer it.
- Describe Last Man Standing as an available round-based free-for-all elimination mode and mention its packaged gt-LMS.cfg preset.
- Include elegant support buttons near the top for the authors: themuffinator at https://github.com/sponsors/themuffinator and ozy at https://ko-fi.com/ozy24. Frame donations as optional support that helps promote future development and offsets the real time, testing, tooling, and release costs involved in maintaining the mod.
- Do not include build instructions, source compilation steps, contributor notes, GitHub badges, or repository development workflow.
- Keep it polished, friendly, and practical. Avoid marketing fluff.

Visual design:
- Full standalone HTML with embedded CSS only.
- Use the MuffMode logo palette: black bevels, graphite/chrome metal, metallic gold/yellow for "Muff", hot red for "Mode", and red UI energy/warning elements. Do not use a green/slime palette.
- Add a soft blue sci-fi arena background like a blurred HUD/stage backdrop: deep navy, diffused blue panels, subtle scanlines, and a faint grid. Keep the blue atmospheric while gold/red/chrome carry the brand identity.
- Presentation should feel like esports meets sci-fi Quake: angular dark metal panels, strong section rails, compact HUD status rows, red accent bars, readable cards/tables, and high contrast.
- Make it elegant and readable: responsive layout, practical hierarchy, no external images or fonts.

Output requirements:
- Return only the HTML document. Start with <!DOCTYPE html>.
- Do not wrap the output in Markdown fences.
- Use "$releaseLabel" in the title/header.

SOURCE MARKDOWN:
$docs

CHANGELOG:
$changelog
"@

    if ((Test-GitHubCopilotCommand) -and (Test-ReleaseCopilotUserToken)) {
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
            Write-Warning "GitHub Copilot English README generation failed; using deterministic English HTML README instead. $($_.Exception.Message)"
        }
    }
    elseif ($RequireCopilot) {
        if (-not (Test-GitHubCopilotCommand)) {
            throw "GitHub Copilot CLI is required because -RequireCopilot was supplied, but 'copilot' was not found on PATH."
        }

        throw "COPILOT_GITHUB_TOKEN is required because -RequireCopilot was supplied, but no user Copilot token is configured."
    }
    elseif (Test-GitHubCopilotCommand) {
        Write-Warning "COPILOT_GITHUB_TOKEN is not set; using deterministic English HTML README instead."
    }
    else {
        Write-Warning "GitHub Copilot CLI was not found; using deterministic English HTML README instead."
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

    $segments = @($normalized -split '\\')
    if ($segments.Count -eq 0 -or
        @($segments | Where-Object { [string]::IsNullOrWhiteSpace($_) -or $_ -eq "." -or $_ -eq ".." }).Count -ne 0) {
        return $false
    }

    if ($segments.Count -eq 1) {
        return @(
        "README.html",
        "README.de.html",
        "README.pl.html",
        "README.fr.html",
        "README.hu.html",
        "README.bg.html",
        "README.md",
        "CHANGELOG.md",
        "LICENSE",
        "THIRD_PARTY_NOTICES.md",
        "MuffModeUpdater.exe",
        "MuffMode.version",
        "VERSION"
        ) -contains $segments[0]
    }

    if ($segments.Count -lt 3 -or
        -not [string]::Equals($segments[0], "rerelease", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }

    $expectedDll = "rerelease\baseq2\game_x64.dll"
    if ([string]::Equals($normalized, $expectedDll, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }

    $extension = [System.IO.Path]::GetExtension($normalized).ToLowerInvariant()
    if ([string]::Equals($segments[1], "maps", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $extension -in @(".bsp", ".ent")
    }

    if ([string]::Equals($segments[1], "bots", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $extension -eq ".nav"
    }

    if ([string]::Equals($segments[1], "baseq2", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $extension -in @(".cfg", ".json", ".md", ".txt", ".version")
    }

    return $false
}

function Test-AllowedPackageRelativeDirectory {
    param([string]$RelativePath)

    $normalized = $RelativePath.Replace('/', '\')
    if ([string]::IsNullOrWhiteSpace($normalized) -or [System.IO.Path]::IsPathRooted($normalized)) {
        return $false
    }

    $segments = @($normalized -split '\\')
    if ($segments.Count -eq 0 -or
        @($segments | Where-Object { [string]::IsNullOrWhiteSpace($_) -or $_ -eq "." -or $_ -eq ".." }).Count -ne 0 -or
        -not [string]::Equals($segments[0], "rerelease", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }

    if ($segments.Count -eq 1) {
        return $true
    }

    return $segments[1] -in @("baseq2", "bots", "maps")
}

function Assert-ReleasePackageContents {
    param(
        [string]$PackageRoot,
        [switch]$RequireLocalizedReadmes
    )

    if (-not (Test-Path -LiteralPath $PackageRoot -PathType Container)) {
        throw "Release package root does not exist: $PackageRoot"
    }

    $packageRootItem = Get-Item -LiteralPath $PackageRoot -Force
    if (($packageRootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Release package root must not be a reparse point: $PackageRoot"
    }

    $requiredFiles = @(
        "README.html",
        "README.md",
        "CHANGELOG.md",
        "LICENSE",
        "THIRD_PARTY_NOTICES.md",
        "MuffModeUpdater.exe",
        "MuffMode.version",
        "VERSION",
        "rerelease\baseq2\game_x64.dll",
        "rerelease\baseq2\muffmode-version.json",
        "rerelease\baseq2\muffmode.version",
        "rerelease\baseq2\CONFIGS_README.md",
        "rerelease\baseq2\muffmode-map-cycle.example.txt",
        "rerelease\baseq2\muffmode-map-pool.example.json",
        "rerelease\baseq2\server-base.cfg",
        "rerelease\baseq2\gt-FFA.cfg",
        "rerelease\baseq2\gt-DUEL.cfg",
        "rerelease\baseq2\gt-TDM.cfg",
        "rerelease\baseq2\gt-CTF.cfg",
        "rerelease\baseq2\gt-CA.cfg",
        "rerelease\baseq2\gt-ARENA.cfg",
        "rerelease\baseq2\gt-FT.cfg",
        "rerelease\baseq2\gt-LMS.cfg",
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
        "rerelease\maps\mm-aerow.bsp",
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
    )

    if ($RequireLocalizedReadmes) {
        $requiredFiles += @(
            "README.de.html",
            "README.pl.html",
            "README.fr.html",
            "README.hu.html",
            "README.bg.html"
        )
    }

    foreach ($requiredFile in $requiredFiles) {
        $path = Join-Path $PackageRoot $requiredFile
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Release package is missing required file: $requiredFile"
        }
    }

    Assert-WindowsExecutableImage -Path (Join-Path $PackageRoot "MuffModeUpdater.exe") -Description "Packaged MuffMode updater" -AllowedMachines @([UInt16]0x8664) -ImageKind Executable
    Assert-WindowsExecutableImage -Path (Join-Path $PackageRoot "rerelease\baseq2\game_x64.dll") -Description "Packaged game DLL" -AllowedMachines @([UInt16]0x8664) -ImageKind Dll

    $fullPackageRoot = [System.IO.Path]::GetFullPath($PackageRoot)
    $separator = [System.IO.Path]::DirectorySeparatorChar.ToString()
    $packageRootPrefix = if ($fullPackageRoot.EndsWith($separator)) {
        $fullPackageRoot
    }
    else {
        "$fullPackageRoot$separator"
    }

    $directories = @(Get-ChildItem -LiteralPath $PackageRoot -Directory -Recurse -Force)
    foreach ($directory in $directories) {
        Assert-PathUnderDirectory -Path $directory.FullName -ParentPath $PackageRoot -Description "Release package directory"
        $relativePath = $directory.FullName.Substring($packageRootPrefix.Length)
        if (($directory.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Release package contains a reparse-point directory: $relativePath"
        }
        if (-not (Test-AllowedPackageRelativeDirectory $relativePath)) {
            throw "Release package contains an unexpected directory path for installed updater compatibility: $relativePath"
        }
    }

    $files = @(Get-ChildItem -LiteralPath $PackageRoot -File -Recurse -Force)
    if ($files.Count -eq 0) {
        throw "Release package does not contain any files."
    }

    foreach ($file in $files) {
        Assert-PathUnderDirectory -Path $file.FullName -ParentPath $PackageRoot -Description "Release package file"
        $relativePath = $file.FullName.Substring($packageRootPrefix.Length)
        if (($file.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Release package contains a reparse-point file: $relativePath"
        }
        if (-not (Test-AllowedPackageRelativePath $relativePath)) {
            throw "Release package contains an unexpected file path for installed updater compatibility: $relativePath"
        }
        if ($file.Length -eq 0) {
            throw "Release package contains an empty file that installed updater builds reject: $relativePath"
        }
    }
}

function Remove-EmptyReleaseAssetPlaceholders {
    param([string]$PackageRoot)

    # These tracked map-sidecar placeholders have never contained an override.
    # Keep them in source history, but do not put zero-byte files into update
    # ZIPs because already-installed updater builds reject empty entries.
    foreach ($relativePath in @(
        "rerelease\maps\rdemo1.dm2.ent",
        "rerelease\maps\xdemo3.dm2.ent"
    )) {
        $path = Join-Path $PackageRoot $relativePath
        if ((Test-Path -LiteralPath $path -PathType Leaf) -and
            (Get-Item -LiteralPath $path -Force).Length -eq 0) {
            Remove-Item -LiteralPath $path -Force
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

function Assert-ExistingUpdaterCompatiblePackageRoot {
    param([string]$PackageRoot)

    # Released updater builds accept only this exact root inventory. Keep the
    # update ZIP compatible until a separately bootstrapped updater can safely
    # replace itself.
    $allowedRootFiles = @(
        "CHANGELOG.md",
        "LICENSE",
        "MuffMode.version",
        "MuffModeUpdater.exe",
        "README.html",
        "README.md",
        "THIRD_PARTY_NOTICES.md",
        "VERSION"
    )

    foreach ($entry in Get-ChildItem -LiteralPath $PackageRoot -Force) {
        if ($entry.PSIsContainer) {
            if ($entry.Name -ne "rerelease") {
                throw "The updater package root contains an unsupported directory for existing updater builds: $($entry.Name)"
            }
        }
        elseif ($allowedRootFiles -notcontains $entry.Name) {
            throw "The updater package root contains an unsupported file for existing updater builds: $($entry.Name)"
        }
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
        [string]$SourceCommit,
        [bool]$SourceTreeDirty,
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
    Remove-EmptyReleaseAssetPlaceholders -PackageRoot $packageRoot

    $baseq2 = Join-Path $packageRoot "rerelease/baseq2"
    New-Item -ItemType Directory -Force -Path $baseq2 | Out-Null

    Copy-Item -LiteralPath $DllPath -Destination (Join-Path $baseq2 "game_x64.dll") -Force
    Copy-Item -LiteralPath (Resolve-RepoPath "LICENSE") -Destination (Join-Path $packageRoot "LICENSE") -Force
    Copy-Item -LiteralPath (Resolve-RepoPath "THIRD_PARTY_NOTICES.md") -Destination (Join-Path $packageRoot "THIRD_PARTY_NOTICES.md") -Force
    Copy-Item -LiteralPath $UpdaterPath -Destination (Join-Path $packageRoot "MuffModeUpdater.exe") -Force
    $englishLanguage = @(
        Get-ReadmeLanguageEntries |
            Where-Object { $_.Code -eq "en" }
    )
    $packageReadmeHtml = Get-Content -Raw -LiteralPath $ReadmeHtmlPath
    $packageReadmeHtml = Add-HtmlReadmeLanguageSwitcher `
        -Html $packageReadmeHtml `
        -CurrentLanguageCode "en" `
        -Languages $englishLanguage
    Set-Content `
        -LiteralPath (Join-Path $packageRoot "README.html") `
        -Value $packageReadmeHtml `
        -Encoding utf8
    Copy-Item -LiteralPath $ChangelogPath -Destination (Join-Path $packageRoot "CHANGELOG.md") -Force
    Copy-OriginalMapReadmesToPackage -PackageRoot $packageRoot

    $versionManifest = [ordered]@{
        Version = $TargetVersion
        TagName = "v$TargetVersion"
        Repository = $ReleaseRepo
        Channel = $Channel
        SourceCommit = $SourceCommit
        SourceTreeDirty = $SourceTreeDirty
        ReleaseUrl = "https://github.com/$ReleaseRepo/releases/tag/v$TargetVersion"
        AssetName = "$packageName.zip"
        PackagedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    }
    Set-Content -LiteralPath (Join-Path $baseq2 "muffmode-version.json") -Value ($versionManifest | ConvertTo-Json) -Encoding utf8
    Set-Content -LiteralPath (Join-Path $baseq2 "muffmode.version") -Value $TargetVersion -Encoding utf8
    Set-Content -LiteralPath (Join-Path $packageRoot "MuffMode.version") -Value $TargetVersion -Encoding utf8
    Set-Content -LiteralPath (Join-Path $packageRoot "VERSION") -Value $TargetVersion -Encoding utf8

    Assert-ReleasePackageContents -PackageRoot $packageRoot
    Assert-ExistingUpdaterCompatiblePackageRoot -PackageRoot $packageRoot

    Write-Step "Validating staged map-pool and BSP assets"
    & (Resolve-RepoPath "scripts/ci/check-map-pool-examples.ps1") `
        -PackageRoot $packageRoot

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

function New-ReleaseProvenanceFiles {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string]$SourceCommit,
        [bool]$SourceTreeDirty,
        [string[]]$AssetPaths,
        [string]$OutputRoot
    )

    if ($SourceCommit -cnotmatch "^[0-9a-f]{40}$") {
        throw "Release provenance requires a lowercase 40-character source commit."
    }

    $assetRecords = @(
        $AssetPaths |
            ForEach-Object {
                $asset = Get-Item -LiteralPath $_ -ErrorAction Stop
                if (-not $asset.PSIsContainer) {
                    $assetHash = Get-FileHash -Algorithm SHA256 -LiteralPath $asset.FullName
                    [ordered]@{
                        Name = $asset.Name
                        Sha256 = $assetHash.Hash.ToLowerInvariant()
                        Size = [Int64]$asset.Length
                    }
                }
            } |
            Sort-Object { $_.Name }
    )
    if ($assetRecords.Count -ne $AssetPaths.Count) {
        throw "Release provenance requires every asset path to name a file."
    }
    $uniqueAssetNames = @($assetRecords.Name | Sort-Object -Unique)
    if ($uniqueAssetNames.Count -ne $assetRecords.Count) {
        throw "Release provenance asset names must be unique."
    }

    $packageName = Get-ReleasePackageName -TargetVersion $TargetVersion -Channel $Channel
    $outputRootAbs = Resolve-FullPath $OutputRoot
    $provenancePath = Join-Path $outputRootAbs "$packageName-provenance.json"
    $checksumsPath = Join-Path $outputRootAbs "$packageName-SHA256SUMS.txt"

    $provenance = [ordered]@{
        SchemaVersion = 1
        Repository = $ReleaseRepo
        Version = $TargetVersion
        TagName = "v$TargetVersion"
        Channel = $Channel
        SourceCommit = $SourceCommit
        SourceTreeDirty = $SourceTreeDirty
        Assets = $assetRecords
    }
    $provenance | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath $provenancePath -Encoding utf8

    $checksumRecords = @($assetRecords) + @(
        [ordered]@{
            Name = [System.IO.Path]::GetFileName($provenancePath)
            Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $provenancePath).Hash.ToLowerInvariant()
        }
    )
    $checksumLines = @(
        $checksumRecords |
            Sort-Object { $_.Name } |
            ForEach-Object { "$($_.Sha256)  $($_.Name)" }
    )
    # Keep the conventional manifest portable for `sha256sum -c` on Unix as
    # well as Windows: UTF-8 without a BOM and explicit LF line endings.
    [System.IO.File]::WriteAllText(
        $checksumsPath,
        ($checksumLines -join "`n") + "`n",
        [System.Text.UTF8Encoding]::new($false))

    return [pscustomobject]@{
        ProvenancePath = $provenancePath
        ChecksumsPath = $checksumsPath
    }
}

function New-WindowsInstaller {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string]$PackageRoot,
        [string]$ReadmeHtmlPath,
        [object[]]$TranslatedReadmes,
        [string]$OutputRoot,
        [string]$InstallerScript,
        [string]$InnoSetupCompiler
    )

    $scriptPath = Resolve-RepoPath $InstallerScript
    if (-not (Test-Path -LiteralPath $scriptPath)) {
        throw "Installer script was not found: $scriptPath"
    }

    $hasTranslatedReadmes = @($TranslatedReadmes).Count -gt 0
    if ($hasTranslatedReadmes) {
        Copy-Item `
            -LiteralPath $ReadmeHtmlPath `
            -Destination (Join-Path $PackageRoot "README.html") `
            -Force
        foreach ($translatedReadme in @($TranslatedReadmes)) {
            Copy-Item `
                -LiteralPath $translatedReadme.Path `
                -Destination (Join-Path $PackageRoot $translatedReadme.PackageFileName) `
                -Force
        }
    }

    Assert-ReleasePackageContents `
        -PackageRoot $PackageRoot `
        -RequireLocalizedReadmes:$hasTranslatedReadmes

    $localizedReadmeNames = @(
        Get-ReadmeTranslationLanguages |
            ForEach-Object { $_.PackageFileName }
    )
    $presentLocalizedReadmes = @(
        $localizedReadmeNames |
            Where-Object {
                Test-Path -LiteralPath (Join-Path $PackageRoot $_) -PathType Leaf
            }
    )
    if ($presentLocalizedReadmes.Count -ne 0 -and
        $presentLocalizedReadmes.Count -ne $localizedReadmeNames.Count) {
        $missingLocalizedReadmes = @(
            $localizedReadmeNames |
                Where-Object { $presentLocalizedReadmes -notcontains $_ }
        )
        throw "Staged package has only part of the localized README set. Missing: $($missingLocalizedReadmes -join ', ')."
    }
    $includeLocalizedReadmes =
        $localizedReadmeNames.Count -gt 0 -and
        $presentLocalizedReadmes.Count -eq $localizedReadmeNames.Count

    $packageName = Get-ReleasePackageName -TargetVersion $TargetVersion -Channel $Channel
    $outputRootAbs = Resolve-FullPath $OutputRoot
    $compiler = Resolve-InnoSetupCompiler -CompilerPath $InnoSetupCompiler
    New-Item -ItemType Directory -Force -Path $outputRootAbs | Out-Null
    $launcherIconFile = Resolve-FullPath "updater/MuffMode.Updater/Assets/MuffModeLauncher.ico"
    if (-not (Test-Path -LiteralPath $launcherIconFile -PathType Leaf)) {
        throw "Launcher icon file was not found: $launcherIconFile"
    }
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "Muff Mode v$TargetVersion $channelName" } else { "Muff Mode v$TargetVersion" }
    $installerBaseName = "$packageName-windows-installer"
    $installerPath = Join-Path $outputRootAbs "$installerBaseName.exe"
    $installerLogPath = Join-Path $outputRootAbs "$installerBaseName.innosetup.log"

    if (Test-Path -LiteralPath $installerPath) {
        Remove-Item -LiteralPath $installerPath -Force
    }
    if (Test-Path -LiteralPath $installerLogPath) {
        Remove-Item -LiteralPath $installerLogPath -Force
    }

    Write-Step "Creating Windows installer $installerPath"
    $installerArguments = @(
        "/DAppVersion=$TargetVersion"
        "/DChannel=$Channel"
        "/DReleaseLabel=$releaseLabel"
        "/DPackageRoot=$PackageRoot"
        "/DOutputDir=$outputRootAbs"
        "/DInstallerBaseName=$installerBaseName"
        "/DLauncherIconFile=$launcherIconFile"
    )
    if ($includeLocalizedReadmes) {
        $installerArguments += "/DIncludeLocalizedReadmes"
    }
    $installerArguments += $scriptPath
    $installerOutput = & $compiler @installerArguments 2>&1

    $installerExitCode = $LASTEXITCODE
    @($installerOutput) | Set-Content -LiteralPath $installerLogPath -Encoding utf8
    $installerOutput | ForEach-Object { Write-Host $_ }

    if ($installerExitCode -ne 0) {
        throw "Inno Setup failed while creating the Windows installer. See $installerLogPath"
    }
    if (-not (Test-Path -LiteralPath $installerPath)) {
        throw "Expected installer was not created: $installerPath"
    }

    Assert-WindowsExecutableImage -Path $installerPath -Description "Generated Windows installer" -AllowedMachines @([UInt16]0x014c, [UInt16]0x8664) -ImageKind Executable
    Write-Host "Installer compiler log: $installerLogPath"
    return $installerPath
}

function Get-RemoteReleaseTagCommit {
    param(
        [string]$Repository,
        [string]$TagName
    )

    $referenceJson = (& gh api "repos/$Repository/git/ref/tags/$TagName" 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) {
        return $null
    }

    try {
        $reference = $referenceJson | ConvertFrom-Json
    }
    catch {
        throw "GitHub returned malformed JSON while resolving release tag '$TagName'."
    }

    $target = $reference.object
    for ($depth = 0; $depth -lt 8; $depth++) {
        $targetType = [string]$target.type
        $targetSha = ([string]$target.sha).ToLowerInvariant()
        if ($targetSha -cnotmatch "^[0-9a-f]{40}$") {
            throw "GitHub release tag '$TagName' resolved to an invalid object SHA."
        }
        if ($targetType -ceq "commit") {
            return $targetSha
        }
        if ($targetType -cne "tag") {
            throw "GitHub release tag '$TagName' resolved to unsupported object type '$targetType'."
        }

        $tagJson = (& gh api "repos/$Repository/git/tags/$targetSha" 2>$null | Out-String).Trim()
        if ($LASTEXITCODE -ne 0) {
            throw "Could not peel annotated GitHub release tag '$TagName'."
        }
        try {
            $target = ($tagJson | ConvertFrom-Json).object
        }
        catch {
            throw "GitHub returned malformed JSON while peeling release tag '$TagName'."
        }
    }

    throw "GitHub release tag '$TagName' exceeded the supported annotated-tag nesting depth."
}

function Assert-RemoteReleaseTag {
    param(
        [string]$Repository,
        [string]$TagName,
        [string]$SourceCommit
    )

    if ($TagName -cnotmatch "^v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$" -or
        $SourceCommit -cnotmatch "^[0-9a-f]{40}$") {
        throw "Remote release tag creation requires a canonical version tag and lowercase source commit."
    }

    $remoteCommit = Get-RemoteReleaseTagCommit -Repository $Repository -TagName $TagName
    if ($null -eq $remoteCommit) {
        & gh api `
            --method POST `
            "repos/$Repository/git/refs" `
            -f "ref=refs/tags/$TagName" `
            -f "sha=$SourceCommit" *> $null
        if ($LASTEXITCODE -ne 0) {
            # A concurrent publisher may have created the tag after our read.
            # Re-resolve it and accept only the intended immutable commit.
            $remoteCommit = Get-RemoteReleaseTagCommit -Repository $Repository -TagName $TagName
            if ($null -eq $remoteCommit) {
                throw "Could not create or resolve GitHub release tag '$TagName'."
            }
        }
        else {
            $remoteCommit = Get-RemoteReleaseTagCommit -Repository $Repository -TagName $TagName
            if ($null -eq $remoteCommit) {
                throw "GitHub release tag '$TagName' was created but could not be verified."
            }
        }
    }

    if ($remoteCommit -cne $SourceCommit) {
        throw "GitHub release tag '$TagName' resolves to $remoteCommit, not packaged source commit $SourceCommit."
    }
}

function Publish-GitHubRelease {
    param(
        [string]$TargetVersion,
        [string]$Channel,
        [string]$SourceCommit,
        [string[]]$AssetPaths,
        [string]$ReleaseNotesPath,
        [bool]$Prerelease
    )

    Assert-Command "gh"
    $dirty = Get-GitStatus
    if ($dirty) {
        throw "Working tree is dirty. Commit release/version changes before publishing a GitHub release."
    }
    if ($SourceCommit -cnotmatch "^[0-9a-f]{40}$") {
        throw "GitHub release publishing requires a lowercase 40-character source commit."
    }

    $tagName = "v$TargetVersion"
    Assert-RemoteReleaseTag `
        -Repository $ReleaseRepo `
        -TagName $tagName `
        -SourceCommit $SourceCommit

    $args = @(
        "release", "create", $tagName
    )
    $args += $AssetPaths
    $args += @(
        "--repo", $ReleaseRepo,
        "--verify-tag",
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

    if ($Prerelease -and $Channel -eq "stable") {
        throw "-Prerelease cannot be combined with -Channel stable because stable asset names are not discoverable as prerelease updates. Choose alpha, beta, or rc."
    }
    if ($SkipUpdaterBuild -and ($CreateGitHubRelease -or $env:GITHUB_ACTIONS -ceq "true")) {
        throw "-SkipUpdaterBuild is limited to local unpublished packages because a reused updater executable is not bound to the current source commit."
    }
    if (-not [string]::IsNullOrWhiteSpace($PreviousTag)) {
        if ($PreviousTag.StartsWith("-", [System.StringComparison]::Ordinal)) {
            throw "Previous release tag must not begin with '-': '$PreviousTag'."
        }
        git -C $RepoRoot check-ref-format "refs/tags/$PreviousTag" *> $null
        if ($LASTEXITCODE -ne 0) {
            throw "Previous release tag is not a valid Git tag name: '$PreviousTag'."
        }
    }

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

    $sourceCommit = Get-SourceCommit
    $sourceTreeDirty = [bool](Get-GitStatus)
    $buildReceipt = Get-ReleaseBuildReceiptPath `
        -Configuration $Configuration `
        -Platform $Platform `
        -RequestedPath $BuildReceiptPath

    if ($SkipBuild) {
        $dllPath = Get-GameDllBuildPath -Configuration $Configuration -Platform $Platform
        if (-not (Test-Path -LiteralPath $dllPath)) {
            throw "-SkipBuild was supplied, but game_x64.dll does not exist at $dllPath."
        }
        if ((Test-Path -LiteralPath $buildReceipt -PathType Leaf) -or $CreateGitHubRelease -or $env:GITHUB_ACTIONS) {
            Assert-ReleaseBuildReceipt `
                -Path $buildReceipt `
                -SourceCommit $sourceCommit `
                -Configuration $Configuration `
                -Platform $Platform `
                -DllPath $dllPath
        }
        else {
            Write-Warning "Reusing game_x64.dll without a build receipt for a local, unpublished package. Published releases require a receipt bound to the current commit."
        }
    }
    else {
        $dllPath = Build-ReleaseDll -Configuration $Configuration -Platform $Platform
        Write-ReleaseBuildReceipt `
            -Path $buildReceipt `
            -SourceCommit $sourceCommit `
            -Configuration $Configuration `
            -Platform $Platform `
            -DllPath $dllPath `
            -WarningsAsErrors $true
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
    $translatedReadmes = @(New-TranslatedHtmlReadmes `
        -SourcePath $readmeHtmlPath `
        -TargetVersion $targetVersion `
        -Channel $Channel `
        -OutputRoot $OutputRoot)

    $package = New-ReleasePackage `
        -TargetVersion $targetVersion `
        -Channel $Channel `
        -SourceCommit $sourceCommit `
        -SourceTreeDirty $sourceTreeDirty `
        -DllPath $dllPath `
        -UpdaterPath $updaterPath `
        -ChangelogPath $releaseNotesPath `
        -ReadmeHtmlPath $readmeHtmlPath `
        -OutputRoot $OutputRoot `
        -AssetRoot $AssetRoot

    $mapArchives = New-MapSupplementArchives `
        -TargetVersion $targetVersion `
        -Channel $Channel `
        -OutputRoot $OutputRoot

    $installerPath = $null
    if ($SkipInstaller) {
        Write-Host "Windows installer not created because -SkipInstaller was supplied."
    }
    else {
        $installerPath = New-WindowsInstaller `
            -TargetVersion $targetVersion `
            -Channel $Channel `
            -PackageRoot $package.Root `
            -ReadmeHtmlPath $readmeHtmlPath `
            -TranslatedReadmes $translatedReadmes `
            -OutputRoot $OutputRoot `
            -InstallerScript $InstallerScript `
            -InnoSetupCompiler $InnoSetupCompiler
    }

    $releaseAssetPaths = New-Object System.Collections.Generic.List[string]
    $releaseAssetPaths.Add($package.ZipPath)
    if ($installerPath) {
        $releaseAssetPaths.Add($installerPath)
    }
    $releaseAssetPaths.Add($mapArchives.SourceMapsZipPath)
    $releaseAssetPaths.Add($mapArchives.OriginalMapsZipPath)

    $provenanceFiles = New-ReleaseProvenanceFiles `
        -TargetVersion $targetVersion `
        -Channel $Channel `
        -SourceCommit $sourceCommit `
        -SourceTreeDirty $sourceTreeDirty `
        -AssetPaths $releaseAssetPaths.ToArray() `
        -OutputRoot $OutputRoot
    $releaseAssetPaths.Add($provenanceFiles.ProvenancePath)
    $releaseAssetPaths.Add($provenanceFiles.ChecksumsPath)

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
    Write-Host "Source commit: $sourceCommit"
    Write-Host "Provenance:    $($provenanceFiles.ProvenancePath)"
    Write-Host "Checksums:     $($provenanceFiles.ChecksumsPath)"
    Write-Host "Notes:   $releaseNotesPath"
    Write-Host "README:  $readmeHtmlPath"
    foreach ($translatedReadme in $translatedReadmes) {
        Write-Host "README $($translatedReadme.Code): $($translatedReadme.Path)"
    }
    Write-Host "Updater: $updaterPath"

    if ($CreateGitHubRelease) {
        Publish-GitHubRelease -TargetVersion $targetVersion -Channel $Channel -SourceCommit $sourceCommit -AssetPaths $releaseAssetPaths.ToArray() -ReleaseNotesPath $releaseNotesPath -Prerelease $isPrerelease
    }
    else {
        Write-Host "GitHub release not created. Re-run with -CreateGitHubRelease to publish with --latest."
    }
}
finally {
    Pop-Location
}
