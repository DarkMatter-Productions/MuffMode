param(
    [string]$CompileCommands = "build\compile_commands.json",
    [string]$Output = "build\analysis\cppcheck.xml"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$compileCommandsPath = Join-Path $repoRoot $CompileCommands
if (-not (Test-Path -LiteralPath $compileCommandsPath)) {
    & "$PSScriptRoot\export-compile-commands.ps1" -Output $CompileCommands
}

$cppcheck = Get-Command cppcheck -ErrorAction SilentlyContinue
if (-not $cppcheck) {
    throw "cppcheck was not found on PATH."
}

$outputPath = Join-Path $repoRoot $Output
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null

$arguments = @(
    "--project=$compileCommandsPath",
    "--enable=warning,performance,portability",
    "--inline-suppr",
    "--xml",
    "--xml-version=2",
    "--error-exitcode=1"
)

$suppressions = Join-Path $repoRoot ".cppcheck-suppressions"
if (Test-Path -LiteralPath $suppressions) {
    $activeSuppressions = @(Get-Content -Path $suppressions | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_) -and $_.TrimStart()[0] -ne "#"
    })
    if ($activeSuppressions.Count -gt 0) {
        $arguments += "--suppressions-list=$suppressions"
    }
}

& $cppcheck.Source @arguments 2>&1 | Tee-Object -FilePath $outputPath
if ($LASTEXITCODE -ne 0) {
    throw "cppcheck reported errors. See $outputPath."
}
