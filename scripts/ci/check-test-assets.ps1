[CmdletBinding()]
param(
    [switch] $RepoOnly,
    [string] $AssetRoot = $env:MUFFMODE_TEST_ASSET_ROOT,
    [string] $GameInstallRoot
)

$ErrorActionPreference = "Stop"

$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location -LiteralPath $repoRoot

function Require-File {
    param(
        [string] $Path,
        [string] $Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing ${Description}: $Path"
    }

    $item = Get-Item -LiteralPath $Path
    if ($item.Length -le 0) {
        throw "Empty ${Description}: $Path"
    }
}

function Require-Directory {
    param(
        [string] $Path,
        [string] $Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Missing ${Description}: $Path"
    }
}

function Require-AnyFile {
    param(
        [string] $Directory,
        [string] $Filter,
        [string] $Description
    )

    $match = Get-ChildItem -LiteralPath $Directory -Filter $Filter -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $match) {
        throw "Missing $Description matching '$Filter' in $Directory"
    }
}

$repoSeeds = @(
    @{ Path = "docs-dev/test-assets/configs/server-headless-smoke.cfg"; Description = "headless smoke config seed" },
    @{ Path = "docs-dev/test-assets/command-sequences/phase-zero-smoke.commands"; Description = "command-sequence corpus seed" },
    @{ Path = "docs-dev/test-assets/command-sequences/phase-three-regression.commands"; Description = "phase-three command regression corpus seed" },
    @{ Path = "docs-dev/test-assets/rotation/basic-rotation.txt"; Description = "rotation/config corpus seed" },
    @{ Path = "docs-dev/test-assets/save-load/synthetic-minimal-save.json"; Description = "save/load corpus seed" },
    @{ Path = "docs-dev/test-assets/red-rover/0.36.04-regressions.json"; Description = "Red Rover regression corpus seed" },
    @{ Path = "docs-dev/test-assets/fuzz-corpus/numeric-parsers/valid-and-invalid.txt"; Description = "numeric parser fuzz corpus seed" },
    @{ Path = "docs-dev/test-assets/fuzz-corpus/gt-cfg/maxclients-lines.txt"; Description = "gametype cfg fuzz corpus seed" },
    @{ Path = "docs-dev/test-assets/manifest.example.json"; Description = "asset manifest example" }
)

foreach ($seed in $repoSeeds) {
    Require-File -Path (Join-Path $repoRoot $seed.Path) -Description $seed.Description
}

Get-Content -Raw (Join-Path $repoRoot "docs-dev/test-assets/manifest.example.json") | ConvertFrom-Json | Out-Null
Get-Content -Raw (Join-Path $repoRoot "docs-dev/test-assets/save-load/synthetic-minimal-save.json") | ConvertFrom-Json | Out-Null
Get-Content -Raw (Join-Path $repoRoot "docs-dev/test-assets/red-rover/0.36.04-regressions.json") | ConvertFrom-Json | Out-Null

if ($RepoOnly) {
    Write-Host "Repo-side phase-zero test asset seeds are present and parseable."
    exit 0
}

if ([string]::IsNullOrWhiteSpace($AssetRoot)) {
    throw "Set MUFFMODE_TEST_ASSET_ROOT or pass -AssetRoot for full asset-pack validation."
}

Require-Directory -Path $AssetRoot -Description "test asset root"
Require-File -Path (Join-Path $AssetRoot "manifest.json") -Description "external asset manifest"
Get-Content -Raw (Join-Path $AssetRoot "manifest.json") | ConvertFrom-Json | Out-Null

if ([string]::IsNullOrWhiteSpace($GameInstallRoot)) {
    $GameInstallRoot = Join-Path $AssetRoot "clean-install"
}

Require-Directory -Path $GameInstallRoot -Description "clean Quake II installation"

$rereleaseRoot = Join-Path $GameInstallRoot "rerelease"
if (-not (Test-Path -LiteralPath $rereleaseRoot -PathType Container)) {
    $rereleaseRoot = $GameInstallRoot
}

$baseq2Root = Join-Path $rereleaseRoot "baseq2"
Require-Directory -Path $baseq2Root -Description "Quake II Rerelease baseq2 directory"
Require-File -Path (Join-Path $baseq2Root "pak0.pak") -Description "Quake II Rerelease pak0.pak"
Require-AnyFile -Directory $rereleaseRoot -Filter "quake2ex*.exe" -Description "Quake II Rerelease executable"

$externalSeeds = @(
    @{ Path = "smoke/server-headless-smoke.cfg"; Description = "external headless smoke config" },
    @{ Path = "corpus/command-sequences/phase-zero-smoke.commands"; Description = "external command-sequence corpus" },
    @{ Path = "corpus/command-sequences/phase-three-regression.commands"; Description = "external phase-three command regression corpus" },
    @{ Path = "corpus/rotation/basic-rotation.txt"; Description = "external rotation/config corpus" },
    @{ Path = "corpus/save-load/synthetic-minimal-save.json"; Description = "external save/load corpus" },
    @{ Path = "corpus/red-rover/0.36.04-regressions.json"; Description = "external Red Rover regression corpus" }
)

foreach ($seed in $externalSeeds) {
    Require-File -Path (Join-Path $AssetRoot $seed.Path) -Description $seed.Description
}

Write-Host "Full phase-zero test asset pack is present."
