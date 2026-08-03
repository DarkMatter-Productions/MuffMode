[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location -LiteralPath $repoRoot

$blockedRules = @(
    [pscustomobject]@{
        Pattern = '(^|/)[^/]+\.(o|d|obj|iobj|dll|exe|lib|exp|pdb|binlog|zip)$'
        Reason = 'generated compiler, binary, symbol, log, or archive output'
    },
    [pscustomobject]@{
        Pattern = '(^|/)build_out\.txt$'
        Reason = 'local build transcript (build_out.txt)'
    }
)

function Find-GeneratedArtifactViolations {
    param(
        [string[]] $Paths
    )

    foreach ($path in $Paths) {
        $normalised = $path -replace '\\', '/'
        foreach ($rule in $blockedRules) {
            if ($normalised -match $rule.Pattern) {
                [pscustomobject]@{
                    Path = $normalised
                    Reason = $rule.Reason
                }
                break
            }
        }
    }
}

$deletedFiles = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
git ls-files --deleted | ForEach-Object {
    [void] $deletedFiles.Add(($_ -replace '\\', '/'))
}

$trackedFiles = @(git ls-files --cached | Where-Object {
    $normalised = $_ -replace '\\', '/'
    -not $deletedFiles.Contains($normalised)
})
$trackedViolations = @(Find-GeneratedArtifactViolations -Paths $trackedFiles)
$ignoredTrackedFiles = @(git ls-files --cached --ignored --exclude-per-directory=.gitignore)
if ($LASTEXITCODE -ne 0) {
    throw "Could not enumerate tracked files that match generated-output ignore rules."
}

if ($ignoredTrackedFiles.Count -gt 0) {
    Write-Error "Files matching generated-output ignore rules are force-tracked:`n$($ignoredTrackedFiles -join "`n")"
    exit 1
}

if ($trackedViolations.Count -gt 0) {
    Write-Error "Generated build artifacts are tracked in the repository:`n$($trackedViolations | Format-Table -AutoSize | Out-String)"
    exit 1
}

if ($env:GITHUB_BASE_REF) {
    $baseRef = "refs/remotes/origin/$($env:GITHUB_BASE_REF)"
    $refSpec = "+refs/heads/$($env:GITHUB_BASE_REF):$baseRef"
    git fetch --no-tags --depth=1 origin $refSpec

    $diffFiles = @(git diff --name-only --diff-filter=ACMRT $baseRef HEAD)
    $diffViolations = @(Find-GeneratedArtifactViolations -Paths $diffFiles)

    if ($diffViolations.Count -gt 0) {
        Write-Error "Generated build artifacts are present in this PR diff:`n$($diffViolations | Format-Table -AutoSize | Out-String)"
        exit 1
    }
}

Write-Host "No generated build artifacts are tracked or present in this PR diff."
