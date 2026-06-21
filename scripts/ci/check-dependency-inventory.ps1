[CmdletBinding()]
param(
    [string]$InventoryPath = "docs-dev/robustness/dependency-inventory.json"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
Set-Location -LiteralPath $repoRoot

function Fail-Inventory {
    param([string]$Message)
    throw "Dependency inventory check failed: $Message"
}

function Get-InventoryPackage {
    param(
        [object]$Inventory,
        [string]$Name
    )

    $package = @($Inventory.packages | Where-Object { $_.name -eq $Name })
    if ($package.Count -ne 1) {
        Fail-Inventory "expected exactly one inventory package named '$Name', found $($package.Count)."
    }
    return $package[0]
}

function Convert-FmtVersionToInteger {
    param([string]$Version)

    $parts = @($Version.Split(".") | ForEach-Object { [int]$_ })
    if ($parts.Count -ne 3) {
        Fail-Inventory "fmt version '$Version' is not major.minor.patch."
    }

    return ($parts[0] * 10000) + ($parts[1] * 100) + $parts[2]
}

$inventoryFullPath = Join-Path $repoRoot $InventoryPath
if (-not (Test-Path -LiteralPath $inventoryFullPath -PathType Leaf)) {
    Fail-Inventory "inventory file is missing: $InventoryPath"
}

$inventory = Get-Content -LiteralPath $inventoryFullPath -Raw | ConvertFrom-Json

if ($inventory.project.license_spdx -ne "GPL-2.0-only") {
    Fail-Inventory "project license_spdx must be GPL-2.0-only."
}

$licenseText = Get-Content -LiteralPath (Join-Path $repoRoot "LICENSE") -Raw
if ($licenseText -notmatch "Version 2, June 1991") {
    Fail-Inventory "root LICENSE is not GPL version 2 text."
}

foreach ($requiredDoc in @(
    "THIRD_PARTY_NOTICES.md",
    "docs/licensing.md",
    "docs/dependencies.md",
    "docs/hardening-guide.md"
)) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $requiredDoc) -PathType Leaf)) {
        Fail-Inventory "required documentation file is missing: $requiredDoc"
    }
}

$vcpkgPath = Join-Path $repoRoot "src/vcpkg.json"
$vcpkg = Get-Content -LiteralPath $vcpkgPath -Raw | ConvertFrom-Json
if ($vcpkg.'builtin-baseline' -ne $inventory.dependency_policy.vcpkg_baseline) {
    Fail-Inventory "src/vcpkg.json baseline does not match dependency inventory."
}

$declaredDependencies = @($vcpkg.dependencies | ForEach-Object {
    if ($_ -is [string]) { $_ } else { $_.name }
})

foreach ($dependency in @("fmt", "jsoncpp")) {
    if ($declaredDependencies -notcontains $dependency) {
        Fail-Inventory "src/vcpkg.json is missing dependency '$dependency'."
    }
}

$fmtPackage = Get-InventoryPackage -Inventory $inventory -Name "fmt"
$fmtCore = Get-Content -LiteralPath (Join-Path $repoRoot "src/fmt/core.h") -Raw
if ($fmtCore -notmatch "#define\s+FMT_VERSION\s+([0-9]+)") {
    Fail-Inventory "could not find FMT_VERSION in src/fmt/core.h."
}
$fmtActual = [int]$Matches[1]
$fmtExpected = Convert-FmtVersionToInteger $fmtPackage.version
if ($fmtActual -ne $fmtExpected) {
    Fail-Inventory "vendored fmt version integer $fmtActual does not match inventory version $($fmtPackage.version) ($fmtExpected)."
}

$jsonPackage = Get-InventoryPackage -Inventory $inventory -Name "jsoncpp"
$jsonVersion = Get-Content -LiteralPath (Join-Path $repoRoot "src/json/version.h") -Raw
if ($jsonVersion -notmatch '#define\s+JSONCPP_VERSION_STRING\s+"([^"]+)"') {
    Fail-Inventory "could not find JSONCPP_VERSION_STRING in src/json/version.h."
}
if ($Matches[1] -ne $jsonPackage.version) {
    Fail-Inventory "vendored JsonCpp version '$($Matches[1])' does not match inventory version '$($jsonPackage.version)'."
}

$releaseScript = Get-Content -LiteralPath (Join-Path $repoRoot "scripts/release.ps1") -Raw
if ($releaseScript -notmatch "THIRD_PARTY_NOTICES\.md") {
    Fail-Inventory "scripts/release.ps1 does not package THIRD_PARTY_NOTICES.md."
}

$noticeText = Get-Content -LiteralPath (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md") -Raw
foreach ($needle in @("{fmt}", "JsonCpp", "GPL-2.0-only")) {
    if ($noticeText -notlike "*$needle*") {
        Fail-Inventory "THIRD_PARTY_NOTICES.md is missing '$needle'."
    }
}

$fmtSpdx = Join-Path $repoRoot "src/vcpkg_installed/x64-windows-static/x64-windows-static/share/fmt/vcpkg.spdx.json"
if (Test-Path -LiteralPath $fmtSpdx) {
    $fmtSpdxText = Get-Content -LiteralPath $fmtSpdx -Raw
    if ($fmtSpdxText -notmatch '"versionInfo"\s*:\s*"10\.1\.1"') {
        Fail-Inventory "installed vcpkg fmt SPDX metadata does not report version 10.1.1."
    }
}

$jsonSpdx = Join-Path $repoRoot "src/vcpkg_installed/x64-windows-static/x64-windows-static/share/jsoncpp/vcpkg.spdx.json"
if (Test-Path -LiteralPath $jsonSpdx) {
    $jsonSpdxText = Get-Content -LiteralPath $jsonSpdx -Raw
    if ($jsonSpdxText -notmatch '"versionInfo"\s*:\s*"1\.9\.5#2"') {
        Fail-Inventory "installed vcpkg JsonCpp SPDX metadata does not report version 1.9.5#2."
    }
}

Write-Host "Dependency inventory, license metadata, and third-party notices are consistent."
