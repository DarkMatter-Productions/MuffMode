#requires -Version 7.0

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [string]$Remote = "origin",
    [string]$RemoteBranch,
    [string]$ExpectedRepository = "DarkMatter-Productions/MuffMode",

    [switch]$AllowDirty,
    [switch]$AllowUntracked,
    [switch]$AllowRepositoryMismatch,
    [switch]$AllowNonCiBranch,
    [switch]$SkipFetch,
    [switch]$SkipLocalGates,
    [switch]$SkipVcpkgSetup,
    [switch]$SkipHostTests,
    [switch]$SkipUpdaterTests,
    [switch]$SkipBuild,
    [switch]$IncludeAnalysis,
    [switch]$IncludeSanitizers,
    # Retained for command-line compatibility; analysis findings are always blocking.
    [switch]$TreatOptionalAnalysisAsErrors,
    [switch]$WaitForGitHub,

    [ValidateRange(1, 240)]
    [int]$GitHubTimeoutMinutes = 150,

    [ValidateRange(5, 300)]
    [int]$GitHubPollSeconds = 20
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
Set-Location -LiteralPath $repoRoot

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [switch]$AllowFailure
    )

    $output = @(& git @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw "git $($Arguments -join ' ') failed with exit code $exitCode.`n$($output -join "`n")"
    }

    [pscustomobject]@{
        ExitCode = $exitCode
        Output = $output
    }
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    Write-Host ""
    Write-Host "== $Name =="
    & $Action
}

function Invoke-RepoScript {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    Invoke-Step -Name $Name -Action $Action
}

function ConvertTo-CleanYamlValue {
    param([string]$Value)

    $clean = $Value.Trim().TrimEnd(",")
    $clean = $clean.Trim('"')
    $clean = $clean.Trim("'")
    return $clean.Trim()
}

function ConvertFrom-InlineBranchList {
    param([string]$ListText)

    @($ListText.Split(",") | ForEach-Object {
        ConvertTo-CleanYamlValue $_
    } | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    })
}

function Get-WorkflowName {
    param([string[]]$Lines, [string]$Path)

    foreach ($line in $Lines) {
        if ($line -match '^\s*name\s*:\s*(?<name>.+?)\s*$') {
            return ConvertTo-CleanYamlValue $Matches["name"]
        }
    }

    return [System.IO.Path]::GetFileNameWithoutExtension($Path)
}

function Get-PushBranchesFromWorkflow {
    param([string]$Path)

    $lines = @(Get-Content -LiteralPath $Path)
    $branches = New-Object System.Collections.Generic.List[string]
    $hasPush = $false

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -notmatch '^(?<indent>\s*)push\s*:\s*(?:#.*)?$') {
            continue
        }

        $hasPush = $true
        $pushIndent = $Matches["indent"].Length

        for ($j = $i + 1; $j -lt $lines.Count; $j++) {
            $candidate = $lines[$j]
            if ([string]::IsNullOrWhiteSpace($candidate) -or $candidate.TrimStart().StartsWith("#")) {
                continue
            }

            if ($candidate -notmatch '^(?<indent>\s*)') {
                continue
            }

            $indent = $Matches["indent"].Length
            if ($indent -le $pushIndent) {
                break
            }

            $trimmed = $candidate.Trim()
            if ($trimmed -match '^branches\s*:\s*\[(?<branches>[^\]]*)\]') {
                foreach ($branch in (ConvertFrom-InlineBranchList $Matches["branches"])) {
                    [void]$branches.Add($branch)
                }
                continue
            }

            if ($trimmed -notmatch '^branches\s*:\s*$') {
                continue
            }

            $branchIndent = $indent
            for ($k = $j + 1; $k -lt $lines.Count; $k++) {
                $branchLine = $lines[$k]
                if ([string]::IsNullOrWhiteSpace($branchLine) -or $branchLine.TrimStart().StartsWith("#")) {
                    continue
                }

                if ($branchLine -notmatch '^(?<indent>\s*)') {
                    continue
                }

                $nestedIndent = $Matches["indent"].Length
                if ($nestedIndent -le $branchIndent) {
                    break
                }

                $nestedTrimmed = $branchLine.Trim()
                if ($nestedTrimmed -match '^-\s*(?<branch>.+?)\s*$') {
                    [void]$branches.Add((ConvertTo-CleanYamlValue $Matches["branch"]))
                }
            }
        }
    }

    [pscustomobject]@{
        HasPush = $hasPush
        Branches = @($branches | Sort-Object -Unique)
        Name = Get-WorkflowName -Lines $lines -Path $Path
        FileName = [System.IO.Path]::GetFileName($Path)
        Path = $Path
    }
}

function Get-GitHubPushWorkflows {
    $workflowRoot = Join-Path $repoRoot ".github\workflows"
    if (-not (Test-Path -LiteralPath $workflowRoot -PathType Container)) {
        return @()
    }

    @(Get-ChildItem -LiteralPath $workflowRoot -File | Where-Object {
        $_.Extension -in @(".yml", ".yaml")
    } | ForEach-Object {
        Get-PushBranchesFromWorkflow -Path $_.FullName
    } | Where-Object {
        $_.HasPush
    })
}

function Test-BranchMatchesWorkflow {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Branch,

        [AllowEmptyCollection()]
        [AllowNull()]
        [string[]]$Patterns
    )

    $patternsToCheck = @($Patterns)
    if ($patternsToCheck.Count -eq 0) {
        return $true
    }

    foreach ($pattern in $patternsToCheck) {
        if ($pattern -eq "*" -or $Branch -like $pattern) {
            return $true
        }
    }

    return $false
}

function Get-GitHubRepositorySlug {
    param([string]$Url)

    if ($Url -match 'github\.com[:/](?<slug>[^/]+/[^/]+?)(?:\.git)?/?$') {
        return $Matches["slug"]
    }

    throw "Remote URL is not a GitHub repository URL: $Url"
}

function Get-CurrentBranchName {
    $branch = ((Invoke-Git -Arguments @("branch", "--show-current")).Output -join "`n").Trim()
    if ([string]::IsNullOrWhiteSpace($branch)) {
        throw "Current HEAD is detached. Check out a branch before verifying a push."
    }

    return $branch
}

