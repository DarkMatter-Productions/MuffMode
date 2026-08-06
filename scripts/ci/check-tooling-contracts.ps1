#requires -Version 7.0

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$testRoot = Join-Path $repoRoot "build\tooling-contracts"
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null

function Fail-ToolingContract {
    param([string]$Message)
    throw "Tooling contract check failed: $Message"
}

$productionOutput = Join-Path $testRoot "compile-commands-production.json"
$runtimeOutput = Join-Path $testRoot "compile-commands-runtime.json"
& "$PSScriptRoot\export-compile-commands.ps1" -Output $productionOutput
& "$PSScriptRoot\export-compile-commands.ps1" -GhostRuntimeTesting -Output $runtimeOutput

$productionCommands = @(Get-Content -LiteralPath $productionOutput -Raw | ConvertFrom-Json)
$runtimeCommands = @(Get-Content -LiteralPath $runtimeOutput -Raw | ConvertFrom-Json)
if ($productionCommands.Count -eq 0 -or $productionCommands.Count -ne $runtimeCommands.Count) {
    Fail-ToolingContract "production and runtime compile databases must contain the same non-zero source set."
}

$productionFiles = @($productionCommands | ForEach-Object {
    [System.IO.Path]::GetFullPath([string]$_.file).ToLowerInvariant()
} | Sort-Object -Unique)
$runtimeFiles = @($runtimeCommands | ForEach-Object {
    [System.IO.Path]::GetFullPath([string]$_.file).ToLowerInvariant()
} | Sort-Object -Unique)
if ($productionFiles.Count -ne $productionCommands.Count -or
    $runtimeFiles.Count -ne $runtimeCommands.Count) {
    Fail-ToolingContract "compile databases must contain each project source exactly once."
}
$sourceDifferences = @(Compare-Object -ReferenceObject $productionFiles -DifferenceObject $runtimeFiles)
if ($sourceDifferences.Count -ne 0) {
    Fail-ToolingContract "production and runtime compile databases contain different source files."
}

foreach ($index in 0..($productionCommands.Count - 1)) {
    $productionArgs = @($productionCommands[$index].arguments)
    $runtimeArgs = @($runtimeCommands[$index].arguments)
    if ($productionArgs -notcontains "/DKEX_Q2_GAME" -or $runtimeArgs -notcontains "/DKEX_Q2_GAME") {
        Fail-ToolingContract "compile database entry $index lost KEX_Q2_GAME."
    }
    if ($productionArgs -contains "/DMM_GHOST_RUNTIME_TESTING") {
        Fail-ToolingContract "production compile database entry $index enables runtime testing."
    }
    if ($runtimeArgs -notcontains "/DMM_GHOST_RUNTIME_TESTING") {
        Fail-ToolingContract "runtime compile database entry $index omits runtime testing."
    }
    if (@($productionArgs + $runtimeArgs | Where-Object { $_ -match '\$\(|%\(' }).Count -ne 0) {
        Fail-ToolingContract "compile database entry $index contains unresolved MSBuild expressions."
    }
}

$negativeManifestPath = Join-Path $testRoot "vcpkg-uninventoried.json"
$negativeManifest = Get-Content -LiteralPath (Join-Path $repoRoot "vcpkg.json") -Raw | ConvertFrom-Json
$negativeManifest.dependencies = @($negativeManifest.dependencies) + "muffmode-uninventoried-contract-test"
$negativeManifest | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $negativeManifestPath -Encoding utf8

$rejectedExtraDependency = $false
try {
    & "$PSScriptRoot\check-dependency-inventory.ps1" -ManifestPath $negativeManifestPath
}
catch {
    if ($_.Exception.Message -notmatch "manifest-only: muffmode-uninventoried-contract-test") {
        throw
    }
    $rejectedExtraDependency = $true
}
if (-not $rejectedExtraDependency) {
    Fail-ToolingContract "dependency inventory accepted an unreviewed manifest dependency."
}

