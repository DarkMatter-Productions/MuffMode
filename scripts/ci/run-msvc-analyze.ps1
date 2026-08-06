param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$analysisLogRelative = "build\analysis\msvc-analyze-$Configuration-$Platform.txt"
$analysisLog = Join-Path $repoRoot $analysisLogRelative
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $analysisLog) | Out-Null
Remove-Item -LiteralPath $analysisLog -Force -ErrorAction SilentlyContinue

& "$PSScriptRoot\build-msbuild.ps1" `
    -Configuration $Configuration `
    -Platform $Platform `
    -Target Rebuild `
    -TreatWarningsAsErrors `
    -RunCodeAnalysis `
    -BinaryLog `
    -BinaryLogPath "build\analysis\msvc-analyze-$Configuration-$Platform.binlog" `
    -AdditionalMsBuildArgs @(
        "/fl",
        "/flp:logfile=$analysisLog;verbosity=normal"
    )

if (-not (Test-Path -LiteralPath $analysisLog -PathType Leaf)) {
    throw "MSVC code-analysis log was not produced: $analysisLogRelative"
}

$blockingWarningsPath = Join-Path $repoRoot "scripts\ci\msvc-analyze-blocking-warnings.txt"
$blockingWarnings = @(Read-AnalyzerBlockingList `
    -Path $blockingWarningsPath -EntryPattern '^C\d{4,5}$' | ForEach-Object {
        $_.ToUpperInvariant()
    })

$blockingFindings = [System.Collections.Generic.List[string]]::new()
$seenBlockingFindings = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($line in Get-Content -LiteralPath $analysisLog) {
    $finding = Get-MsvcAnalyzerFinding -Line $line
    if ($finding -and $blockingWarnings -contains $finding.Code -and
        $seenBlockingFindings.Add($finding.Text)) {
        $blockingFindings.Add($finding.Text)
    }
}
if ($blockingFindings.Count -ne 0) {
    throw "MSVC /analyze reported $($blockingFindings.Count) blocking analyzer finding(s). See $analysisLogRelative."
}