function Resolve-RemoteBranch {
    param([string]$LocalBranch)

    if (-not [string]::IsNullOrWhiteSpace($RemoteBranch)) {
        return $RemoteBranch
    }

    $upstream = Invoke-Git -Arguments @("rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}") -AllowFailure
    if ($upstream.ExitCode -eq 0) {
        $upstreamName = ($upstream.Output -join "`n").Trim()
        if ($upstreamName -match '^[^/]+/(?<branch>.+)$') {
            return $Matches["branch"]
        }
    }

    return $LocalBranch
}

function Assert-CleanWorktree {
    $status = @((Invoke-Git -Arguments @("status", "--porcelain=v1")).Output)

    if ($AllowUntracked) {
        $status = @($status | Where-Object { -not $_.StartsWith("??") })
    }

    if ($status.Count -eq 0) {
        Write-Host "Working tree is clean."
        return
    }

    $message = "Working tree has uncommitted changes:`n$($status -join "`n")"
    if ($AllowDirty) {
        Write-Warning $message
        return
    }

    throw "$message`nCommit, stash, or pass -AllowDirty when you intentionally want to verify committed HEAD only."
}

function Test-RemoteBranchExists {
    param([string]$Branch)

    if ($SkipFetch) {
        $refCheck = Invoke-Git -Arguments @("show-ref", "--verify", "--quiet", "refs/remotes/$Remote/$Branch") -AllowFailure
        return ($refCheck.ExitCode -eq 0)
    }

    $remoteHeads = Invoke-Git -Arguments @("ls-remote", "--heads", $Remote, $Branch)
    $exists = @($remoteHeads.Output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }).Count -gt 0
    if ($exists) {
        Invoke-Git -Arguments @("fetch", "--no-tags", $Remote, "+refs/heads/$Branch`:refs/remotes/$Remote/$Branch") | Out-Null
    }

    return $exists
}

