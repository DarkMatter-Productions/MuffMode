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

    [switch]$SkipBuild,
    [switch]$SkipUpdaterBuild,
    [switch]$SkipInstaller,
    [switch]$UpdateVersionFiles,
    [switch]$VersionOnly,
    [switch]$CreateGitHubRelease,
    [switch]$Prerelease,
    [switch]$RequireCopilot,
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
        throw "GitHub Copilot CLI is required for release changelog and README generation. Install @github/copilot and authenticate it so 'copilot' can run in non-interactive mode.`n$output"
    }
}

function Test-GitHubCopilotCommand {
    return [bool](Get-Command "copilot" -ErrorAction SilentlyContinue)
}

function ConvertTo-ReleaseSentence {
    param([string]$Text)

    $clean = ($Text -replace '^(feat|feature|fix|docs|doc|chore|ci|build|refactor|style|test)(\([^)]+\))?!?:\s*', '').Trim()
    if ([string]::IsNullOrWhiteSpace($clean)) {
        return $Text.Trim()
    }

    if ($clean.Length -eq 1) {
        return $clean.ToUpperInvariant()
    }

    return "$($clean.Substring(0, 1).ToUpperInvariant())$($clean.Substring(1))"
}

function Get-DeterministicReleaseCategory {
    param([string]$Subject)

    $text = $Subject.ToLowerInvariant()

    if ($text -match '\b(server|host|hosting|admin|cvar|config|configs|diagnostic|doctor|updater|installer)\b') {
        return "Server Hosting"
    }
    if ($text -match '\b(competitive|match|duel|tdm|team|captain|ready|timeout|overtime|ruleset|vote|voting)\b') {
        return "Competitive Play"
    }
    if ($text -match '\b(gametype|weapon|balance|item|map|entity|horde|arena|hook|grapple)\b') {
        return "Gameplay and Balance"
    }
    if ($text -match '\b(fix|bug|crash|error|resolve|repair|correct)\b') {
        return "Fixes"
    }
    if ($text -match '\b(doc|docs|readme|release|package|workflow|changelog|installer|asset)\b') {
        return "Documentation and Packaging"
    }

    return "Internal Maintenance"
}

