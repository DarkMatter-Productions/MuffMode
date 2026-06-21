param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot

& "$PSScriptRoot\build-msbuild.ps1" `
    -Solution "tests\host\MuffMode.HostTests.vcxproj" `
    -Configuration $Configuration `
    -Platform $Platform `
    -Target Build

$exe = Join-Path $repoRoot "tests\host\bin\$Platform\$Configuration\MuffMode.HostTests.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Host test executable was not produced: $exe"
}

Invoke-NativeCommand -FilePath $exe -Arguments @() -WorkingDirectory $repoRoot