function Resolve-NewBranchComparisonRef {
    $defaultBranch = ""
    $symbolicHead = Invoke-Git -Arguments @(
        "symbolic-ref", "--quiet", "--short", "refs/remotes/$Remote/HEAD"
    ) -AllowFailure
    if ($symbolicHead.ExitCode -eq 0) {
        $headName = ($symbolicHead.Output -join "`n").Trim()
        $remotePrefix = "$Remote/"
        if ($headName.StartsWith($remotePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $defaultBranch = $headName.Substring($remotePrefix.Length)
        }
    }

    if ([string]::IsNullOrWhiteSpace($defaultBranch)) {
        if ($SkipFetch) {
            throw "The remote default branch is not cached. Rerun without -SkipFetch before verifying a new branch."
        }

        $remoteHead = Invoke-Git -Arguments @("ls-remote", "--symref", $Remote, "HEAD")
        foreach ($line in $remoteHead.Output) {
            if ($line -match '^ref:\s+refs/heads/(?<branch>\S+)\s+HEAD$') {
                $defaultBranch = $Matches["branch"]
                break
            }
        }
    }

    if ([string]::IsNullOrWhiteSpace($defaultBranch)) {
        throw "Could not resolve the default branch for remote '$Remote'."
    }

    $defaultRef = "refs/remotes/$Remote/$defaultBranch"
    $cachedDefault = Invoke-Git -Arguments @(
        "show-ref", "--verify", "--quiet", $defaultRef
    ) -AllowFailure
    if ($cachedDefault.ExitCode -ne 0) {
        if ($SkipFetch) {
            throw "The comparison ref '$defaultRef' is not cached. Rerun without -SkipFetch before verifying a new branch."
        }
        Invoke-Git -Arguments @(
            "fetch", "--no-tags", $Remote,
            "+refs/heads/$defaultBranch`:$defaultRef"
        ) | Out-Null
    }

    $mergeBase = Invoke-Git -Arguments @("merge-base", "HEAD", $defaultRef) -AllowFailure
    $comparisonRef = ($mergeBase.Output -join "`n").Trim()
    if ($mergeBase.ExitCode -ne 0 -or $comparisonRef -notmatch '^[0-9a-fA-F]{40}$') {
        throw "Could not resolve a merge base between HEAD and '$defaultRef'."
    }

    Write-Host "New branch comparison base: $comparisonRef ($defaultRef)."
    return $comparisonRef
}

function Assert-PushTarget {
    param([string]$Branch)

    $remoteExists = Test-RemoteBranchExists -Branch $Branch
    if (-not $remoteExists) {
        Write-Host "Remote branch '$Remote/$Branch' does not exist yet; treating this as a new branch push."
        return [pscustomobject]@{
            RemoteBranchExists = $false
            Ahead = $null
            Behind = $null
        }
    }

    $remoteRef = "refs/remotes/$Remote/$Branch"
    $countsText = ((Invoke-Git -Arguments @("rev-list", "--left-right", "--count", "HEAD...$remoteRef")).Output -join " ").Trim()
    if ($countsText -notmatch '^(?<ahead>\d+)\s+(?<behind>\d+)$') {
        throw "Could not parse ahead/behind counts for $remoteRef`: $countsText"
    }

    $ahead = [int]$Matches["ahead"]
    $behind = [int]$Matches["behind"]
    if ($behind -gt 0) {
        throw "Push would not be fast-forward: $Remote/$Branch has $behind commit(s) not in HEAD. Rebase or merge before pushing."
    }

    if ($ahead -eq 0) {
        Write-Warning "HEAD has no commits ahead of $Remote/$Branch. Local gates will still verify the checked-out commit."
    }
    else {
        Write-Host "HEAD is $ahead commit(s) ahead of $Remote/$Branch and can be pushed fast-forward."
    }

    [pscustomobject]@{
        RemoteBranchExists = $true
        Ahead = $ahead
        Behind = $behind
    }
}

function Assert-GitHubWorkflowCoverage {
    param([string]$Branch)

    $pushWorkflows = @(Get-GitHubPushWorkflows)
    if ($pushWorkflows.Count -eq 0) {
        throw "No GitHub workflows with push triggers were found under .github/workflows."
    }

    $matching = @($pushWorkflows | Where-Object {
        Test-BranchMatchesWorkflow -Branch $Branch -Patterns $_.Branches
    })

    if ($matching.Count -eq 0) {
        $summary = ($pushWorkflows | ForEach-Object {
            $patterns = if ($_.Branches.Count -eq 0) { "*" } else { $_.Branches -join ", " }
            "$($_.FileName): $patterns"
        }) -join "; "

        $message = "No GitHub push workflows match branch '$Branch'. Current push branches: $summary"
        if ($AllowNonCiBranch) {
            Write-Warning $message
            return @()
        }

        throw "$message`nPass -RemoteBranch with the GitHub branch you intend to update, or -AllowNonCiBranch for a branch that is intentionally outside push CI."
    }

    Write-Host "GitHub push workflows for '$Branch': $($matching.FileName -join ', ')"
    return $matching
}

function Invoke-GitHubWait {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Workflows,

        [Parameter(Mandatory = $true)]
        [string]$Repository,

        [Parameter(Mandatory = $true)]
        [string]$Branch,

        [Parameter(Mandatory = $true)]
        [string]$HeadSha
    )

    if ($Workflows.Count -eq 0) {
        throw "No matching push workflows are available to wait for."
    }

    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if (-not $gh) {
        throw "GitHub CLI 'gh' was not found on PATH. Install or authenticate gh, or rerun without -WaitForGitHub."
    }

    $deadline = (Get-Date).AddMinutes($GitHubTimeoutMinutes)
    $pending = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($workflow in $Workflows) {
        [void]$pending.Add($workflow.FileName)
    }

    while ($pending.Count -gt 0 -and (Get-Date) -lt $deadline) {
        foreach ($workflow in @($Workflows | Where-Object { $pending.Contains($_.FileName) })) {
            $jsonOutput = @(& $gh.Source run list `
                --repo $Repository `
                --workflow $workflow.FileName `
                --branch $Branch `
                --commit $HeadSha `
                --event push `
                --limit 1 `
                --json databaseId,headSha,status,conclusion,url,workflowName,createdAt 2>&1)

            if ($LASTEXITCODE -ne 0) {
                throw "gh run list failed for $($workflow.FileName):`n$($jsonOutput -join "`n")"
            }

            $jsonText = ($jsonOutput -join "`n").Trim()
            $runs = @()
            if (-not [string]::IsNullOrWhiteSpace($jsonText) -and $jsonText -ne "[]") {
                $converted = $jsonText | ConvertFrom-Json
                if ($null -ne $converted) {
                    $runs = @($converted)
                }
            }

            if ($runs.Count -eq 0) {
                continue
            }

            $run = $runs[0]
            if ($run.status -ne "completed") {
                Write-Host "$($workflow.FileName) is $($run.status): $($run.url)"
                continue
            }

            if ($run.conclusion -ne "success") {
                throw "$($workflow.FileName) completed with conclusion '$($run.conclusion)': $($run.url)"
            }

            Write-Host "$($workflow.FileName) succeeded: $($run.url)"
            [void]$pending.Remove($workflow.FileName)
        }

        if ($pending.Count -gt 0) {
            Write-Host "Waiting for GitHub Actions: $($pending -join ', ')"
            Start-Sleep -Seconds $GitHubPollSeconds
        }
    }

    if ($pending.Count -gt 0) {
        throw "Timed out after $GitHubTimeoutMinutes minute(s) waiting for GitHub Actions: $($pending -join ', ')"
    }
}