$negativeFeatureManifestPath = Join-Path $testRoot "vcpkg-uninventoried-feature.json"
$negativeFeatureManifest = Get-Content -LiteralPath (Join-Path $repoRoot "vcpkg.json") -Raw | ConvertFrom-Json
$negativeFeatureManifest.dependencies[0] = [pscustomobject]@{
    name = [string]$negativeFeatureManifest.dependencies[0]
    features = @("muffmode-contract-feature")
}
$negativeFeatureManifest | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $negativeFeatureManifestPath -Encoding utf8

$rejectedFeatureChange = $false
try {
    & "$PSScriptRoot\check-dependency-inventory.ps1" -ManifestPath $negativeFeatureManifestPath
}
catch {
    if ($_.Exception.Message -notmatch "features=muffmode-contract-feature") {
        throw
    }
    $rejectedFeatureChange = $true
}
if (-not $rejectedFeatureChange) {
    Fail-ToolingContract "dependency inventory accepted an unreviewed vcpkg feature change."
}

$negativeOverrideManifestPath = Join-Path $testRoot "vcpkg-uninventoried-override.json"
$negativeOverrideManifest = Get-Content -LiteralPath (Join-Path $repoRoot "vcpkg.json") -Raw | ConvertFrom-Json
$negativeOverrideManifest | Add-Member -NotePropertyName overrides -NotePropertyValue @(
    [pscustomobject]@{ name = "fmt"; version = "10.1.0" }
)
$negativeOverrideManifest | ConvertTo-Json -Depth 20 |
    Set-Content -LiteralPath $negativeOverrideManifestPath -Encoding utf8

$rejectedOverride = $false
try {
    & "$PSScriptRoot\check-dependency-inventory.ps1" -ManifestPath $negativeOverrideManifestPath
}
catch {
    if ($_.Exception.Message -notmatch "unreviewed top-level properties: overrides") {
        throw
    }
    $rejectedOverride = $true
}
if (-not $rejectedOverride) {
    Fail-ToolingContract "dependency inventory accepted an unreviewed vcpkg override."
}

$vcpkgSetup = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\ci\setup-vcpkg.ps1") -Raw
if ($vcpkgSetup -match '(?i)fetch[^\r\n]+origin\W+master' -or
    $vcpkgSetup -notmatch "builtin-baseline") {
    Fail-ToolingContract "vcpkg setup must use the manifest's immutable baseline, not a mutable branch."
}
foreach ($cacheBootstrapContract in @(
    'allowedCacheDirectoryNames = @("downloads", "buildtrees", "packages")',
    '[System.IO.FileAttributes]::ReparsePoint',
    '@("init", $Root)'
)) {
    if ($vcpkgSetup -notmatch [regex]::Escape($cacheBootstrapContract)) {
        Fail-ToolingContract "vcpkg setup no longer safely initializes an actions/cache-only root."
    }
}
$bootstrapInvocation = $vcpkgSetup.IndexOf(
    'Invoke-NativeCommand -FilePath $bootstrap',
    [System.StringComparison]::Ordinal)
$vcpkgExeGuard = $vcpkgSetup.IndexOf(
    'if (-not (Test-Path -LiteralPath $vcpkgExe',
    [System.StringComparison]::Ordinal)
if ($bootstrapInvocation -lt 0 -or
    ($vcpkgExeGuard -ge 0 -and $vcpkgExeGuard -lt $bootstrapInvocation)) {
    Fail-ToolingContract "vcpkg.exe must be rebuilt from the pinned checkout instead of conditionally reused."
}

