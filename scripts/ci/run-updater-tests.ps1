param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$project = Join-Path $repoRoot "updater\MuffMode.Updater.Tests\MuffMode.Updater.Tests.csproj"
$dotnet = Get-Command dotnet -ErrorAction Stop

Invoke-NativeCommand `
    -FilePath $dotnet.Source `
    -Arguments @(
        "run",
        "--project", $project,
        "--configuration", $Configuration,
        "--property:TreatWarningsAsErrors=true"
    ) `
    -WorkingDirectory $repoRoot
