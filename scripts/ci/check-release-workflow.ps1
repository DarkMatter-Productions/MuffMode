#requires -Version 7.0

[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$Workflow = ".github/workflows/release.yml"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$workflowPath = if ([System.IO.Path]::IsPathRooted($Workflow)) {
    [System.IO.Path]::GetFullPath($Workflow)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Workflow))
}

if (-not (Test-Path -LiteralPath $workflowPath -PathType Leaf)) {
    throw "Release workflow contract check failed: workflow not found: $workflowPath"
}

$workflowText = Get-Content -LiteralPath $workflowPath -Raw

function Fail-ReleaseWorkflowContract {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    throw "Release workflow contract check failed: $Message"
}

function Get-SnippetCount {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Snippet
    )

    return [regex]::Matches($Text, [regex]::Escape($Snippet)).Count
}

function Get-WorkflowJobBlock {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,

        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $pattern = "(?ms)^  $([regex]::Escape($Name)):\r?\n(?<body>.*?)(?=^  [A-Za-z0-9_-]+:\r?\n|\z)"
    $match = [regex]::Match($Text, $pattern)
    if (-not $match.Success) {
        Fail-ReleaseWorkflowContract "could not find the '$Name' job."
    }

    return $match.Value
}

function Assert-NoExpressionsInRunScripts {
    param([string]$Text)

    $lines = @($Text -split "`r?`n")
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -match '^\s*run\s*:\s*[^|>].*\$\{\{') {
            Fail-ReleaseWorkflowContract "GitHub expressions must be passed through step env instead of interpolated into run commands (line $($i + 1))."
        }
        if ($line -notmatch '^(?<indent>\s*)run\s*:\s*[|>][+-]?\s*$') {
            continue
        }

        $baseIndent = $Matches.indent.Length
        for ($j = $i + 1; $j -lt $lines.Count; $j++) {
            $bodyLine = $lines[$j]
            if ([string]::IsNullOrWhiteSpace($bodyLine)) {
                continue
            }
            $bodyIndent = $bodyLine.Length - $bodyLine.TrimStart().Length
            if ($bodyIndent -le $baseIndent) {
                break
            }
            if ($bodyLine.Contains('${{')) {
                Fail-ReleaseWorkflowContract "GitHub expressions must be passed through step env instead of interpolated into run scripts (line $($j + 1))."
            }
        }
    }
}

function Assert-JobContentsPermission {
    param(
        [Parameter(Mandatory = $true)]
        [string]$JobBlock,

        [Parameter(Mandatory = $true)]
        [string]$JobName,

        [Parameter(Mandatory = $true)]
        [ValidateSet("read", "write")]
        [string]$Expected
    )

    $pattern = "(?m)^    permissions:\r?\n      contents: $Expected\s*$"
    if (-not [regex]::IsMatch($JobBlock, $pattern)) {
        Fail-ReleaseWorkflowContract "the '$JobName' job must declare contents: $Expected."
    }
}