$fuzzWorkflow = Get-Content -LiteralPath (Join-Path $repoRoot ".github\workflows\fuzz.yml") -Raw
if ($fuzzWorkflow -match '(?m)^\s*continue-on-error\s*:') {
    Fail-ToolingContract "the fuzz job must not hide failures with continue-on-error."
}
if ($fuzzWorkflow -match 'build-fuzz-targets\.ps1[^\r\n]*-AllowUnsupported') {
    Fail-ToolingContract "the CI fuzz build must not classify target failures as unsupported."
}
foreach ($requiredCommand in @("build-fuzz-targets.ps1", "run-fuzz-smoke.ps1")) {
    if ($fuzzWorkflow -notmatch [regex]::Escape($requiredCommand)) {
        Fail-ToolingContract "the fuzz workflow is missing $requiredCommand."
    }
}
if ($fuzzWorkflow -notmatch [regex]::Escape('22.1.7')) {
    Fail-ToolingContract "the fuzz workflow must validate its pinned LLVM version."
}

$analysisWorkflow = Get-Content -LiteralPath (Join-Path $repoRoot ".github\workflows\analysis.yml") -Raw
foreach ($requiredCommand in @(
    "run-msvc-analyze.ps1",
    "export-compile-commands.ps1",
    "run-clang-tidy.ps1",
    "run-cppcheck.ps1",
    "run-sanitized-build.ps1"
)) {
    if ($analysisWorkflow -notmatch [regex]::Escape($requiredCommand)) {
        Fail-ToolingContract "the analysis workflow is missing $requiredCommand."
    }
}
if ($analysisWorkflow -match 'run-sanitized-build\.ps1[^\r\n]*-AllowUnsupported') {
    Fail-ToolingContract "sanitizer CI must expose compiler and linker failures."
}
# Cppcheck is deliberately informational in CI while its baseline over the vanilla
# and upstream tree is untriaged, so this no longer requires the analysis job to
# fail on its findings. Local push verification below still treats them as blocking.
if ($analysisWorkflow -notmatch '(?s)Run Cppcheck.*?continue-on-error\s*:\s*true') {
    Fail-ToolingContract "Cppcheck is informational until its baseline is triaged; update this contract when its findings become blocking."
}
if ($analysisWorkflow -match 'HEAD~1') {
    Fail-ToolingContract "clang-tidy must scan the full compile database when no trustworthy comparison SHA exists."
}
if ($analysisWorkflow -notmatch [regex]::Escape('$env:GITHUB_PATH') -or
    $analysisWorkflow -notmatch [regex]::Escape('C:\Program Files\LLVM\bin') -or
    $analysisWorkflow -notmatch [regex]::Escape('C:\Program Files\Cppcheck') -or
    $analysisWorkflow -notmatch [regex]::Escape('Split-Path -Parent')) {
    Fail-ToolingContract "analyzer tool paths must persist into later GitHub Actions steps."
}
foreach ($toolVersion in @('22.1.7', '2.19.0')) {
    if ($analysisWorkflow -notmatch [regex]::Escape($toolVersion)) {
        Fail-ToolingContract "the analysis workflow is missing pinned tool version $toolVersion."
    }
}
if ($analysisWorkflow -notmatch '(?s)static-analysis:.*?timeout-minutes:\s*120') {
    Fail-ToolingContract "the full static-analysis job needs its reviewed 120-minute completion budget."
}

$buildWorkflow = Get-Content -LiteralPath (Join-Path $repoRoot ".github\workflows\build.yml") -Raw
foreach ($newBranchContract in @(
    'github.event.repository.default_branch',
    'git merge-base HEAD',
    'timeout-minutes: 90'
)) {
    if ($buildWorkflow -notmatch [regex]::Escape($newBranchContract)) {
        Fail-ToolingContract "the build workflow lost new-branch or timeout contract '$newBranchContract'."
    }
}

$pushVerifier = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\ci\verify-github-push.ps1") -Raw
if ($pushVerifier -match 'Invoke-OptionalCppcheck' -or
    $pushVerifier -match 'matching the non-blocking GitHub analysis job' -or
    $pushVerifier -notmatch [regex]::Escape('scripts\ci\run-cppcheck.ps1')) {
    Fail-ToolingContract "local push verification must treat Cppcheck findings as blocking."
}
foreach ($pushContract in @(
    '--event push',
    'ls-remote", "--symref"',
    'merge-base", "HEAD"',
    'GitHubTimeoutMinutes = 150'
)) {
    if ($pushVerifier -notmatch [regex]::Escape($pushContract)) {
        Fail-ToolingContract "push verification lost '$pushContract'."
    }
}
if ($pushVerifier -match 'HEAD~1') {
    Fail-ToolingContract "new-branch push verification must not inspect only the last commit."
}

