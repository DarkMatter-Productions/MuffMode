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

function Ensure-VcpkgCheckout {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $bootstrap = Join-Path $Root "bootstrap-vcpkg.bat"
    if (Test-Path -LiteralPath $bootstrap) {
        return
    }

    $vcpkgRepository = "https://github.com/microsoft/vcpkg.git"

    if (-not (Test-Path -LiteralPath $Root)) {
        Invoke-NativeCommand -FilePath "git" -Arguments @("clone", "--depth", "1", $vcpkgRepository, $Root) -WorkingDirectory $repoRoot
        return
    }

    if (-not (Test-Path -LiteralPath (Join-Path $Root ".git"))) {
        Invoke-NativeCommand -FilePath "git" -Arguments @("init") -WorkingDirectory $Root
    }

    $remote = & git -C $Root remote get-url origin 2>$null
    if ($LASTEXITCODE -ne 0) {
        Invoke-NativeCommand -FilePath "git" -Arguments @("remote", "add", "origin", $vcpkgRepository) -WorkingDirectory $Root
    }
    elseif ($remote -ne $vcpkgRepository) {
        Invoke-NativeCommand -FilePath "git" -Arguments @("remote", "set-url", "origin", $vcpkgRepository) -WorkingDirectory $Root
    }

    Invoke-NativeCommand -FilePath "git" -Arguments @("fetch", "--depth", "1", "origin", "master") -WorkingDirectory $Root
    Invoke-NativeCommand -FilePath "git" -Arguments @("checkout", "--force", "FETCH_HEAD") -WorkingDirectory $Root
}

Ensure-VcpkgCheckout -Root $VcpkgRoot

$vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
if (-not (Test-Path -LiteralPath $vcpkgExe)) {
    $bootstrap = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
    Invoke-NativeCommand -FilePath $bootstrap -Arguments @("-disableMetrics") -WorkingDirectory $VcpkgRoot
}

if ($Install) {
    Invoke-NativeCommand -FilePath $vcpkgExe -Arguments @("install", "--triplet", $Triplet) -WorkingDirectory (Join-Path $repoRoot "src")
}

Write-Host "VCPKG_ROOT=$VcpkgRoot"