$localBranch = Get-CurrentBranchName
$targetBranch = Resolve-RemoteBranch -LocalBranch $localBranch

Invoke-Step -Name "Check GitHub remote" -Action {
    $remoteUrl = ((Invoke-Git -Arguments @("remote", "get-url", "--push", $Remote)).Output -join "`n").Trim()
    $repoSlug = Get-GitHubRepositorySlug -Url $remoteUrl

    if (-not [string]::IsNullOrWhiteSpace($ExpectedRepository) -and $repoSlug -ine $ExpectedRepository) {
        $message = "Remote '$Remote' points at GitHub repo '$repoSlug', expected '$ExpectedRepository'."
        if ($AllowRepositoryMismatch) {
            Write-Warning $message
        }
        else {
            throw "$message Pass -AllowRepositoryMismatch when verifying a fork intentionally."
        }
    }

    Write-Host "Remote '$Remote' -> $repoSlug"
}

Invoke-Step -Name "Check worktree state" -Action {
    Assert-CleanWorktree
}

$pushTarget = Invoke-Step -Name "Check push target" -Action {
    Write-Host "Local branch '$localBranch' will be verified against '$Remote/$targetBranch'."
    Assert-PushTarget -Branch $targetBranch
}
$changedSince = if ($pushTarget.RemoteBranchExists) {
    "refs/remotes/$Remote/$targetBranch"
}
else {
    Resolve-NewBranchComparisonRef
}

$matchingPushWorkflows = @(Invoke-Step -Name "Check GitHub workflow trigger coverage" -Action {
    Assert-GitHubWorkflowCoverage -Branch $targetBranch
})