$changelogCheck = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\ci\check-changelog.ps1") -Raw
if ($changelogCheck -notmatch [regex]::Escape('all-zero new-branch sentinel')) {
    Fail-ToolingContract "the changelog gate must reject unresolved new-branch sentinels."
}
$dependencyCheck = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\ci\check-dependency-inventory.ps1") -Raw
foreach ($importPattern in @('*.props', '*.targets')) {
    if ($dependencyCheck -notmatch [regex]::Escape($importPattern)) {
        Fail-ToolingContract "dependency inventory does not inspect tracked $importPattern imports."
    }
}
$generatedArtifactCheck = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\ci\check-generated-artifacts.ps1") -Raw
if ($generatedArtifactCheck -match [regex]::Escape('--exclude-standard') -or
    $generatedArtifactCheck -notmatch [regex]::Escape('--exclude-per-directory=.gitignore')) {
    Fail-ToolingContract "generated-artifact checks must use repository-owned ignore rules only."
}

$workflowPaths = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot ".github\workflows") `
    -Filter "*.yml" -File)
foreach ($workflowPath in $workflowPaths) {
    foreach ($line in Get-Content -LiteralPath $workflowPath.FullName) {
        if ($line -notmatch '^\s*uses:\s*[^@\s]+@(?<ref>[^\s#]+)') {
            continue
        }
        if ($Matches['ref'] -notmatch '^[0-9a-fA-F]{40}$') {
            Fail-ToolingContract "workflow action is not commit-pinned in '$($workflowPath.Name)': $($line.Trim())"
        }
    }
}

$clangTidyConfig = Get-Content -LiteralPath (Join-Path $repoRoot ".clang-tidy") -Raw
if ($clangTidyConfig -notmatch '(?m)^WarningsAsErrors:\s*[''\"]{2}\s*$') {
    Fail-ToolingContract "the full clang-tidy report must remain advisory while the blocking ratchet is applied by the runner."
}
$clangTidyRunner = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\ci\run-clang-tidy.ps1") -Raw
$clangTidyBlocking = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\ci\clang-tidy-blocking-checks.txt") -Raw
foreach ($blockingContract in @(
    'clang-analyzer-core.*',
    'clang-analyzer-cplusplus.NewDelete*',
    'clang-analyzer-deadcode.*',
    'clang-analyzer-nullability.*',
    'clang-analyzer-security.*',
    'clang-analyzer-unix.*',
    'bugprone-undefined-memory-manipulation',
    'bugprone-use-after-move'
)) {
    if ($clangTidyBlocking -notmatch [regex]::Escape($blockingContract)) {
        Fail-ToolingContract "clang-tidy blocking ratchet is missing '$blockingContract'."
    }
}
if ($clangTidyRunner -notmatch [regex]::Escape('blocking correctness/safety finding(s)')) {
    Fail-ToolingContract "clang-tidy runner does not enforce its blocking ratchet."
}

$aliasedChecks = @(Get-ClangTidyDiagnosticChecks -Line `
    'source.cpp:1:1: warning: fixture [modernize-use-auto,clang-analyzer-core.NullDereference,-warnings-as-errors]')