$versionOutput = 'target_sha: ${{ steps.version.outputs.target_sha }}'
$selfCheck = 'run: ./scripts/ci/check-release-workflow.ps1'
$versionResolution = '$targetSha = "$(git rev-parse HEAD)".Trim().ToLowerInvariant()'
$versionEmission = '"target_sha=$targetSha" >> $env:GITHUB_OUTPUT'
$buildCheckout = 'ref: ${{ needs.version.outputs.target_sha }}'
$buildExpectedResolution = '$expectedSha = $env:RELEASE_TARGET_SHA.Trim().ToLowerInvariant()'
$buildActualResolution = '$actualSha = "$(git rev-parse HEAD)".Trim().ToLowerInvariant()'
$buildCheckoutComparison = 'if ($actualSha -cne $expectedSha) {'
$publishResolution = '$targetSha = $env:RELEASE_TARGET_SHA.Trim()'
$publishTagAssertion = 'Assert-RemoteReleaseTag `'
$publishVerifyTag = '"--verify-tag",'
$toolingContract = 'run: ./scripts/ci/check-tooling-contracts.ps1'
$regressionCorpus = 'run: ./scripts/ci/check-regression-corpus.ps1'
$dependencyInventory = 'run: ./scripts/ci/check-dependency-inventory.ps1'
$hostTests = 'run: ./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64'
$updaterTests = 'run: ./scripts/ci/run-updater-tests.ps1 -Configuration Release'
$strictBuild = 'run: ./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -TreatWarningsAsErrors -BinaryLog'
$packageSkipBuild = 'SkipBuild = $true'
$provenanceUpload = 'dist/release/*.json'
$checksumUpload = 'dist/release/*.txt'
$provenanceSourceCheck = '[string]$provenanceData.SourceCommit -cne $targetSha'
$checksumVerification = 'throw "Release checksum mismatch for $name."'
$publishedMetadata = '$assets = @($binaryAssets) + @($provenance, $checksums)'
$exactPackageName = '$packageBaseName = "muffmode-$version$channelSuffix"'
$provenancePropertySet = '$requiredProvenanceProperties = @("SchemaVersion", "Repository", "Version", "TagName", "Channel", "SourceCommit", "SourceTreeDirty", "Assets")'
$buildReceipt = 'muffmode-build-receipt.json'

$requiredOnce = [ordered]@{
    "version job target_sha output" = $versionOutput
    "pre-mutation provenance contract check" = $selfCheck
    "post-version target SHA resolution" = $versionResolution
    "target_sha step output emission" = $versionEmission
    "immutable build checkout" = $buildCheckout
    "build expected SHA resolution" = $buildExpectedResolution
    "build actual SHA resolution" = $buildActualResolution
    "build checkout equality check" = $buildCheckoutComparison
    "publish target_sha resolution" = $publishResolution
    "remote release-tag assertion" = $publishTagAssertion
    "verified GitHub release tag argument" = $publishVerifyTag
    "release tooling contract gate" = $toolingContract
    "release regression corpus gate" = $regressionCorpus
    "release dependency inventory gate" = $dependencyInventory
    "release host test gate" = $hostTests
    "release updater test gate" = $updaterTests
    "strict release build" = $strictBuild
    "package build reuse" = $packageSkipBuild
    "provenance artifact upload" = $provenanceUpload
    "checksum artifact upload" = $checksumUpload
    "provenance source-commit validation" = $provenanceSourceCheck
    "checksum verification" = $checksumVerification
    "provenance and checksum publication" = $publishedMetadata
    "exact channel-bound package naming" = $exactPackageName
    "required provenance property set" = $provenancePropertySet
    "commit-bound strict build receipt" = $buildReceipt
}

foreach ($contract in $requiredOnce.GetEnumerator()) {
    $count = Get-SnippetCount -Text $workflowText -Snippet $contract.Value
    if ($count -ne 1) {
        Fail-ReleaseWorkflowContract "expected exactly one $($contract.Key), found $count."
    }
}

Assert-NoExpressionsInRunScripts -Text $workflowText

$targetValidationPattern = '(?m)^\s*if \(\$targetSha -cnotmatch "\^\[0-9a-f\]\{40\}\$"\) \{\s*$'
$targetValidationMatches = [regex]::Matches($workflowText, $targetValidationPattern)
if ($targetValidationMatches.Count -ne 2) {
    Fail-ReleaseWorkflowContract "expected target_sha validation in the version and publish jobs, found $($targetValidationMatches.Count) checks."
}

$mutableCheckoutPattern = '(?m)^\s*ref:\s*\$\{\{\s*github\.ref_name\s*\}\}\s*$'
if ([regex]::IsMatch($workflowText, $mutableCheckoutPattern)) {
    Fail-ReleaseWorkflowContract "build checkout must not use mutable github.ref_name."
}

