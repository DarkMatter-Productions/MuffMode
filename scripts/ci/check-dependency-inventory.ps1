[CmdletBinding()]
param(
    [string]$InventoryPath = "docs-dev/robustness/dependency-inventory.json",
    [string]$ManifestPath = "vcpkg.json"
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

$vcpkgPath = if ([System.IO.Path]::IsPathRooted($ManifestPath)) {
    [System.IO.Path]::GetFullPath($ManifestPath)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $ManifestPath))
}

function ConvertTo-NormalizedVcpkgDependency {
    param([object]$Dependency)

    if ($Dependency -is [string]) {
        $name = ([string]$Dependency).Trim().ToLowerInvariant()
        if ([string]::IsNullOrWhiteSpace($name)) {
            Fail-Inventory "vcpkg dependency name cannot be empty."
        }
        return $name
    }

    $knownProperties = @("name", "features", "default-features", "host", "platform")
    $unknownProperties = @($Dependency.PSObject.Properties.Name | Where-Object {
        $knownProperties -notcontains $_
    })
    if ($unknownProperties.Count -ne 0) {
        Fail-Inventory "vcpkg dependency '$($Dependency.name)' uses unreviewed properties: $($unknownProperties -join ', ')."
    }

    $name = ([string]$Dependency.name).Trim().ToLowerInvariant()
    if ([string]::IsNullOrWhiteSpace($name)) {
        Fail-Inventory "vcpkg dependency object contains no name."
    }

    $features = @($Dependency.features | ForEach-Object {
        ([string]$_).Trim().ToLowerInvariant()
    } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
    $defaultFeaturesProperty = $Dependency.PSObject.Properties["default-features"]
    $defaultFeatures = if ($defaultFeaturesProperty) { [bool]$defaultFeaturesProperty.Value } else { $true }
    $hostProperty = $Dependency.PSObject.Properties["host"]
    $isHostDependency = if ($hostProperty) { [bool]$hostProperty.Value } else { $false }
    $platform = if ($Dependency.PSObject.Properties["platform"]) {
        ([string]$Dependency.platform).Trim()
    }
    else {
        ""
    }

    if ($features.Count -eq 0 -and $defaultFeatures -and -not $isHostDependency -and
        [string]::IsNullOrWhiteSpace($platform)) {
        return $name
    }

    return "$name|features=$($features -join ',')|default-features=$($defaultFeatures.ToString().ToLowerInvariant())|host=$($isHostDependency.ToString().ToLowerInvariant())|platform=$platform"
}
if (-not (Test-Path -LiteralPath $vcpkgPath -PathType Leaf)) {
    Fail-Inventory "manifest file is missing: $ManifestPath"
}
$vcpkg = Get-Content -LiteralPath $vcpkgPath -Raw | ConvertFrom-Json
$reviewedManifestProperties = @(
    '$schema',
    'name',
    'version',
    'dependencies',
    'builtin-baseline'
)
$unreviewedManifestProperties = @($vcpkg.PSObject.Properties.Name | Where-Object {
    $reviewedManifestProperties -notcontains $_
})
if ($unreviewedManifestProperties.Count -ne 0) {
    Fail-Inventory "vcpkg.json uses unreviewed top-level properties: $($unreviewedManifestProperties -join ', ')."
}
$trackedVcpkgConfigurations = @(& git -C $repoRoot ls-files -- `
    "vcpkg-configuration.json" "vcpkg-configuration.json5")
if ($LASTEXITCODE -ne 0) {
    Fail-Inventory "could not inspect tracked vcpkg registry configuration."
}
if ($trackedVcpkgConfigurations.Count -ne 0) {
    Fail-Inventory "tracked vcpkg registry configuration is not represented in the dependency inventory: $($trackedVcpkgConfigurations -join ', ')."
}
if ($vcpkg.'builtin-baseline' -ne $inventory.dependency_policy.vcpkg_baseline) {
    Fail-Inventory "vcpkg.json baseline does not match dependency inventory."
}

$declaredDependencies = @($vcpkg.dependencies | ForEach-Object {
    ConvertTo-NormalizedVcpkgDependency $_
})
$duplicateManifestDependencies = @($declaredDependencies | ForEach-Object { $_.Split('|')[0] } | Group-Object | Where-Object Count -gt 1)
if ($duplicateManifestDependencies.Count -ne 0) {
    Fail-Inventory "vcpkg.json contains duplicate dependencies: $($duplicateManifestDependencies.Name -join ', ')."
}

$inventoryDependencies = @($inventory.packages | ForEach-Object {
    $property = $_.PSObject.Properties["vcpkg_dependency"]
    if ($property -and $null -ne $property.Value) {
        ConvertTo-NormalizedVcpkgDependency $property.Value
    }
})
$duplicateInventoryDependencies = @($inventoryDependencies | ForEach-Object { $_.Split('|')[0] } | Group-Object | Where-Object Count -gt 1)
if ($duplicateInventoryDependencies.Count -ne 0) {
    Fail-Inventory "dependency inventory contains duplicate vcpkg dependencies: $($duplicateInventoryDependencies.Name -join ', ')."
}

$uninventoriedDependencies = @($declaredDependencies | Where-Object { $inventoryDependencies -notcontains $_ })
$undeclaredDependencies = @($inventoryDependencies | Where-Object { $declaredDependencies -notcontains $_ })
if ($uninventoriedDependencies.Count -ne 0 -or $undeclaredDependencies.Count -ne 0) {
    $details = @()
    if ($uninventoriedDependencies.Count -ne 0) {
        $details += "manifest-only: $($uninventoriedDependencies -join ', ')"
    }
    if ($undeclaredDependencies.Count -ne 0) {
        $details += "inventory-only: $($undeclaredDependencies -join ', ')"
    }
    Fail-Inventory "vcpkg manifest and inventory dependencies differ ($($details -join '; '))."
}

$declaredNuGetDependencies = @()
$trackedProjectPaths = @(& git -C $repoRoot ls-files -- `
    "*.csproj" "*.fsproj" "*.vbproj" "*.props" "*.targets")
if ($LASTEXITCODE -ne 0) {
    Fail-Inventory "could not enumerate tracked MSBuild dependency files."
}
$projectFiles = @($trackedProjectPaths | ForEach-Object {
    Get-Item -LiteralPath (Join-Path $repoRoot $_)
})
foreach ($projectFile in $projectFiles) {
    [xml]$projectXml = Get-Content -LiteralPath $projectFile.FullName -Raw
    foreach ($reference in @($projectXml.SelectNodes("//*[local-name()='PackageReference']"))) {
        $name = ([string]$reference.Include).Trim().ToLowerInvariant()
        if ([string]::IsNullOrWhiteSpace($name)) {
            $name = ([string]$reference.Update).Trim().ToLowerInvariant()
        }
        if ([string]::IsNullOrWhiteSpace($name)) {
            Fail-Inventory "PackageReference in '$($projectFile.FullName)' has no Include or Update name."
        }
        $version = ([string]$reference.Version).Trim()
        if ([string]::IsNullOrWhiteSpace($version)) {
            $versionNode = $reference.SelectSingleNode("./*[local-name()='Version']")
            if ($versionNode) {
                $version = ([string]$versionNode.InnerText).Trim()
            }
        }
        $declaredNuGetDependencies += "$name|version=$version"
    }
}
$declaredNuGetDependencies = @($declaredNuGetDependencies | Sort-Object -Unique)
$inventoryNuGetDependencies = @($inventory.dependency_policy.nuget_dependencies | ForEach-Object {
    $name = ([string]$_.name).Trim().ToLowerInvariant()
    $version = ([string]$_.version).Trim()
    if ([string]::IsNullOrWhiteSpace($name)) {
        Fail-Inventory "dependency inventory contains a NuGet dependency without a name."
    }
    "$name|version=$version"
} | Sort-Object -Unique)
$nugetDifferences = @(Compare-Object -ReferenceObject $inventoryNuGetDependencies -DifferenceObject $declaredNuGetDependencies)
if ($nugetDifferences.Count -ne 0) {
    $details = @($nugetDifferences | ForEach-Object { "$($_.SideIndicator) $($_.InputObject)" }) -join '; '
    Fail-Inventory "project PackageReference declarations and inventory differ ($details)."
}

$fmtPackage = Get-InventoryPackage -Inventory $inventory -Name "fmt"
$fmtCore = Get-Content -LiteralPath (Join-Path $repoRoot "third_party/fmt/include/fmt/core.h") -Raw
if ($fmtCore -notmatch "#define\s+FMT_VERSION\s+([0-9]+)") {
    Fail-Inventory "could not find FMT_VERSION in third_party/fmt/include/fmt/core.h."
}
$fmtActual = [int]$Matches[1]
$fmtExpected = Convert-FmtVersionToInteger $fmtPackage.version
if ($fmtActual -ne $fmtExpected) {
    Fail-Inventory "vendored fmt version integer $fmtActual does not match inventory version $($fmtPackage.version) ($fmtExpected)."
}

$jsonPackage = Get-InventoryPackage -Inventory $inventory -Name "jsoncpp"
$jsonVersion = Get-Content -LiteralPath (Join-Path $repoRoot "third_party/jsoncpp/include/json/version.h") -Raw
if ($jsonVersion -notmatch '#define\s+JSONCPP_VERSION_STRING\s+"([^"]+)"') {
    Fail-Inventory "could not find JSONCPP_VERSION_STRING in third_party/jsoncpp/include/json/version.h."
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

$fmtSpdx = Join-Path $repoRoot "vcpkg_installed/x64-windows-static/x64-windows-static/share/fmt/vcpkg.spdx.json"
if (Test-Path -LiteralPath $fmtSpdx) {
    $fmtSpdxText = Get-Content -LiteralPath $fmtSpdx -Raw
    if ($fmtSpdxText -notmatch '"versionInfo"\s*:\s*"10\.1\.1"') {
        Fail-Inventory "installed vcpkg fmt SPDX metadata does not report version 10.1.1."
    }
}

$jsonSpdx = Join-Path $repoRoot "vcpkg_installed/x64-windows-static/x64-windows-static/share/jsoncpp/vcpkg.spdx.json"
if (Test-Path -LiteralPath $jsonSpdx) {
    $jsonSpdxText = Get-Content -LiteralPath $jsonSpdx -Raw
    if ($jsonSpdxText -notmatch '"versionInfo"\s*:\s*"1\.9\.5#2"') {
        Fail-Inventory "installed vcpkg JsonCpp SPDX metadata does not report version 1.9.5#2."
    }
}

Write-Host "Dependency inventory, license metadata, and third-party notices are consistent."
