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

    # clang ships its sanitizer runtimes built against the release CRT, so a debug
    # build cannot link one: libcpmtd.lib carries _ITERATOR_DEBUG_LEVEL 2 and
    # clang_rt.ubsan_standalone_cxx carries 0, and lld-link refuses the pair. Build
    # this sanitizer against the release CRT unless the caller asked for a specific
    # configuration. ASAN is unaffected; it uses the MSVC runtime.
    if (-not $PSBoundParameters.ContainsKey("Configuration")) {
        $Configuration = "Release"
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
