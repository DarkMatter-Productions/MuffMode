param(
    [string]$CompileCommands = "build\compile_commands.json",
    [string]$Output = "build\analysis\clang-tidy.txt",
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

if ($Files.Count -eq 0 -and -not [string]::IsNullOrWhiteSpace($ChangedSince)) {
    Push-Location $repoRoot
    try {
        $Files = @(git diff --name-only --diff-filter=ACMRT $ChangedSince -- "src/*.cpp" "src/**/*.cpp")
    }
    finally {
        Pop-Location
    }
}

if ($Files.Count -eq 0) {
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

$databaseDir = Split-Path -Parent $compileCommandsPath
$configPath = Join-Path $repoRoot ".clang-tidy"
$failed = $false
"clang-tidy files: $($Files.Count)" | Set-Content -Path $outputPath -Encoding utf8

foreach ($file in $Files) {
    "== $file ==" | Tee-Object -FilePath $outputPath -Append | Out-Null
    & $clangTidy.Source "-p=$databaseDir" "--config-file=$configPath" $file 2>&1 |
        Tee-Object -FilePath $outputPath -Append
    if ($LASTEXITCODE -ne 0) {
        $failed = $true
    }
}

if ($failed) {
    throw "clang-tidy reported errors. See $outputPath."
}
