param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$Triplet = "x64-windows-static",
    [switch]$Install
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $VcpkgRoot = Join-Path $repoRoot "vcpkg"
}

$VcpkgRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($VcpkgRoot)
$env:VCPKG_ROOT = $VcpkgRoot

if (-not (Test-Path -LiteralPath $VcpkgRoot)) {
    Invoke-NativeCommand -FilePath "git" -Arguments @("clone", "https://github.com/microsoft/vcpkg.git", $VcpkgRoot) -WorkingDirectory $repoRoot
}

$vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
if (-not (Test-Path -LiteralPath $vcpkgExe)) {
    $bootstrap = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
    Invoke-NativeCommand -FilePath $bootstrap -Arguments @("-disableMetrics") -WorkingDirectory $VcpkgRoot
}

if ($Install) {
    Invoke-NativeCommand -FilePath $vcpkgExe -Arguments @("install", "--triplet", $Triplet) -WorkingDirectory (Join-Path $repoRoot "src")
}

Write-Host "VCPKG_ROOT=$VcpkgRoot"
