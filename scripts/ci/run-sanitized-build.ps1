param(
    [ValidateSet("Address", "Undefined")]
    [string]$Sanitizer = "Address",

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [switch]$AllowUnsupported
)

$ErrorActionPreference = "Stop"

if ($Sanitizer -eq "Undefined") {
    $clangCl = Get-Command clang-cl -ErrorAction SilentlyContinue
    if (-not $clangCl) {
        $message = "UndefinedBehaviorSanitizer requires clang-cl on PATH."
        if ($AllowUnsupported) {
            Write-Warning $message
            exit 0
        }
        throw $message
    }
}

$arguments = @{
    Configuration = $Configuration
    Platform = $Platform
    Target = "Rebuild"
    BinaryLog = $true
    BinaryLogPath = "build\analysis\msbuild-$($Sanitizer.ToLowerInvariant())-$Configuration-$Platform.binlog"
}

if ($Sanitizer -eq "Address") {
    $arguments["EnableASAN"] = $true
    & "$PSScriptRoot\build-msbuild.ps1" @arguments
}
else {
    $arguments["EnableUBSAN"] = $true
    # A present compiler makes source, project, and linker failures actionable.
    # AllowUnsupported is only for machines where clang-cl is absent.
    & "$PSScriptRoot\build-msbuild.ps1" @arguments
}
