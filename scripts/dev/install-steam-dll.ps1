param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [string]$Quake2Root = $env:MUFFMODE_QUAKE2_ROOT
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function ConvertFrom-VdfString {
    param([Parameter(Mandatory = $true)][string]$Value)

    return $Value.Replace('\\', '\').Replace('\/', '/').Replace('\"', '"')
}

function Resolve-ExistingDirectory {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    try {
        $expanded = [Environment]::ExpandEnvironmentVariables($Path)
        if (Test-Path -LiteralPath $expanded -PathType Container) {
            return (Resolve-Path -LiteralPath $expanded).Path
        }
    }
    catch {
        return $null
    }

    return $null
}

function Get-SteamRootCandidates {
    $candidates = @()
    $registryKeys = @(
        "HKCU:\Software\Valve\Steam",
        "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam",
        "HKLM:\SOFTWARE\Valve\Steam"
    )

    foreach ($key in $registryKeys) {
        try {
            $properties = Get-ItemProperty -LiteralPath $key -ErrorAction Stop
            foreach ($propertyName in @("InstallPath", "SteamPath")) {
                if ($properties.PSObject.Properties.Name -contains $propertyName) {
                    $candidates += $properties.$propertyName
                }
            }
        }
        catch {
        }
    }

    if (${env:ProgramFiles(x86)}) {
        $candidates += (Join-Path ${env:ProgramFiles(x86)} "Steam")
    }
    if ($env:ProgramFiles) {
        $candidates += (Join-Path $env:ProgramFiles "Steam")
    }

    $candidates |
        ForEach-Object { Resolve-ExistingDirectory $_ } |
        Where-Object { $_ } |
        Sort-Object -Unique
}

function Get-SteamLibraryRoots {
    $libraries = @()

    foreach ($steamRoot in Get-SteamRootCandidates) {
        $libraries += $steamRoot
        $libraryFolders = Join-Path $steamRoot "steamapps\libraryfolders.vdf"

        if (-not (Test-Path -LiteralPath $libraryFolders -PathType Leaf)) {
            continue
        }

        foreach ($line in Get-Content -LiteralPath $libraryFolders) {
            if ($line -match '^\s*"path"\s+"([^"]+)"') {
                $libraries += (ConvertFrom-VdfString $Matches[1])
            }
        }
    }

    $libraries |
        ForEach-Object { Resolve-ExistingDirectory $_ } |
        Where-Object { $_ } |
        Sort-Object -Unique
}

function Get-SteamAppInstallDir {
    param([Parameter(Mandatory = $true)][string]$ManifestPath)

    foreach ($line in Get-Content -LiteralPath $ManifestPath) {
        if ($line -match '^\s*"installdir"\s+"([^"]+)"') {
            return ConvertFrom-VdfString $Matches[1]
        }
    }

    return "Quake 2"
}

function Get-Quake2RootCandidates {
    if (-not [string]::IsNullOrWhiteSpace($Quake2Root)) {
        $explicitRoot = Resolve-ExistingDirectory $Quake2Root
        if ($explicitRoot) {
            return @($explicitRoot)
        }

        throw "MUFFMODE_QUAKE2_ROOT or -Quake2Root points to a directory that does not exist: $Quake2Root"
    }

    $candidates = @()
    foreach ($libraryRoot in Get-SteamLibraryRoots) {
        $steamApps = Join-Path $libraryRoot "steamapps"
        $common = Join-Path $steamApps "common"
        $manifest = Join-Path $steamApps "appmanifest_2320.acf"

        if (Test-Path -LiteralPath $manifest -PathType Leaf) {
            $installDir = Get-SteamAppInstallDir -ManifestPath $manifest
            $candidates += (Join-Path $common $installDir)
        }

        $candidates += (Join-Path $common "Quake 2")
    }

    $candidates |
        ForEach-Object { Resolve-ExistingDirectory $_ } |
        Where-Object { $_ } |
        Sort-Object -Unique
}

function Resolve-Quake2SteamRoot {
    $candidates = @(Get-Quake2RootCandidates)

    foreach ($candidate in $candidates) {
        $baseq2 = Join-Path $candidate "rerelease\baseq2"
        if (Test-Path -LiteralPath $baseq2 -PathType Container) {
            return $candidate
        }
    }

    if ($candidates.Count -gt 0) {
        throw "Found Quake II candidate(s), but none contained rerelease\baseq2: $($candidates -join '; ')"
    }

    throw "Could not find the Steam Quake II install. Set MUFFMODE_QUAKE2_ROOT to the outer Quake 2 folder and retry."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourceDll = Join-Path $repoRoot "build\msbuild\$Platform\$Configuration\game_x64.dll"

if (-not (Test-Path -LiteralPath $sourceDll -PathType Leaf)) {
    throw "Built DLL not found: $sourceDll"
}

$quake2Root = Resolve-Quake2SteamRoot
$destinationDll = Join-Path $quake2Root "rerelease\baseq2\game_x64.dll"
$destinationDirectory = Split-Path -Parent $destinationDll

if (-not (Test-Path -LiteralPath $destinationDirectory -PathType Container)) {
    throw "Quake II rerelease baseq2 directory not found: $destinationDirectory"
}

try {
    Copy-Item -LiteralPath $sourceDll -Destination $destinationDll -Force
}
catch {
    throw "Could not replace $destinationDll. Close Quake II if it is running, then retry. $($_.Exception.Message)"
}

Write-Host "Installed $Configuration|$Platform DLL:"
Write-Host "  $sourceDll"
Write-Host "  -> $destinationDll"
