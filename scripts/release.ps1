[CmdletBinding()]
param(
    [ValidateSet("auto", "major", "minor", "patch")]
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

    [switch]$SkipBuild,
    [switch]$SkipUpdaterBuild,
    [switch]$SkipInstaller,
    [switch]$UpdateVersionFiles,
    [switch]$CreateGitHubRelease,
    [switch]$Prerelease,
    [switch]$AllowDirtyPackage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$ReleaseRepo = "DarkMatter-Productions/MuffMode"

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

function Assert-Command {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' was not found on PATH."
    }
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

function Invoke-GitHubCopilot {
    param(
        [string]$Prompt,
        [string]$Purpose,
        [string[]]$AllowedTools = @()
    )

    Assert-Command "gh"

    $args = @("copilot", "-s", "--no-ask-user")
    foreach ($tool in $AllowedTools) {
        $args += "--allow-tool=$tool"
    }

    $output = $Prompt | & gh @args 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub Copilot failed while $Purpose.`n$output"
    }

    return (Remove-AnsiSequences $output).Trim()
}

function Assert-GitHubCopilot {
    Assert-Command "gh"

    $output = & gh copilot -- --help 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub Copilot CLI is required for release changelog and README generation. Install and authenticate Copilot CLI so 'gh copilot' can run in non-interactive mode.`n$output"
    }
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

    $localHeader = Join-Path $RepoRoot "src/g_local.h"
    if (Test-Path -LiteralPath $localHeader) {
        $content = Get-Content -Raw -LiteralPath $localHeader
        if ($content -match 'GAMEMOD_VERSION\s*=\s*"([^"]+)"') {
            return $Matches[1]
        }
    }

    throw "Could not determine the current source version. Add VERSION or update src/g_local.h."
}

function Get-LatestReleaseTag {
    Assert-Command "gh"
    try {
        $json = gh release list --repo $ReleaseRepo --limit 50 --json tagName,isLatest,publishedAt 2>$null
        $releases = $json | ConvertFrom-Json
        if ($releases.Count -gt 0) {
            $latest = $releases | Sort-Object publishedAt -Descending | Select-Object -First 1
            if ($latest.tagName) {
                return $latest.tagName
            }
        }
    }
    catch {
        Write-Warning "Could not query GitHub releases through gh; falling back to local git tags."
    }

    $tag = git -C $RepoRoot tag --list "v*" --sort=-version:refname | Select-Object -First 1
    if (-not $tag) {
        throw "Could not find a previous release tag."
    }
    return $tag
}

function Resolve-TargetVersion {
    param([string]$LatestTag)

    if ($Version) {
        return (ConvertTo-SemVer $Version).Text
    }

    $latest = ConvertTo-SemVer $LatestTag
    $current = ConvertTo-SemVer (Get-CurrentSourceVersion)

    switch ($VersionMode) {
        "major" { return "$($latest.Major + 1).0.0" }
        "minor" { return "$($latest.Major).$($latest.Minor + 1).0" }
        "patch" { return "$($latest.Major).$($latest.Minor).$($latest.Patch + 1)" }
        "auto" {
            if ((Compare-SemVer $current $latest) -gt 0) {
                return $current.Text
            }
            return "$($latest.Major).$($latest.Minor).$($latest.Patch + 1)"
        }
    }
}

function Update-VersionFiles {
    param([string]$TargetVersion)

    $versionFile = Join-Path $RepoRoot "VERSION"
    Set-Content -LiteralPath $versionFile -Value $TargetVersion -Encoding utf8

    $localHeader = Join-Path $RepoRoot "src/g_local.h"
    $content = Get-Content -Raw -LiteralPath $localHeader
    $replacement = "constexpr const char *GAMEMOD_VERSION = `"$TargetVersion`";"
    $updated = [regex]::Replace(
        $content,
        'constexpr\s+const\s+char\s+\*GAMEMOD_VERSION\s*=\s*"[^"]+";',
        $replacement,
        1
    )
    if ($updated -eq $content) {
        throw "Could not update GAMEMOD_VERSION in src/g_local.h."
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

    $localHeader = Join-Path $RepoRoot "src/g_local.h"
    $content = Get-Content -Raw -LiteralPath $localHeader
    if ($content -notmatch "GAMEMOD_VERSION\s*=\s*`"$([regex]::Escape($TargetVersion))`"") {
        throw "src/g_local.h GAMEMOD_VERSION does not match $TargetVersion."
    }
}

