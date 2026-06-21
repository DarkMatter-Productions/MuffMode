[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location -LiteralPath $repoRoot

$blockedRules = @(
    [pscustomobject]@{
        Pattern = '(^|/)[^/]+\.(o|d)$'
        Reason = 'native compiler object/dependency files (*.o, *.d)'
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