$expectedActionPins = [ordered]@{
    'actions/checkout@d23441a48e516b6c34aea4fa41551a30e30af803 # v6' = 2
    'actions/setup-node@249970729cb0ef3589644e2896645e5dc5ba9c38 # v6' = 1
    'actions/setup-dotnet@26b0ec14cb23fa6904739307f278c14f94c95bf1 # v5' = 1
    'microsoft/setup-msbuild@30375c66a4eea26614e0d39710365f22f8b0af57 # v3' = 1
    'actions/cache@caa296126883cff596d87d8935842f9db880ef25 # v5' = 1
    'actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a # v7' = 1
    'actions/download-artifact@37930b1c2abaa49bbe596cd826c3c89aef350131 # v7' = 1
    'actions/github-script@3a2844b7e9c422d3c10d287c895573f7108da1b3 # v9' = 1
}
foreach ($pin in $expectedActionPins.GetEnumerator()) {
    $count = Get-SnippetCount -Text $workflowText -Snippet "uses: $($pin.Key)"
    if ($count -ne $pin.Value) {
        Fail-ReleaseWorkflowContract "expected $($pin.Value) use(s) of immutable action pin '$($pin.Key)', found $count."
    }
}
$allUses = [regex]::Matches($workflowText, '(?m)^\s*uses:\s*[^\s@]+@(?<ref>[^\s#]+)(?:\s+#\s+v\d+)?\s*$')
if ($allUses.Count -ne ($expectedActionPins.Values | Measure-Object -Sum).Sum) {
    Fail-ReleaseWorkflowContract "release workflow contains an unexpected or malformed third-party action use."
}
foreach ($use in $allUses) {
    if ($use.Groups['ref'].Value -cnotmatch '^[0-9a-f]{40}$') {
        Fail-ReleaseWorkflowContract "every third-party release action must use an immutable lowercase commit SHA."
    }
}

$jobsIndex = $workflowText.IndexOf("jobs:", [System.StringComparison]::Ordinal)
if ($jobsIndex -lt 0) {
    Fail-ReleaseWorkflowContract "workflow does not contain jobs."
}
$workflowPreamble = $workflowText.Substring(0, $jobsIndex)
if (-not [regex]::IsMatch($workflowPreamble, '(?m)^permissions:\r?\n  contents: read\s*$')) {
    Fail-ReleaseWorkflowContract "workflow default permissions must be contents: read."
}
if ($workflowPreamble -match '(?m)^\s+contents:\s*write\s*$') {
    Fail-ReleaseWorkflowContract "workflow-wide contents: write is forbidden."
}
if ($workflowText -match '(?m)^\s+copilot-requests:\s*write\s*$') {
    Fail-ReleaseWorkflowContract "Copilot request permission must not be granted workflow-wide."
}

$versionJob = Get-WorkflowJobBlock -Text $workflowText -Name "version"
$buildJob = Get-WorkflowJobBlock -Text $workflowText -Name "build"
$publishJob = Get-WorkflowJobBlock -Text $workflowText -Name "publish"
Assert-JobContentsPermission -JobBlock $versionJob -JobName "version" -Expected "write"
Assert-JobContentsPermission -JobBlock $buildJob -JobName "build" -Expected "read"
Assert-JobContentsPermission -JobBlock $publishJob -JobName "publish" -Expected "write"

$writePermissionMatches = [regex]::Matches($workflowText, '(?m)^\s+contents:\s*write\s*$')
if ($writePermissionMatches.Count -ne 2) {
    Fail-ReleaseWorkflowContract "only the version and publish jobs may declare contents: write."
}

