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

$manifest = Get-Content -LiteralPath (Join-Path $repoRoot "vcpkg.json") -Raw | ConvertFrom-Json
$vcpkgRevision = [string]$manifest.'builtin-baseline'
if ($vcpkgRevision -notmatch '^[0-9a-fA-F]{40}$') {
    throw "vcpkg.json must provide a 40-character builtin-baseline for the tool checkout."
}

$VcpkgRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($VcpkgRoot)
$env:VCPKG_ROOT = $VcpkgRoot

function Ensure-VcpkgCheckout {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,

        [Parameter(Mandatory = $true)]
        [string]$Revision
    )

    $vcpkgRepository = "https://github.com/microsoft/vcpkg.git"
    $createdCheckout = $false

    if (-not (Test-Path -LiteralPath $Root)) {
        Invoke-NativeCommand -FilePath "git" -Arguments @(
            "clone", "--filter=blob:none", "--no-checkout", $vcpkgRepository, $Root
        ) -WorkingDirectory $repoRoot
        $createdCheckout = $true
    }
    elseif (-not (Test-Path -LiteralPath (Join-Path $Root ".git"))) {
        # [MuffMode] actions/cache restores these mutable cache directories
        # before the pinned tool checkout exists. Initialize only that exact,
        # cache-only shape; retain the hard failure for arbitrary directories.
        $allowedCacheDirectoryNames = @("downloads", "buildtrees", "packages")
        $unexpectedEntries = @(Get-ChildItem -LiteralPath $Root -Force | Where-Object {
            -not $_.PSIsContainer -or
            $_.Name -notin $allowedCacheDirectoryNames -or
            ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint)
        })
        if ($unexpectedEntries.Count -ne 0) {
            throw "VCPKG_ROOT exists but is neither a Git checkout nor a cache-only directory: $Root"
        }

        Invoke-NativeCommand -FilePath "git" -Arguments @("init", $Root) -WorkingDirectory $repoRoot
    }

    $remote = & git -C $Root remote get-url origin 2>$null
    if ($LASTEXITCODE -ne 0) {
        Invoke-NativeCommand -FilePath "git" -Arguments @("remote", "add", "origin", $vcpkgRepository) -WorkingDirectory $Root
    }
    elseif ($remote -ne $vcpkgRepository) {
        Invoke-NativeCommand -FilePath "git" -Arguments @("remote", "set-url", "origin", $vcpkgRepository) -WorkingDirectory $Root
    }

    # This guard protects a checkout somebody else owns. A --no-checkout clone we
    # just made reports its whole tree as deleted, so exempt the one we created or
    # no machine could ever start from nothing.
    if (-not $createdCheckout) {
        $dirty = @(& git -C $Root status --porcelain --untracked-files=no)
        if ($LASTEXITCODE -ne 0) {
            throw "Could not inspect the vcpkg checkout at $Root."
        }
        if ($dirty.Count -ne 0) {
            throw "Refusing to replace a modified vcpkg checkout at $Root."
        }
    }

    $currentRevision = (& git -C $Root rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -eq 0 -and $currentRevision.Trim() -eq $Revision) {
        if (-not (Test-Path -LiteralPath (Join-Path $Root "bootstrap-vcpkg.bat") -PathType Leaf)) {
            throw "Pinned vcpkg checkout is missing bootstrap-vcpkg.bat: $Root"
        }
        return
    }

    Invoke-NativeCommand -FilePath "git" -Arguments @("fetch", "--depth", "1", "origin", $Revision) -WorkingDirectory $Root
    Invoke-NativeCommand -FilePath "git" -Arguments @("checkout", "--detach", "--force", "FETCH_HEAD") -WorkingDirectory $Root

    $resolvedRevision = (& git -C $Root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $resolvedRevision -ne $Revision) {
        throw "vcpkg checkout did not resolve to pinned revision $Revision."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Root "bootstrap-vcpkg.bat") -PathType Leaf)) {
        throw "Pinned vcpkg checkout is missing bootstrap-vcpkg.bat: $Root"
    }
}

function Test-PkgConfigProgram {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }

    try {
        & $Path --version | Out-Null
    }
    catch {
        return $false
    }

    return ($LASTEXITCODE -eq 0)
}