function Resolve-PreviousTag {
    param([string]$TargetVersion, [string]$FallbackLatestTag)

    if ($PreviousTag) {
        git -C $RepoRoot rev-parse "$PreviousTag^{commit}" *> $null
        if ($LASTEXITCODE -ne 0) {
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

function Build-ReleaseDll {
    param([string]$Configuration, [string]$Platform)

    Assert-Command "msbuild"
    $solution = Join-Path $RepoRoot "src/MuffMode.sln"
    Write-Step "Building $Configuration|$Platform"
    & msbuild $solution "/p:Configuration=$Configuration" "/p:Platform=$Platform"
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed."
    }

    $dll = Join-Path $RepoRoot "game_x64.dll"
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
    & dotnet publish $projectPath `
        -c $Configuration `
        -r $Runtime `
        --self-contained true `
        -p:PublishSingleFile=true `
        -p:IncludeNativeLibrariesForSelfExtract=true `
        -p:DebugType=embedded `
        -p:DebugSymbols=false `
        -o $publishRoot

    if ($LASTEXITCODE -ne 0) {
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

function Get-ReleaseChangeContext {
    param(
        [string]$TargetVersion,
        [string]$PreviousTag
    )

    $range = "$PreviousTag..HEAD"
    $compareUrl = "https://github.com/$ReleaseRepo/compare/$PreviousTag...v$TargetVersion"

    $commits = (git -C $RepoRoot log --date=short --pretty=format:'%h%x09%ad%x09%s%x09%an' $range | Out-String).Trim()
    if (-not $commits) {
        throw "No commits found in $range. Refusing to create an empty changelog."
    }

    $nonMergeDetails = (git -C $RepoRoot log --no-merges --date=short --pretty=format:'---%ncommit %h%nDate: %ad%nSubject: %s%nBody:%n%b' --name-status $range | Out-String).Trim()
    $mergeCommits = (git -C $RepoRoot log --merges --date=short --pretty=format:'%h%x09%ad%x09%s' $range | Out-String).Trim()
    $shortStat = (git -C $RepoRoot diff --shortstat $range | Out-String).Trim()
    $diffStat = (git -C $RepoRoot diff --find-renames --stat $range | Out-String).Trim()
    $nameStatus = (git -C $RepoRoot diff --find-renames --name-status $range | Out-String).Trim()

    return @"
Release target: Muff Mode v$TargetVersion
Previous release tag: $PreviousTag
Git range: $range
Compare URL: $compareUrl

COMMIT SUMMARY
$(Limit-Text -Text $commits -MaxCharacters 18000)

MERGE COMMITS
$(Limit-Text -Text $mergeCommits -MaxCharacters 8000)

OVERALL SHORT STAT
$shortStat

CHANGED FILES
$(Limit-Text -Text $nameStatus -MaxCharacters 20000)

DIFF STAT
$(Limit-Text -Text $diffStat -MaxCharacters 20000)

NON-MERGE COMMIT DETAILS
$(Limit-Text -Text $nonMergeDetails -MaxCharacters 55000)
"@
}

function Convert-CopilotOutputToMarkdown {
    param(
        [string]$Output,
        [string]$TargetVersion,
        [string]$PreviousTag
    )

    $clean = (Remove-AnsiSequences $Output).Trim()
    $fence = [regex]::Match($clean, '(?is)```(?:markdown|md)?\s*(.*?)\s*```')
    if ($fence.Success) {
        $clean = $fence.Groups[1].Value.Trim()
    }
    else {
        $heading = [regex]::Match($clean, '(?m)^#\s+.+$')
        if ($heading.Success -and $heading.Index -gt 0) {
            $clean = $clean.Substring($heading.Index).Trim()
        }
    }

    if ($clean -notmatch '(?m)^#\s+') {
        throw "GitHub Copilot did not return a Markdown changelog with a title."
    }
    if ($clean -notmatch '(?m)^##\s+') {
        throw "GitHub Copilot did not return category headings for the changelog."
    }
    if ($clean -notmatch '(?m)^\s*-\s+\S') {
        throw "GitHub Copilot did not return bullet-point release notes."
    }
    if ($clean -notmatch [regex]::Escape($PreviousTag)) {
        throw "GitHub Copilot changelog does not mention the previous release tag $PreviousTag."
    }
    if ($clean -notmatch [regex]::Escape($TargetVersion)) {
        throw "GitHub Copilot changelog does not mention target version $TargetVersion."
    }
    if ($clean -match '(?i)```|as an ai|i can(?:not|''t)|i am unable') {
        throw "GitHub Copilot returned conversational or fenced output instead of clean release notes."
    }

    return $clean.Trim()
}

function New-ReleaseChangelog {
    param(
        [string]$TargetVersion,
        [string]$PreviousTag,
        [string]$Channel,
        [string]$OutputPath
    )

    Write-Step "Compiling Copilot changelog from $PreviousTag..HEAD"
    $range = "$PreviousTag..HEAD"
    $compareUrl = "https://github.com/$ReleaseRepo/compare/$PreviousTag...v$TargetVersion"
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "Muff Mode v$TargetVersion $channelName" } else { "Muff Mode v$TargetVersion" }
    $changeContext = Get-ReleaseChangeContext -TargetVersion $TargetVersion -PreviousTag $PreviousTag

    $prompt = @"
You are GitHub Copilot preparing public release notes for $releaseLabel.

Inspect the supplied git range context and write an elegant Markdown changelog for the release package and GitHub release notes.

Audience:
- Casual Quake II Remastered players who want to know what feels better or easier.
- Competitive players who care about match flow, rulesets, balance, teams, voting, and reliability.
- Server hosts who care about setup, cvars, configs, diagnostics, packaging, and admin controls.

Scope rules:
- Use only changes from the supplied range: $range.
- Include the compare link: $compareUrl.
- This project is currently in the $Channel channel. Make the beta/release state visible in the title or intro when relevant.
- Summarize changes by practical impact; do not dump every commit.
- Combine duplicate or related commits into one clear bullet.
- If a change is internal, explain why it matters to players, competitive matches, or server hosts. If it has no practical user-facing effect, keep it under "Internal Maintenance".
- Do not invent features or fixes that are not supported by the context.
- Do not include build instructions, source compilation steps, contributor workflow, badges, or marketing fluff.

Category guidance:
- Start with "# $releaseLabel Changelog".
- Include a short intro that says these are changes since $PreviousTag.
- Use only category headings that have real content.
- Prefer these headings when relevant:
  ## Highlights
  ## Player Experience
  ## Competitive Play
  ## Server Hosting
  ## Gameplay and Balance
  ## Maps and Content
  ## Fixes
  ## Documentation and Packaging
  ## Internal Maintenance
- Use concise bullets. Lead with the user-facing result, then add technical context only when it helps.

Output requirements:
- Return only Markdown.
- Do not wrap the output in a code fence.
- Include the compare link near the top.

GIT RANGE CONTEXT:
$changeContext
"@

    $output = Invoke-GitHubCopilot `
        -Prompt $prompt `
        -Purpose "generating the release changelog" `
        -AllowedTools @("shell(git:*)")

    $markdown = Convert-CopilotOutputToMarkdown `
        -Output $output `
        -TargetVersion $TargetVersion `
        -PreviousTag $PreviousTag

    Set-Content -LiteralPath $OutputPath -Value $markdown -Encoding utf8
}

function Get-ReadmeSourceMarkdown {
    $files = @(
        "README.md",
        "docs/player-guide.md",
        "docs/server-host-guide.md",
        "docs/gameplay-reference.md",
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

    Assert-Command "gh"

    $docs = Get-ReadmeSourceMarkdown
    $changelog = Get-Content -Raw -LiteralPath $ChangelogPath
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "MuffMode v$TargetVersion $channelName" } else { "MuffMode v$TargetVersion" }

    $prompt = @"
You are GitHub Copilot helping prepare a public release package for $releaseLabel.

Create a complete standalone HTML document for end users. Use the Markdown documentation and changelog below as source material.

Audience and scope:
- Primary audience: Quake II Remastered players and server hosts installing this release.
- This project is currently in $Channel channel. Make that release state visible but not alarming.
- Include installation, first-use guidance, player usage, voting, common host setup, gametype overview, ruleset overview, offhand hook bind, debugging pointer, package contents, and the changelog.
- Explain that most Windows users can use the installer, which defaults to Steam and offers Epic Online Store / Epic Games Store, GOG, and custom folder choices. Also include the zip/manual extraction path for users who prefer it.
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

    Write-Step "Generating end-user README.html with GitHub Copilot"
    $output = Invoke-GitHubCopilot -Prompt $prompt -Purpose "generating README.html"

    $html = Convert-CopilotOutputToHtml $output
    Set-Content -LiteralPath $OutputPath -Value $html -Encoding utf8
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
    $outputRootAbs = Resolve-RepoPath $OutputRoot
    $stagingRoot = Join-Path $outputRootAbs "staging"
    $packageRoot = Join-Path $stagingRoot $packageName

    if (Test-Path -LiteralPath $packageRoot) {
        Remove-Item -LiteralPath $packageRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

    Copy-ReleaseAssets -AssetRoot $AssetRoot -PackageRoot $packageRoot

    $baseq2 = Join-Path $packageRoot "rerelease/baseq2"
    New-Item -ItemType Directory -Force -Path $baseq2 | Out-Null

    Copy-Item -LiteralPath $DllPath -Destination (Join-Path $baseq2 "game_x64.dll") -Force
    Copy-Item -LiteralPath $UpdaterPath -Destination (Join-Path $packageRoot "MuffModeUpdater.exe") -Force
    Copy-Item -LiteralPath $ReadmeHtmlPath -Destination (Join-Path $packageRoot "README.html") -Force
    Copy-Item -LiteralPath $ChangelogPath -Destination (Join-Path $packageRoot "CHANGELOG.md") -Force

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

    $packageName = Get-ReleasePackageName -TargetVersion $TargetVersion -Channel $Channel
    $outputRootAbs = Resolve-RepoPath $OutputRoot
    $compiler = Resolve-InnoSetupCompiler -CompilerPath $InnoSetupCompiler
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "Muff Mode v$TargetVersion $channelName" } else { "Muff Mode v$TargetVersion" }
    $installerBaseName = "$packageName-windows-installer"
    $installerPath = Join-Path $outputRootAbs "$installerBaseName.exe"

    if (Test-Path -LiteralPath $installerPath) {
        Remove-Item -LiteralPath $installerPath -Force
    }

    Write-Step "Creating Windows installer $installerPath"
    & $compiler `
        "/DAppVersion=$TargetVersion" `
        "/DChannel=$Channel" `
        "/DReleaseLabel=$releaseLabel" `
        "/DPackageRoot=$PackageRoot" `
        "/DOutputDir=$outputRootAbs" `
        "/DInstallerBaseName=$installerBaseName" `
        $scriptPath

    if ($LASTEXITCODE -ne 0) {
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
        "--latest",
        "--fail-on-no-commits"
    )
    if ($Prerelease) {
        $args += "--prerelease"
    }

    $releaseState = if ($Prerelease) { "latest prerelease" } else { "latest stable release" }
    Write-Step "Publishing GitHub release v$TargetVersion as $releaseState"
    & gh @args
    if ($LASTEXITCODE -ne 0) {
        throw "gh release create failed."
    }
}

Push-Location $RepoRoot
try {
    Assert-Command "git"
    Assert-Command "gh"
    Assert-GitHubCopilot

    $dirty = Get-GitStatus
    if ($dirty -and -not $AllowDirtyPackage -and -not $UpdateVersionFiles) {
        throw "Working tree is dirty. Commit changes or pass -AllowDirtyPackage for a local package build."
    }

    $latestReleaseTag = Get-LatestReleaseTag
    $targetVersion = Resolve-TargetVersion -LatestTag $latestReleaseTag
    $previousReleaseTag = Resolve-PreviousTag -TargetVersion $targetVersion -FallbackLatestTag $latestReleaseTag
    $isPrerelease = (Test-IsPrereleaseChannel -Channel $Channel) -or [bool]$Prerelease

    Write-Step "Latest release tag: $latestReleaseTag"
    Write-Step "Previous changelog tag: $previousReleaseTag"
    Write-Step "Target version: $targetVersion ($Channel; GitHub prerelease: $isPrerelease)"

    if ($UpdateVersionFiles) {
        Write-Step "Updating VERSION and src/g_local.h"
        Update-VersionFiles -TargetVersion $targetVersion
    }
    Assert-VersionFilesMatch -TargetVersion $targetVersion

    if ($CreateGitHubRelease) {
        $dirtyAfterVersion = Get-GitStatus
        if ($dirtyAfterVersion) {
            throw "Version files or other files are uncommitted. Commit them before publishing. You can still build the package without -CreateGitHubRelease."
        }
    }

    if ($SkipBuild) {
        $dllPath = Join-Path $RepoRoot "game_x64.dll"
        if (-not (Test-Path -LiteralPath $dllPath)) {
            throw "-SkipBuild was supplied, but game_x64.dll does not exist at the repository root."
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

    New-ReleaseChangelog -TargetVersion $targetVersion -PreviousTag $previousReleaseTag -Channel $Channel -OutputPath $releaseNotesPath
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
    Write-Step "Package ready"
    Write-Host "Package: $($package.ZipPath)"
    Write-Host "SHA256:  $($hash.Hash.ToLowerInvariant())"
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
