param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [string]$Project = "projects\msvc\game.vcxproj",
    [string]$Output = "build\compile_commands.json",
    [string]$Compiler = "clang-cl.exe",

    [switch]$GhostRuntimeTesting
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

function Split-MSBuildList {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return @()
    }

    return @($Value -split ";" | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_) -and $_ -notmatch "%\("
    })
}

function Expand-CompileSetting {
    param(
        [string]$Value,
        [hashtable]$Properties
    )

    $expanded = $Value
    foreach ($property in $Properties.GetEnumerator()) {
        $expanded = $expanded.Replace("`$($($property.Key))", [string]$property.Value)
    }

    # Item metadata expressions are removed by Split-MSBuildList after list
    # expansion. Property expressions must be resolved here because they may
    # prefix a real definition, as MMGhostRuntimeTestingDefine does.
    if ($expanded -match '\$\([^)]+\)') {
        throw "Compile setting contains an unresolved MSBuild expression: $expanded"
    }

    return $expanded
}

$repoRoot = Get-RepoRoot
$projectPath = if ([System.IO.Path]::IsPathRooted($Project)) {
    [System.IO.Path]::GetFullPath($Project)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Project))
}
$projectDir = Split-Path -Parent $projectPath

[xml]$projectXml = Get-Content -Raw -Path $projectPath
$namespace = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable)
$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")

$itemDefinition = $projectXml.SelectSingleNode("//msb:ItemDefinitionGroup[contains(@Condition, '$Configuration|$Platform')]/msb:ClCompile", $namespace)
if (-not $itemDefinition) {
    throw "Could not find ClCompile settings for $Configuration|$Platform in $Project."
}

$compileProperties = @{
    ProjectDir = "$projectDir\"
    MMGhostRuntimeTestingDefine = if ($GhostRuntimeTesting) { "MM_GHOST_RUNTIME_TESTING;" } else { "" }
}

$defines = @(Split-MSBuildList (Expand-CompileSetting `
    -Value ([string]$itemDefinition.PreprocessorDefinitions) `
    -Properties $compileProperties))
$disabledWarnings = @(Split-MSBuildList (Expand-CompileSetting `
    -Value ([string]$itemDefinition.DisableSpecificWarnings) `
    -Properties $compileProperties))
$includeDirs = @(Split-MSBuildList (Expand-CompileSetting `
    -Value ([string]$itemDefinition.AdditionalIncludeDirectories) `
    -Properties $compileProperties) | ForEach-Object {
    [System.IO.Path]::GetFullPath($_)
})

$vcpkgInclude = Join-Path $repoRoot "vcpkg_installed\x64-windows-static\x64-windows-static\include"
if (Test-Path -LiteralPath $vcpkgInclude) {
    $includeDirs += $vcpkgInclude
}

$compileItems = $projectXml.SelectNodes("//msb:ClCompile[@Include]", $namespace)
$commands = @(foreach ($item in $compileItems) {
    $sourcePath = [System.IO.Path]::GetFullPath((Join-Path $projectDir $item.Include))
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Project compile source does not exist: $($item.Include) ($sourcePath)"
    }

    $args = @(
        $Compiler,
        "/nologo",
        "/TP",
        "/std:c++17",
        "/EHsc",
        "/permissive-",
        "/c"
    )

    foreach ($define in $defines) {
        $args += "/D$define"
    }
    foreach ($includeDir in ($includeDirs | Select-Object -Unique)) {
        $args += "/I$includeDir"
    }
    foreach ($warning in $disabledWarnings) {
        $args += "/wd$warning"
    }

    $args += $sourcePath

    [ordered]@{
        directory = $projectDir
        file = $sourcePath
        arguments = $args
    }
})

if ($commands.Count -eq 0) {
    throw "No existing C++ source files were exported from $Project."
}

foreach ($command in $commands) {
    $unresolved = @($command.arguments | Where-Object {
        $_ -match '\$\([^)]+\)' -or $_ -match '%\([^)]+\)'
    })
    if ($unresolved.Count -ne 0) {
        throw "Compile command for '$($command.file)' contains unresolved MSBuild expressions: $($unresolved -join ', ')"
    }

    if ($command.arguments -notcontains "/DKEX_Q2_GAME") {
        throw "Compile command for '$($command.file)' is missing the required KEX_Q2_GAME definition."
    }

    $hasRuntimeTesting = $command.arguments -contains "/DMM_GHOST_RUNTIME_TESTING"
    if ($hasRuntimeTesting -ne [bool]$GhostRuntimeTesting) {
        throw "Compile command for '$($command.file)' has the wrong MM_GHOST_RUNTIME_TESTING state."
    }
}

$outputPath = if ([System.IO.Path]::IsPathRooted($Output)) {
    [System.IO.Path]::GetFullPath($Output)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Output))
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null
$commands | ConvertTo-Json -Depth 8 | Set-Content -Path $outputPath -Encoding utf8

Write-Host "Wrote $($commands.Count) compile commands to $outputPath"