if ($aliasedChecks.Count -ne 2 -or
    -not (Test-ClangTidyBlockingCheck `
        -Checks $aliasedChecks -Patterns @('clang-analyzer-core.*')) -or
    (Test-ClangTidyBlockingCheck `
        -Checks $aliasedChecks -Patterns @('bugprone-use-after-move'))) {
    Fail-ToolingContract "clang-tidy aliases are not parsed and matched exactly."
}
foreach ($clangRunnerContract in @(
    "`$PSBoundParameters.ContainsKey('Files')",
    'scripts/ci/clang-tidy-blocking-checks.txt',
    '--use-color=false',
    '--list-checks'
)) {
    if ($clangTidyRunner -notmatch [regex]::Escape($clangRunnerContract)) {
        Fail-ToolingContract "clang-tidy selection or diagnostic parsing lost '$clangRunnerContract'."
    }
}
$clangPreflightIndex = $clangTidyRunner.IndexOf(
    '"--list-checks"', [System.StringComparison]::Ordinal)
$clangEmptySelectionIndex = $clangTidyRunner.IndexOf(
    'if ($Files.Count -eq 0)', [System.StringComparison]::Ordinal)
if ($clangPreflightIndex -lt 0 -or $clangEmptySelectionIndex -lt 0 -or
    $clangPreflightIndex -gt $clangEmptySelectionIndex) {
    Fail-ToolingContract "clang-tidy policy preflight must run before an empty selection can succeed."
}

$msvcAnalyzeScript = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\ci\run-msvc-analyze.ps1") -Raw
$analysisProps = Get-Content -LiteralPath (Join-Path $repoRoot "projects\msvc\MuffMode.Analysis.props") -Raw
foreach ($msvcContract in @(
    '-TreatWarningsAsErrors',
    'msvc-analyze-blocking-warnings.txt',
    'blocking analyzer finding(s)'
)) {
    if ($msvcAnalyzeScript -notmatch [regex]::Escape($msvcContract)) {
        Fail-ToolingContract "MSVC code-analysis ratchet is missing '$msvcContract'."
    }
}
if ($analysisProps -notmatch [regex]::Escape('/analyze:WX-')) {
    Fail-ToolingContract "MSVC analyzer warnings must remain advisory until the reviewed ratchet parses them."
}

$msvcBlockingPath = Join-Path $repoRoot "scripts\ci\msvc-analyze-blocking-warnings.txt"
$msvcBlocking = @(Read-AnalyzerBlockingList `
    -Path $msvcBlockingPath -EntryPattern '^C\d{4,5}$')
foreach ($requiredMsvcCode in @(
    'C6001', 'C6011', 'C6031', 'C6386', 'C6387',
    'C26815', 'C26865', 'C28020', 'C28182'
)) {
    if ($msvcBlocking -notcontains $requiredMsvcCode) {
        Fail-ToolingContract "MSVC blocking ratchet is missing $requiredMsvcCode."
    }
}
$directFinding = Get-MsvcAnalyzerFinding -Line `
    '2>E:\repo\source.cpp(7): warning C26865: fixture [game.vcxproj]'
$summaryFinding = Get-MsvcAnalyzerFinding -Line `
    '  E:\repo\source.cpp(7): warning C26865: fixture [game.vcxproj]'
$findingKeys = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
if (-not $directFinding -or -not $summaryFinding -or
    $directFinding.Code -ne 'C26865' -or
    $directFinding.Text -ne $summaryFinding.Text -or
    -not $findingKeys.Add($directFinding.Text) -or
    $findingKeys.Add($summaryFinding.Text)) {
    Fail-ToolingContract "MSVC analyzer findings are not parsed and deduplicated exactly."
}

$invalidAnalyzerList = Join-Path $testRoot 'invalid-analyzer-list.txt'
@('C6001', 'C6001') | Set-Content -LiteralPath $invalidAnalyzerList -Encoding utf8
$invalidListRejected = $false
try {
    @(Read-AnalyzerBlockingList `
        -Path $invalidAnalyzerList -EntryPattern '^C\d{4,5}$') | Out-Null
}
catch {
    $invalidListRejected = $true
}
if (-not $invalidListRejected) {
    Fail-ToolingContract "duplicate analyzer blocking-list entries must fail closed."
}

Write-Host "Compile database, dependency inventory, analysis, fuzz, and sanitizer contracts are valid."
