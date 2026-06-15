$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot

$redRoverPath = Join-Path $repoRoot "docs-dev\test-assets\red-rover\0.36.04-regressions.json"
if (-not (Test-Path -LiteralPath $redRoverPath -PathType Leaf)) {
    throw "Missing Red Rover regression corpus: $redRoverPath"
}

$redRover = Get-Content -Raw -Path $redRoverPath | ConvertFrom-Json
$requiredScenarioIds = @(
    "frag-warning-lower-bound",
    "scorelimit-uses-individual-rr-score",
    "manual-team-switch-blocked-in-progress",
    "scoreboard-footer-reserve",
    "spectator-join-during-in-progress-match"
)

$scenarioIds = @($redRover.scenarios | ForEach-Object { $_.id })
foreach ($id in $requiredScenarioIds) {
    if ($scenarioIds -notcontains $id) {
        throw "Red Rover regression corpus is missing scenario '$id'."
    }
}

$commandPath = Join-Path $repoRoot "docs-dev\test-assets\command-sequences\phase-three-regression.commands"
if (-not (Test-Path -LiteralPath $commandPath -PathType Leaf)) {
    throw "Missing phase-three command regression corpus: $commandPath"
}

$commandText = Get-Content -Raw -Path $commandPath
foreach ($needle in @("teleport 0 0 64", "spawn monster_soldier spawnflags", "use_index nope", "drop_index nope", "ghost nope", "killbeep nope")) {
    if ($commandText -notmatch [regex]::Escape($needle)) {
        throw "Command regression corpus missing '$needle'."
    }
}

foreach ($seed in @(
    "docs-dev\test-assets\fuzz-corpus\numeric-parsers\valid-and-invalid.txt",
    "docs-dev\test-assets\fuzz-corpus\gt-cfg\maxclients-lines.txt"
)) {
    $path = Join-Path $repoRoot $seed
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing fuzz corpus seed: $path"
    }
}

Write-Host "Regression and fuzz corpus seeds are present and parseable."