$buildStepsIndex = $buildJob.IndexOf("    steps:", [System.StringComparison]::Ordinal)
if ($buildStepsIndex -lt 0) {
    Fail-ReleaseWorkflowContract "build job does not contain steps."
}
$buildJobConfiguration = $buildJob.Substring(0, $buildStepsIndex)
if ($buildJobConfiguration -match '(?m)^\s+(?:GH_TOKEN|GITHUB_TOKEN|COPILOT_GITHUB_TOKEN|COPILOT_AUTH_TOKEN)\s*:') {
    Fail-ReleaseWorkflowContract "release credentials must not be declared at build-job scope."
}
foreach ($jobContract in @(
    @{ Name = "version"; Block = $versionJob },
    @{ Name = "publish"; Block = $publishJob }
)) {
    $stepsIndex = $jobContract.Block.IndexOf("    steps:", [System.StringComparison]::Ordinal)
    if ($stepsIndex -lt 0) {
        Fail-ReleaseWorkflowContract "the '$($jobContract.Name)' job does not contain steps."
    }
    $jobConfiguration = $jobContract.Block.Substring(0, $stepsIndex)
    if ($jobConfiguration -match '(?m)^\s+(?:GH_TOKEN|GITHUB_TOKEN|COPILOT_GITHUB_TOKEN|COPILOT_AUTH_TOKEN)\s*:') {
        Fail-ReleaseWorkflowContract "release credentials must not be declared at $($jobContract.Name)-job scope."
    }
}
if ($buildJob -match '(?m)^\s+GITHUB_TOKEN\s*:') {
    Fail-ReleaseWorkflowContract "the build job must not expose GITHUB_TOKEN; its package step uses a read-only GH_TOKEN."
}
if ($workflowText.Contains('$env:GITHUB_ENV')) {
    Fail-ReleaseWorkflowContract "release credentials must remain step-local instead of being written to GITHUB_ENV."
}
if ((Get-SnippetCount -Text $workflowText -Snippet 'GH_TOKEN: ${{ github.token }}') -ne 3) {
    Fail-ReleaseWorkflowContract "GH_TOKEN must appear only in the version, package, and publish steps."
}
if ($workflowText -match '(?m)^\s+GITHUB_TOKEN\s*:') {
    Fail-ReleaseWorkflowContract "GITHUB_TOKEN must not be exported explicitly by the release workflow."
}
if (
    (Get-SnippetCount -Text $workflowText -Snippet '${{ secrets.COPILOT_GITHUB_TOKEN') -ne 1 -or
    (Get-SnippetCount -Text $workflowText -Snippet 'secrets.RELEASE_BOT_TOKEN }}') -ne 1
) {
    Fail-ReleaseWorkflowContract "Copilot credentials must appear only in the step-local fallback expression used for packaging."
}

$environmentInputContracts = [ordered]@{
    'RELEASE_INPUT_VERSION: ${{ inputs.version }}' = 1
    'RELEASE_PREVIOUS_TAG: ${{ inputs.previous_tag }}' = 2
    'RELEASE_REF_NAME: ${{ github.ref_name }}' = 1
    'RELEASE_REF_TYPE: ${{ github.ref_type }}' = 1
    'RELEASE_TARGET_SHA: ${{ needs.version.outputs.target_sha }}' = 3
}
foreach ($contract in $environmentInputContracts.GetEnumerator()) {
    $count = Get-SnippetCount -Text $workflowText -Snippet $contract.Key
    if ($count -ne $contract.Value) {
        Fail-ReleaseWorkflowContract "release input '$($contract.Key)' must be passed through step env exactly $($contract.Value) time(s), found $count."
    }
}
if ((Get-SnippetCount -Text $workflowText -Snippet 'git check-ref-format "refs/tags/$previousTag"') -ne 2 -or
    -not $workflowText.Contains('git check-ref-format "refs/heads/$($env:RELEASE_REF_NAME)"')) {
    Fail-ReleaseWorkflowContract "release tag and branch inputs must be validated with git check-ref-format before use."
}
if ((Get-SnippetCount -Text $workflowText -Snippet 'git rev-parse --verify --end-of-options "refs/tags/${previousTag}^{commit}"') -ne 2) {
    Fail-ReleaseWorkflowContract "previous_tag must resolve through the exact refs/tags namespace before use."
}