if (-not $SkipLocalGates) {
    Invoke-RepoScript -Name "Check release workflow provenance contract" -Action {
        & (Join-Path $repoRoot "scripts\ci\check-release-workflow.ps1")
    }
    Invoke-RepoScript -Name "Check analysis and fuzz tooling contracts" -Action {
        & (Join-Path $repoRoot "scripts\ci\check-tooling-contracts.ps1")
    }
    Invoke-RepoScript -Name "Check generated artifacts" -Action {
        & (Join-Path $repoRoot "scripts\ci\check-generated-artifacts.ps1")
    }
    Invoke-RepoScript -Name "Check map-pool examples" -Action {
        & (Join-Path $repoRoot "scripts\ci\check-map-pool-examples.ps1")
    }
    Invoke-RepoScript -Name "Check changelog ledger" -Action {
        & (Join-Path $repoRoot "scripts\ci\check-changelog.ps1") -ChangedSince $changedSince
    }
    Invoke-RepoScript -Name "Check phase-zero asset seeds" -Action {
        & (Join-Path $repoRoot "scripts\ci\check-test-assets.ps1") -RepoOnly
    }
    Invoke-RepoScript -Name "Check regression corpus" -Action {
        & (Join-Path $repoRoot "scripts\ci\check-regression-corpus.ps1")
    }

    if (-not $SkipHostTests) {
        Invoke-RepoScript -Name "Run host smoke tests" -Action {
            & (Join-Path $repoRoot "scripts\ci\run-host-tests.ps1") -Configuration $Configuration -Platform $Platform
        }
    }
    else {
        Write-Warning "Skipping host tests by request."
    }

    if (-not $SkipUpdaterTests) {
        Invoke-RepoScript -Name "Run updater cleanup tests" -Action {
            & (Join-Path $repoRoot "scripts\ci\run-updater-tests.ps1") -Configuration $Configuration
        }
    }
    else {
        Write-Warning "Skipping updater tests by request."
    }

    if (-not $SkipVcpkgSetup) {
        Invoke-RepoScript -Name "Setup vcpkg" -Action {
            & (Join-Path $repoRoot "scripts\ci\setup-vcpkg.ps1") -Install
        }
    }
    else {
        Write-Warning "Skipping vcpkg setup by request."
    }

    Invoke-RepoScript -Name "Check dependency inventory" -Action {
        & (Join-Path $repoRoot "scripts\ci\check-dependency-inventory.ps1")
    }

    if (-not $SkipBuild) {
        Invoke-RepoScript -Name "Strict release build" -Action {
            & (Join-Path $repoRoot "scripts\ci\build-msbuild.ps1") -Configuration $Configuration -Platform $Platform -TreatWarningsAsErrors -BinaryLog
        }
    }
    else {
        Write-Warning "Skipping strict release build by request."
    }

    if ($IncludeAnalysis) {
        Invoke-RepoScript -Name "Run MSVC /analyze" -Action {
            & (Join-Path $repoRoot "scripts\ci\run-msvc-analyze.ps1") -Configuration $Configuration -Platform $Platform
        }
        Invoke-RepoScript -Name "Export compile commands" -Action {
            & (Join-Path $repoRoot "scripts\ci\export-compile-commands.ps1")
        }

        Invoke-RepoScript -Name "Run clang-tidy on touched files" -Action {
            & (Join-Path $repoRoot "scripts\ci\run-clang-tidy.ps1") -ChangedSince $changedSince -AllowEmpty
        }

        Invoke-RepoScript -Name "Run Cppcheck" -Action {
            & (Join-Path $repoRoot "scripts\ci\run-cppcheck.ps1")
        }
    }

    if ($IncludeSanitizers) {
        Invoke-RepoScript -Name "Run AddressSanitizer build" -Action {
            & (Join-Path $repoRoot "scripts\ci\run-sanitized-build.ps1") -Sanitizer Address
        }
        Invoke-RepoScript -Name "Run UndefinedBehaviorSanitizer build" -Action {
            & (Join-Path $repoRoot "scripts\ci\run-sanitized-build.ps1") -Sanitizer Undefined -AllowUnsupported
        }
    }
}
else {
    Write-Warning "Skipping local gates by request."
}

if ($WaitForGitHub) {
    $headSha = ((Invoke-Git -Arguments @("rev-parse", "HEAD")).Output -join "`n").Trim()
    $remoteUrl = ((Invoke-Git -Arguments @("remote", "get-url", "--push", $Remote)).Output -join "`n").Trim()
    $repoSlug = Get-GitHubRepositorySlug -Url $remoteUrl

    Invoke-Step -Name "Wait for GitHub Actions" -Action {
        Invoke-GitHubWait -Workflows $matchingPushWorkflows -Repository $repoSlug -Branch $targetBranch -HeadSha $headSha
    }
}

Write-Host ""
Write-Host "GitHub push verification passed for HEAD -> $Remote/$targetBranch."
