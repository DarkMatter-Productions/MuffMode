param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\build-msbuild.ps1" `
    -Configuration $Configuration `
    -Platform $Platform `
    -Target Rebuild `
    -RunCodeAnalysis `
    -BinaryLog `
    -BinaryLogPath "build\analysis\msvc-analyze-$Configuration-$Platform.binlog"
