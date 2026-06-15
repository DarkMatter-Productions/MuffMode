param(
    [switch]$AllowUnsupported
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$clang = Get-Command clang-cl -ErrorAction SilentlyContinue
if (-not $clang) {
    $message = "clang-cl was not found; libFuzzer targets cannot be built on this machine."
    if ($AllowUnsupported) {
        Write-Warning $message
        exit 0
    }
    throw $message
}

$outDir = Join-Path $repoRoot "build\fuzz"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$target = Join-Path $repoRoot "tests\fuzz\fuzz_numeric_parsers.cpp"
$exe = Join-Path $outDir "fuzz_numeric_parsers.exe"

$arguments = @(
    "/nologo",
    "/std:c++17",
    "/EHsc",
    "/I$repoRoot\src",
    "-fsanitize=fuzzer,address",
    $target,
    "/Fe:$exe"
)

try {
    Invoke-NativeCommand -FilePath $clang.Source -Arguments $arguments -WorkingDirectory $repoRoot
}
catch {
    if ($AllowUnsupported) {
        Write-Warning "libFuzzer target build is not supported by this toolchain yet: $($_.Exception.Message)"
        exit 0
    }
    throw
}

Write-Host "Built libFuzzer target: $exe"
