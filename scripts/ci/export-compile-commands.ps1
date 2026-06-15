param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [string]$Project = "src\game.vcxproj",
    [string]$Output = "build\compile_commands.json",
    [string]$Compiler = "clang-cl.exe"
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

$repoRoot = Get-RepoRoot
$projectPath = Join-Path $repoRoot $Project
$projectDir = Split-Path -Parent $projectPath

[xml]$projectXml = Get-Content -Raw -Path $projectPath
$namespace = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable)
$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")

$itemDefinition = $projectXml.SelectSingleNode("//msb:ItemDefinitionGroup[contains(@Condition, '$Configuration|$Platform')]/msb:ClCompile", $namespace)
if (-not $itemDefinition) {
    throw "Could not find ClCompile settings for $Configuration|$Platform in $Project."
}

$defines = @(Split-MSBuildList $itemDefinition.PreprocessorDefinitions)
$disabledWarnings = @(Split-MSBuildList $itemDefinition.DisableSpecificWarnings)
$includeDirs = @(Split-MSBuildList $itemDefinition.AdditionalIncludeDirectories | ForEach-Object {
    $_.Replace('$(ProjectDir)', "$projectDir\")
})

$vcpkgInclude = Join-Path $projectDir "vcpkg_installed\x64-windows-static\x64-windows-static\include"
if (Test-Path -LiteralPath $vcpkgInclude) {
    $includeDirs += $vcpkgInclude
}

$compileItems = $projectXml.SelectNodes("//msb:ClCompile[@Include]", $namespace)
$commands = foreach ($item in $compileItems) {
    $sourcePath = Join-Path $projectDir $item.Include
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        continue
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
}

$outputPath = Join-Path $repoRoot $Output
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null
$commands | ConvertTo-Json -Depth 8 | Set-Content -Path $outputPath -Encoding utf8

Write-Host "Wrote $($commands.Count) compile commands to $outputPath"