function Use-PkgConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $env:PKG_CONFIG = $Path

    # Ports build in an environment vcpkg sanitizes, so PKG_CONFIG only reaches the
    # portfile when it is named here.
    $kept = @()
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_KEEP_ENV_VARS)) {
        $kept = @($env:VCPKG_KEEP_ENV_VARS -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }
    if ($kept -notcontains "PKG_CONFIG") {
        $kept += "PKG_CONFIG"
    }
    $env:VCPKG_KEEP_ENV_VARS = ($kept -join ';')

    Write-Host "PKG_CONFIG=$Path"
}

function Ensure-PkgConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,

        [Parameter(Mandatory = $true)]
        [string]$VcpkgExe,

        [Parameter(Mandatory = $true)]
        [string]$Triplet
    )

    # [MuffMode] Both manifest ports end in vcpkg_fixup_pkgconfig(), which resolves
    # pkg-config from an MSYS2 build pinned by the baseline. MSYS2 only serves current
    # package versions, so that download now fails on every mirror and a cold cache
    # dies before a single translation unit is compiled. vcpkg skips the download when
    # PKG_CONFIG names a program, and the baseline's own pkgconf port fixes its files
    # up with SKIP_CHECK, so it builds without needing one to already exist.
    if (-not [string]::IsNullOrWhiteSpace($env:PKG_CONFIG)) {
        if (-not (Test-PkgConfigProgram -Path $env:PKG_CONFIG)) {
            throw "PKG_CONFIG does not name a usable pkg-config program: $($env:PKG_CONFIG)"
        }
        Use-PkgConfig -Path $env:PKG_CONFIG
        return
    }

    # actions/cache carries packages\ but not installed\, so accept either copy before
    # paying for the build again.
    $candidates = @(
        (Join-Path $Root "installed\$Triplet\tools\pkgconf\pkgconf.exe"),
        (Join-Path $Root "packages\pkgconf_$Triplet\tools\pkgconf\pkgconf.exe")
    )

    $pkgConfig = $candidates | Where-Object { Test-PkgConfigProgram -Path $_ } | Select-Object -First 1
    if (-not $pkgConfig) {
        # The root manifest is inventory-reviewed and must not gain a build-only
        # dependency, so install this the classic way, into the tool checkout. vcpkg
        # selects manifest mode from the first vcpkg.json at or above its working
        # directory and manifest mode refuses named packages, so run it from a scratch
        # directory outside the repository.
        $classicRoot = Join-Path ([System.IO.Path]::GetTempPath()) "muffmode-vcpkg-classic"
        New-Item -ItemType Directory -Path $classicRoot -Force | Out-Null
        Invoke-NativeCommand -FilePath $VcpkgExe -Arguments @(
            "install", "pkgconf:$Triplet", "--vcpkg-root", $Root
        ) -WorkingDirectory $classicRoot
        $pkgConfig = $candidates | Where-Object { Test-PkgConfigProgram -Path $_ } | Select-Object -First 1
    }
    if (-not $pkgConfig) {
        throw "The pinned vcpkg baseline did not produce a pkg-config program for $Triplet."
    }

    Use-PkgConfig -Path $pkgConfig
}

Ensure-VcpkgCheckout -Root $VcpkgRoot -Revision $vcpkgRevision

$vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
$bootstrap = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
# Rebuild the executable from the pinned checkout every time. vcpkg.exe is
# untracked and may otherwise survive a revision change or local replacement.
Invoke-NativeCommand -FilePath $bootstrap -Arguments @("-disableMetrics") -WorkingDirectory $VcpkgRoot
if (-not (Test-Path -LiteralPath $vcpkgExe -PathType Leaf)) {
    throw "Pinned vcpkg bootstrap did not produce vcpkg.exe: $vcpkgExe"
}

if ($Install) {
    Ensure-PkgConfig -Root $VcpkgRoot -VcpkgExe $vcpkgExe -Triplet $Triplet
    Invoke-NativeCommand -FilePath $vcpkgExe -Arguments @("install", "--triplet", $Triplet) -WorkingDirectory $repoRoot
}

Write-Host "VCPKG_ROOT=$VcpkgRoot"
Write-Host "VCPKG_REVISION=$vcpkgRevision"
