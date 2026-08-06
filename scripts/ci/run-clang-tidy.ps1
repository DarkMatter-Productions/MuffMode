param(
    [string]$CompileCommands = "build\compile_commands.json",
    [string]$Output = "build\analysis\clang-tidy.txt",
    [string]$BlockingChecks = "scripts\ci\clang-tidy-blocking-checks.txt",
    [string]$ChangedSince,
    [string[]]$Files = @(),
    [switch]$AllowEmpty
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$compileCommandsPath = Join-Path $repoRoot $CompileCommands
if (-not (Test-Path -LiteralPath $compileCommandsPath)) {
    & "$PSScriptRoot\export-compile-commands.ps1" -Output $CompileCommands
}

$clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
if (-not $clangTidy) {
    throw "clang-tidy was not found on PATH."
}

$filesWereBound = $PSBoundParameters.ContainsKey('Files')
$changedSelection = -not [string]::IsNullOrWhiteSpace($ChangedSince)
$useFullDatabase = -not $filesWereBound -and -not $changedSelection
if ($Files.Count -eq 0 -and -not [string]::IsNullOrWhiteSpace($ChangedSince)) {
    Push-Location $repoRoot
    try {
        $changedPaths = @(git diff --name-only --diff-filter=ACMRT $ChangedSince -- `
            src `
            .clang-tidy `
            scripts/ci/clang-tidy-blocking-checks.txt `
            scripts/ci/common.ps1 `
            scripts/ci/export-compile-commands.ps1 `
            scripts/ci/run-clang-tidy.ps1 `
            projects/msvc/game.vcxproj `
            projects/msvc/MuffMode.Analysis.props)
        $gitExitCode = $LASTEXITCODE
        if ($gitExitCode -ne 0) {
            throw "git diff failed with exit code $gitExitCode while selecting clang-tidy files."
        }
        $forceFullAnalysis = @($changedPaths | Where-Object {
            $_ -notmatch '(?i)^src/'
        }).Count -ne 0
        $changedHeaders = @($changedPaths | Where-Object {
            $_ -match '(?i)\.(?:h|hh|hpp|hxx|inc)$'
        })
        if ($forceFullAnalysis -or $changedHeaders.Count -ne 0) {
            $useFullDatabase = $true
        }
        else {
            $Files = @($changedPaths | Where-Object {
                $_ -match '(?i)\.(?:c|cc|cpp|cxx)$'
            })
        }
    }
    finally {
        Pop-Location
    }
}

if ($Files.Count -eq 0 -and $useFullDatabase) {
    $database = Get-Content -Raw -Path $compileCommandsPath | ConvertFrom-Json
    $Files = @($database | ForEach-Object { $_.file })
}

$Files = @($Files | ForEach-Object {
    $path = $_
    if (-not [System.IO.Path]::IsPathRooted($path)) {
        $path = Join-Path $repoRoot $path
    }
    $path
} | Where-Object { Test-Path -LiteralPath $_ } | Sort-Object -Unique)

$databaseDir = Split-Path -Parent $compileCommandsPath
$configPath = Join-Path $repoRoot ".clang-tidy"
$blockingChecksPath = Join-Path $repoRoot $BlockingChecks
$blockingPatterns = @(Read-AnalyzerBlockingList `
    -Path $blockingChecksPath `
    -EntryPattern '^[A-Za-z0-9_.?*\-]+$')

$enabledOutput = @(& $clangTidy.Source `
    "--config-file=$configPath" `
    "--list-checks" `
    "--use-color=false" 2>&1)
$enabledExitCode = $LASTEXITCODE
if ($enabledExitCode -ne 0) {
    throw "clang-tidy could not list enabled checks (exit $enabledExitCode)."
}
$enabledChecks = @($enabledOutput | ForEach-Object {
    ([string]$_).Trim()
} | Where-Object {
    $_ -match '^[A-Za-z0-9_.-]+$' -and $_ -ne 'Enabled checks:'
})
foreach ($pattern in $blockingPatterns) {
    if (-not (Test-ClangTidyBlockingCheck `
            -Checks $enabledChecks -Patterns @($pattern))) {
        throw "clang-tidy blocking pattern '$pattern' matches no enabled check."
    }
}

$outputPath = Join-Path $repoRoot $Output
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null

if ($Files.Count -eq 0) {
    if ($AllowEmpty) {
        "No changed C++ source files to analyze." | Set-Content -Path $outputPath -Encoding utf8
        Write-Host "No changed C++ source files to analyze."
        exit 0
    }
    throw "No C++ source files were selected for clang-tidy."
}

$failed = $false
$blockingFindings = [System.Collections.Generic.List[string]]::new()
$seenBlockingFindings = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
"clang-tidy files: $($Files.Count)" | Set-Content -Path $outputPath -Encoding utf8

foreach ($file in $Files) {
    "== $file ==" | Tee-Object -FilePath $outputPath -Append | Out-Null
    $diagnostics = @(& $clangTidy.Source "-p=$databaseDir" `
        "--config-file=$configPath" "--use-color=false" $file 2>&1)
    $exitCode = $LASTEXITCODE
    $diagnostics | Tee-Object -FilePath $outputPath -Append
    if ($exitCode -ne 0) {
        $failed = $true
    }
    foreach ($diagnostic in $diagnostics) {
        $text = [string]$diagnostic
        $checks = @(Get-ClangTidyDiagnosticChecks -Line $text)
        if ((Test-ClangTidyBlockingCheck `
                -Checks $checks -Patterns $blockingPatterns) -and
            $seenBlockingFindings.Add($text)) {
            $blockingFindings.Add($text)
        }
    }
}

if ($failed) {
    throw "clang-tidy could not complete. See $outputPath."
}
if ($blockingFindings.Count -ne 0) {
    "" | Add-Content -LiteralPath $outputPath
    "Blocking clang-tidy findings: $($blockingFindings.Count)" |
        Add-Content -LiteralPath $outputPath
    $blockingFindings | Add-Content -LiteralPath $outputPath
    throw "clang-tidy reported $($blockingFindings.Count) blocking correctness/safety finding(s). See $outputPath."
}