function New-DeterministicReleaseChangelog {
    param(
        [string]$TargetVersion,
        [string]$PreviousTag,
        [string]$Channel,
        [string]$OutputPath
    )

    Write-Step "Compiling deterministic changelog from $PreviousTag..HEAD"
    $range = "$PreviousTag..HEAD"
    $compareUrl = "https://github.com/$ReleaseRepo/compare/$PreviousTag...v$TargetVersion"
    $channelName = Get-ChannelDisplayName -Channel $Channel
    $releaseLabel = if ($channelName) { "Muff Mode v$TargetVersion $channelName" } else { "Muff Mode v$TargetVersion" }

    $commitLines = git -C $RepoRoot log --no-merges --date=short --pretty=format:'%h%x09%s' $range
    if (-not $commitLines) {
        $commitLines = git -C $RepoRoot log --date=short --pretty=format:'%h%x09%s' $range
    }
    if (-not $commitLines) {
        throw "No commits found in $range. Refusing to create an empty changelog."
    }

    $categories = [ordered]@{
        "Highlights" = New-Object System.Collections.Generic.List[string]
        "Player Experience" = New-Object System.Collections.Generic.List[string]
        "Competitive Play" = New-Object System.Collections.Generic.List[string]
        "Server Hosting" = New-Object System.Collections.Generic.List[string]
        "Gameplay and Balance" = New-Object System.Collections.Generic.List[string]
        "Fixes" = New-Object System.Collections.Generic.List[string]
        "Documentation and Packaging" = New-Object System.Collections.Generic.List[string]
        "Internal Maintenance" = New-Object System.Collections.Generic.List[string]
    }

    $seen = New-Object System.Collections.Generic.HashSet[string]
    foreach ($line in $commitLines) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line -split "`t", 2
        if ($parts.Count -lt 2) { continue }

        $hash = $parts[0].Trim()
        $subject = $parts[1].Trim()
        if ([string]::IsNullOrWhiteSpace($subject)) { continue }
        if ($subject -match '^Bump Muff Mode to v') { continue }

        $summary = ConvertTo-ReleaseSentence $subject
        if (-not $seen.Add($summary.ToLowerInvariant())) { continue }

        $category = Get-DeterministicReleaseCategory $subject
        if ($categories[$category].Count -lt 8) {
            $categories[$category].Add("- $summary ([``$hash``](https://github.com/$ReleaseRepo/commit/$hash))")
        }
    }

    if ($categories["Highlights"].Count -eq 0) {
        foreach ($categoryName in @("Player Experience", "Competitive Play", "Server Hosting", "Gameplay and Balance", "Fixes", "Documentation and Packaging")) {
            if ($categories[$categoryName].Count -gt 0 -and $categories["Highlights"].Count -lt 3) {
                $plain = $categories[$categoryName][0] -replace '\s+\(\[`?[0-9a-f]+`?\]\([^)]+\)\)$', ''
                $categories["Highlights"].Add($plain)
            }
        }
    }

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# $releaseLabel Changelog")
    $lines.Add("")
    $lines.Add("These are the notable changes since $PreviousTag for the $Channel release channel.")
    $lines.Add("")
    $lines.Add("Compare: [$PreviousTag...v$TargetVersion]($compareUrl)")

    foreach ($categoryName in $categories.Keys) {
        if ($categories[$categoryName].Count -eq 0) { continue }
        $lines.Add("")
        $lines.Add("## $categoryName")
        foreach ($entry in $categories[$categoryName]) {
            $lines.Add($entry)
        }
    }

    Set-Content -LiteralPath $OutputPath -Value ($lines -join "`n") -Encoding utf8
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
    <div class="eyebrow">Quake II Remastered server-side mod</div>
    <h1>$encodedLabel</h1>
    <p class="lede">A practical release package for casual games, competitive matches, and the server hosts keeping MuffMode sessions running. This package is flagged as <span class="tag">$Channel</span>.</p>
  </header>
  <main>
    <section>
      <h2>Install</h2>
      <div class="grid">
        <article class="card">
          <h3>Windows Installer</h3>
          <p>Use the installer for the cleanest setup. It shows detected Steam, Epic Online Store, and GOG installs, keeps an other-location option available, and can create updater and launcher shortcuts.</p>
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
      <h2>Gametype And Ruleset Notes</h2>
      <div class="grid">
        <article class="card"><strong>Common gametypes:</strong> FFA, Duel, TDM, CTF, Clan Arena, Freeze Tag, CaptureStrike, Red Rover, LMS, Horde, ProBall, Instagib, and NadeFest.</article>
        <article class="card"><strong>Rulesets:</strong> Quake II Rerelease, Muff Mode, Quake III Arena style, Q2RE Balanced, Quake style, and Quake Champions style.</article>
      </div>
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
        if ($content -match "GAMEMOD_VERSION\s*=\s*`"$([regex]::Escape($TargetVersion))`"") {
            return
        }
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

function Build-ReleaseDll {
    param([string]$Configuration, [string]$Platform)

    Assert-Command "msbuild"
    $solution = Join-Path $RepoRoot "src/MuffMode.sln"
    Write-Step "Building $Configuration|$Platform"
    $buildOutput = & msbuild $solution "/p:Configuration=$Configuration" "/p:Platform=$Platform" 2>&1
    $buildExitCode = $LASTEXITCODE
    $buildOutput | ForEach-Object { Write-Host $_ }
    if ($buildExitCode -ne 0) {
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

    if (Test-GitHubCopilotCommand) {
        try {
            $output = Invoke-GitHubCopilot `
                -Prompt $prompt `
                -Purpose "generating the release changelog" `
                -AllowedTools @("shell(git:*)")

            $markdown = Convert-CopilotOutputToMarkdown `
                -Output $output `
                -TargetVersion $TargetVersion `
                -PreviousTag $PreviousTag

            Set-Content -LiteralPath $OutputPath -Value $markdown -Encoding utf8
            return
        }
        catch {
            if ($RequireCopilot) {
                throw
            }
            Write-Warning "GitHub Copilot changelog generation failed; using deterministic release notes instead. $($_.Exception.Message)"
        }
    }
    else {
        if ($RequireCopilot) {
            throw "GitHub Copilot CLI is required because -RequireCopilot was supplied, but 'copilot' was not found on PATH."
        }
        Write-Warning "GitHub Copilot CLI was not found; using deterministic release notes instead."
    }

    New-DeterministicReleaseChangelog `
        -TargetVersion $TargetVersion `
        -PreviousTag $PreviousTag `
        -Channel $Channel `
        -OutputPath $OutputPath
}

function Get-ReadmeSourceMarkdown {
    $files = @(
        "README.md",
        "docs/player-guide.md",
        "docs/server-host-guide.md",
        "docs/gameplay-reference.md",
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
- Primary audience: Quake II Remastered players and server hosts installing this release.
- This project is currently in $Channel channel. Make that release state visible but not alarming.
- Include installation, first-use guidance, player usage, voting, common host setup, gametype overview, ruleset overview, offhand hook bind, debugging pointer, package contents, and the changelog.
- Include a compact "Included Custom Maps" section using the source map guide. Show map title, filename, release status, and good gametype fits, and link to the full Muff Mode Map Guide for history, original release dates, preserved original readmes/BSPs, separate remaster source-map links, and item registers.
- Explain that original map readmes are included in the main installer/manual zip under rerelease/baseq2/docs/muffmode/maps/original-readmes, while source maps and original BSPs are published as separate supplemental release archives.
- Explain that most Windows users can use the installer, which presents detected Steam, Epic Online Store, and GOG installs, keeps an other-location choice available, and offers Desktop/Start menu shortcuts for the updater and launcher. Also include the zip/manual extraction path for users who prefer it.
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

    foreach ($topLevelFile in @("README.html", "README.md", "CHANGELOG.md", "LICENSE", "MuffModeUpdater.exe", "MuffMode.version", "VERSION")) {
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
        "CHANGELOG.md",
        "LICENSE",
        "MuffModeUpdater.exe",
        "rerelease\baseq2\game_x64.dll",
        "rerelease\baseq2\muffmode-version.json",
        "rerelease\baseq2\muffmode.version",
        "rerelease\baseq2\docs\muffmode\maps\original-readmes\README.md"
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
        Write-Step "Updating VERSION and src/g_local.h"
        Update-VersionFiles -TargetVersion $targetVersion
    }
    Assert-VersionFilesMatch -TargetVersion $targetVersion

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