$copilotPack = 'npm pack "$expectedCopilotPackage@$expectedCopilotVersion" --ignore-scripts --json'
$copilotPrepare = '$copilotExecutableItem.DirectoryName | Add-Content -LiteralPath $env:GITHUB_PATH'
$copilotIntegrity = 'sha512-8Mo9y3/8CVU2w35WqwSiRMTGH1kKHR3URPSJYF4J4OG8L7NOEy2fafXR9Tuq3H21Srg3OzFkl/A+Taunqz9KcA=='
$innoInstall = 'choco install innosetup --version 6.7.1 --allow-downgrade --yes --no-progress'
$copilotPackIndex = $buildJob.IndexOf($copilotPack, [System.StringComparison]::Ordinal)
$copilotPrepareIndex = $buildJob.IndexOf($copilotPrepare, [System.StringComparison]::Ordinal)
$innoInstallIndex = $buildJob.IndexOf($innoInstall, [System.StringComparison]::Ordinal)
$vcpkgSetupIndex = $buildJob.IndexOf('./scripts/ci/setup-vcpkg.ps1 -Install', [System.StringComparison]::Ordinal)
$packageCredentialIndex = $buildJob.IndexOf('COPILOT_AUTH_TOKEN: ${{ secrets.COPILOT_GITHUB_TOKEN || secrets.RELEASE_BOT_TOKEN }}', [System.StringComparison]::Ordinal)
$packageGhTokenIndex = $buildJob.IndexOf('GH_TOKEN: ${{ github.token }}', [System.StringComparison]::Ordinal)
$releaseToolInstallIndexes = @($copilotPrepareIndex, $innoInstallIndex)
if ($releaseToolInstallIndexes -contains -1) {
    Fail-ReleaseWorkflowContract "release tools must be installed at the reviewed pinned versions."
}
foreach ($runtimeContract in @(
    'node-version: 22.23.2',
    '"$(node --version)".Trim() -cne "v22.23.2"',
    'dotnet-version: 8.0.423',
    '"$(dotnet --version)".Trim() -cne "8.0.423"',
    '$expectedCopilotPackage = "@github/copilot-win32-x64"',
    $copilotIntegrity,
    '$expectedCopilotExecutableSize = 156874528',
    '$actualCopilotIntegrity = "sha512-$([Convert]::ToBase64String($sha512.ComputeHash($copilotStream)))"',
    '$archiveEntries = @(& tar -tf $copilotTarball.FullName)',
    '$segments -contains ''..''',
    '$copilotExecutableEntries -ne 1',
    '& tar -xf $copilotTarball.FullName -C $copilotExtractDirectory',
    '$copilotExecutable = Join-Path $copilotExtractDirectory "package\copilot.exe"',
    '$copilotExecutableStream.ReadByte() -ne 0x4d',
    '& $copilotExecutableItem.FullName --help',
    '$compilerVersion -cnotmatch "^6\.7\.1(?:\.0)?$"',
    'INNO_SETUP_COMPILER: ${{ steps.inno.outputs.compiler_path }}'
)) {
    if (-not $buildJob.Contains($runtimeContract)) {
        Fail-ReleaseWorkflowContract "release runtime verification is missing '$runtimeContract'."
    }
}
# Installing the reviewed SDK does not select it: dotnet resolves to the newest SDK on the
# runner image unless global.json pins one, so the pin and the workflow must agree exactly.
$dotnetPin = [regex]::Match($buildJob, '(?m)^\s*dotnet-version:\s*(?<version>\d+\.\d+\.\d+)\s*$')
if (-not $dotnetPin.Success) {
    Fail-ReleaseWorkflowContract "release build must install an exact .NET SDK version."
}
$reviewedDotnetVersion = $dotnetPin.Groups['version'].Value
$globalJsonPath = Join-Path $repoRoot "global.json"
if (-not (Test-Path -LiteralPath $globalJsonPath -PathType Leaf)) {
    Fail-ReleaseWorkflowContract "global.json must pin the reviewed .NET SDK, or dotnet selects whichever SDK the runner image ships."
}
$sdkPin = (Get-Content -LiteralPath $globalJsonPath -Raw | ConvertFrom-Json).PSObject.Properties['sdk']
if (-not $sdkPin) {
    Fail-ReleaseWorkflowContract "global.json is missing its sdk block."
}
$sdkVersionPin = $sdkPin.Value.PSObject.Properties['version']
$sdkRollForwardPin = $sdkPin.Value.PSObject.Properties['rollForward']
if (-not $sdkVersionPin -or $sdkVersionPin.Value -cne $reviewedDotnetVersion) {
    Fail-ReleaseWorkflowContract "global.json must pin sdk.version to the reviewed $reviewedDotnetVersion that the workflow installs."
}
if (-not $sdkRollForwardPin -or $sdkRollForwardPin.Value -cne "disable") {
    Fail-ReleaseWorkflowContract "global.json must set sdk.rollForward to disable so a newer SDK on the runner image cannot be selected."
}
if ($copilotPackIndex -lt 0 -or $copilotPackIndex -ge $copilotPrepareIndex -or
    (Get-SnippetCount -Text $buildJob -Snippet $copilotPack) -ne 1 -or
    (Get-SnippetCount -Text $buildJob -Snippet $copilotPrepare) -ne 1 -or
    (Get-SnippetCount -Text $buildJob -Snippet 'choco install innosetup') -ne 1 -or
    [regex]::IsMatch($buildJob, '(?m)^\s*npm\s+(?:view|install)\s+')) {
    Fail-ReleaseWorkflowContract "Copilot must be packed once, hash-verified, and run directly from the reviewed platform tarball without npm dependency resolution."
}
if ($vcpkgSetupIndex -lt 0 -or
    $buildJob -match '(?i)git\s+(clone|fetch)[^\r\n]*microsoft/vcpkg|origin\W+master') {
    Fail-ReleaseWorkflowContract "release dependencies must use the manifest-pinned vcpkg setup entrypoint."
}
$lastReleaseToolInstallIndex = ($releaseToolInstallIndexes | Measure-Object -Maximum).Maximum
if ($packageCredentialIndex -le $lastReleaseToolInstallIndex -or $packageGhTokenIndex -le $lastReleaseToolInstallIndex) {
    Fail-ReleaseWorkflowContract "build credentials must be scoped to packaging after release-tool installation."
}

