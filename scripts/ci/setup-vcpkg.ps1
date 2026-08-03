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

    if (-not (Test-Path -LiteralPath $Root)) {
        Invoke-NativeCommand -FilePath "git" -Arguments @(
            "clone", "--filter=blob:none", "--no-checkout", $vcpkgRepository, $Root
        ) -WorkingDirectory $repoRoot
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

    $dirty = @(& git -C $Root status --porcelain --untracked-files=no)
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inspect the vcpkg checkout at $Root."
    }
    if ($dirty.Count -ne 0) {
        throw "Refusing to replace a modified vcpkg checkout at $Root."
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
    Invoke-NativeCommand -FilePath $vcpkgExe -Arguments @("install", "--triplet", $Triplet) -WorkingDirectory $repoRoot
}

Write-Host "VCPKG_ROOT=$VcpkgRoot"
Write-Host "VCPKG_REVISION=$vcpkgRevision"
