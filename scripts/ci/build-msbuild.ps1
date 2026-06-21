param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [ValidateSet("Build", "Rebuild", "Clean")]
    [string]$Target = "Build",

    [string]$Solution = "src\MuffMode.sln",
    [string]$PlatformToolset,
    [switch]$TreatWarningsAsErrors,
    [switch]$RunCodeAnalysis,
    [switch]$EnableASAN,
    [switch]$EnableUBSAN,
    [switch]$BinaryLog,
    [string]$BinaryLogPath,
    [string[]]$AdditionalMsBuildArgs = @()
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$solutionPath = Join-Path $repoRoot $Solution
if (-not (Test-Path -LiteralPath $solutionPath)) {
    throw "Solution not found: $solutionPath"
}

$arguments = @(
    $solutionPath,
    "/t:$Target",
    "/m",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform"
)

if ($TreatWarningsAsErrors) {
    $arguments += "/p:MMTreatWarningsAsErrors=true"
}

if ($RunCodeAnalysis) {
    $arguments += "/p:MMRunCodeAnalysis=true"
    $arguments += "/p:RunCodeAnalysis=true"
    $arguments += "/p:EnableMicrosoftCodeAnalysis=true"
}

if ($EnableASAN) {
    $arguments += "/p:MMEnableAddressSanitizer=true"
    $arguments += "/p:EnableASAN=true"
}

if ($EnableUBSAN) {
    $arguments += "/p:MMEnableUndefinedSanitizer=true"
    if ([string]::IsNullOrWhiteSpace($PlatformToolset)) {
        $PlatformToolset = "ClangCL"
    }
}

if (-not [string]::IsNullOrWhiteSpace($PlatformToolset)) {
    $arguments += "/p:PlatformToolset=$PlatformToolset"
}

if ($BinaryLog) {
    if ([string]::IsNullOrWhiteSpace($BinaryLogPath)) {
        $BinaryLogPath = "build\logs\msbuild-$Configuration-$Platform.binlog"
    }
    $fullBinaryLogPath = Join-Path $repoRoot $BinaryLogPath
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fullBinaryLogPath) | Out-Null
    $arguments += "/bl:$fullBinaryLogPath"
}

$arguments += $AdditionalMsBuildArgs

Invoke-NativeCommand -FilePath (Resolve-MSBuild) -Arguments $arguments -WorkingDirectory $repoRoot
