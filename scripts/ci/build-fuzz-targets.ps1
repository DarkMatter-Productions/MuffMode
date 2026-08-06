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
$runtimeDestination = Join-Path $outDir "clang_rt.asan_dynamic-x86_64.dll"

# Never leave a previously successful target available after a failed build.
foreach ($staleOutput in @($exe, $runtimeDestination)) {
    if (Test-Path -LiteralPath $staleOutput) {
        Remove-Item -LiteralPath $staleOutput -Force
    }
}

$resourceDir = (& $clang.Source --print-resource-dir)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace(($resourceDir -join ""))) {
    $message = "clang-cl could not report its resource directory; libFuzzer support cannot be verified."
    if ($AllowUnsupported) {
        Write-Warning $message
        exit 0
    }
    throw $message
}

$runtimeDir = Join-Path ([string]($resourceDir | Select-Object -Last 1)).Trim() "lib\windows"
$requiredRuntimeFiles = @(
    "clang_rt.fuzzer-x86_64.lib",
    "clang_rt.asan_dynamic-x86_64.lib",
    "clang_rt.asan_dynamic-x86_64.dll"
)
$missingRuntimeFiles = @($requiredRuntimeFiles | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $runtimeDir $_) -PathType Leaf)
})
if ($missingRuntimeFiles.Count -ne 0) {
    $message = "clang-cl is missing required x64 libFuzzer/ASan runtimes: $($missingRuntimeFiles -join ', ')."
    if ($AllowUnsupported) {
        Write-Warning $message
        exit 0
    }
    throw $message
}

$arguments = @(
    "/nologo",
    "/std:c++17",
    "/EHsc",
    "/I$repoRoot\src\sgame",
    "-fsanitize=fuzzer,address",
    $target,
    "/Fe:$exe"
)

Invoke-NativeCommand -FilePath $clang.Source -Arguments $arguments -WorkingDirectory $repoRoot

if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "clang-cl reported success but did not produce the expected target: $exe"
}

Copy-Item -LiteralPath (Join-Path $runtimeDir "clang_rt.asan_dynamic-x86_64.dll") `
    -Destination $runtimeDestination -Force

Write-Host "Built libFuzzer target: $exe"
Write-Host "Staged ASan runtime: $runtimeDestination"