$buildVerifyIndex = $buildJob.IndexOf($buildCheckoutComparison, [System.StringComparison]::Ordinal)
$packageStepIndex = $buildJob.IndexOf('      - name: Build Release Package', [System.StringComparison]::Ordinal)
$packageInvocationIndex = $buildJob.IndexOf('.\scripts\release.ps1 @parameters', [System.StringComparison]::Ordinal)
if ($buildVerifyIndex -lt 0 -or $packageStepIndex -lt 0 -or $packageInvocationIndex -lt 0) {
    Fail-ReleaseWorkflowContract "build verification and package invocation must be present."
}
foreach ($gate in @($toolingContract, $regressionCorpus, $dependencyInventory, $hostTests, $updaterTests, $strictBuild)) {
    $gateIndex = $buildJob.IndexOf($gate, [System.StringComparison]::Ordinal)
    if ($gateIndex -le $buildVerifyIndex -or $gateIndex -ge $packageStepIndex) {
        Fail-ReleaseWorkflowContract "release gate '$gate' must run after checkout verification and before packaging."
    }
}
$strictBuildIndex = $buildJob.IndexOf($strictBuild, [System.StringComparison]::Ordinal)
$buildReceiptIndex = $buildJob.IndexOf('      - name: Record Strict Build Receipt', [System.StringComparison]::Ordinal)
if ($buildReceiptIndex -le $strictBuildIndex -or $buildReceiptIndex -ge $packageStepIndex) {
    Fail-ReleaseWorkflowContract "the commit-bound DLL build receipt must be written after the strict build and before packaging."
}
$skipBuildIndex = $buildJob.IndexOf($packageSkipBuild, [System.StringComparison]::Ordinal)
if ($skipBuildIndex -le $packageStepIndex -or $skipBuildIndex -ge $packageInvocationIndex) {
    Fail-ReleaseWorkflowContract "packaging must reuse the strict DLL with SkipBuild instead of rebuilding non-strictly."
}
$buildCommandMatches = [regex]::Matches($buildJob, 'build-msbuild\.ps1')
if ($buildCommandMatches.Count -ne 1) {
    Fail-ReleaseWorkflowContract "the release build job must contain exactly one explicit MSBuild command."
}

$provenanceValidationIndex = $publishJob.IndexOf($provenanceSourceCheck, [System.StringComparison]::Ordinal)
$checksumValidationIndex = $publishJob.IndexOf($checksumVerification, [System.StringComparison]::Ordinal)
$publishedMetadataIndex = $publishJob.IndexOf($publishedMetadata, [System.StringComparison]::Ordinal)
$publishArgumentsIndex = $publishJob.IndexOf('$arguments = @("release", "create", $tag)', [System.StringComparison]::Ordinal)
if ($provenanceValidationIndex -lt 0 -or
    $checksumValidationIndex -le $provenanceValidationIndex -or
    $publishedMetadataIndex -le $checksumValidationIndex -or
    $publishArgumentsIndex -le $publishedMetadataIndex) {
    Fail-ReleaseWorkflowContract "publish must validate provenance and checksums before adding them to the GitHub release."
}

$releaseScript = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\release.ps1") -Raw
foreach ($releaseScriptContract in @(
    'function New-ReleaseProvenanceFiles',
    'function Assert-ReleaseBuildReceipt',
    '"/p:MMTreatWarningsAsErrors=true"',
    '-WarningsAsErrors $true',
    '-not [bool]$receipt.WarningsAsErrors',
    'SourceCommit = $SourceCommit',
    'SourceTreeDirty = $SourceTreeDirty',
    'SHA256SUMS.txt',
    'function Assert-RemoteReleaseTag',
    '"--verify-tag",',
    'refs/tags/${TagName}^{commit}',
    '-SkipUpdaterBuild is limited to local unpublished packages',
    '-Prerelease cannot be combined with -Channel stable',
    'Published releases require a receipt bound to the current commit.'
)) {
    if (-not $releaseScript.Contains($releaseScriptContract)) {
        Fail-ReleaseWorkflowContract "release.ps1 is missing provenance contract '$releaseScriptContract'."
    }
}

$versionMutation = $workflowText.IndexOf('git push origin "HEAD:refs/heads/$($env:RELEASE_REF_NAME)"', [System.StringComparison]::Ordinal)
$selfCheckIndex = $workflowText.IndexOf($selfCheck, [System.StringComparison]::Ordinal)
$versionResolutionIndex = $workflowText.IndexOf($versionResolution, [System.StringComparison]::Ordinal)
$versionEmissionIndex = $workflowText.IndexOf($versionEmission, [System.StringComparison]::Ordinal)
$buildCheckoutIndex = $workflowText.IndexOf($buildCheckout, [System.StringComparison]::Ordinal)
$publishResolutionIndex = $workflowText.IndexOf($publishResolution, [System.StringComparison]::Ordinal)
$publishTagAssertionIndex = $workflowText.IndexOf($publishTagAssertion, [System.StringComparison]::Ordinal)
$publishVerifyTagIndex = $workflowText.IndexOf($publishVerifyTag, [System.StringComparison]::Ordinal)
$versionValidationIndex = $targetValidationMatches[0].Index
$publishValidationIndex = $targetValidationMatches[1].Index

if ($versionMutation -lt 0 -or $selfCheckIndex -ge $versionMutation) {
    Fail-ReleaseWorkflowContract "the release workflow must check this contract before version mutation."
}
if ($versionResolutionIndex -le $versionMutation) {
    Fail-ReleaseWorkflowContract "target_sha must be resolved after the optional version commit is pushed."
}
if ($versionValidationIndex -le $versionResolutionIndex -or $versionEmissionIndex -le $versionValidationIndex) {
    Fail-ReleaseWorkflowContract "target_sha must be validated before it is emitted."
}
if ($buildCheckoutIndex -le $versionEmissionIndex) {
    Fail-ReleaseWorkflowContract "the build job must consume the emitted target_sha."
}
if (
    $publishResolutionIndex -le $buildCheckoutIndex -or
    $publishValidationIndex -le $publishResolutionIndex -or
    $publishTagAssertionIndex -le $publishValidationIndex -or
    $publishVerifyTagIndex -le $publishTagAssertionIndex
) {
    Fail-ReleaseWorkflowContract "the publish job must validate target_sha and bind the exact remote tag before passing it to gh release create."
}

Write-Host "Release workflow input-isolation, immutable-action, build-receipt, provenance, strict-gate, and least-privilege contracts are valid."
