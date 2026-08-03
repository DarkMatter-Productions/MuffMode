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
$duplicateScenarioIds = @($scenarioIds | Group-Object | Where-Object Count -gt 1)
if ($duplicateScenarioIds.Count -ne 0) {
    throw "Red Rover regression corpus contains duplicate scenario IDs: $($duplicateScenarioIds.Name -join ', ')."
}
foreach ($id in $requiredScenarioIds) {
    if ($scenarioIds -notcontains $id) {
        throw "Red Rover regression corpus is missing scenario '$id'."
    }
}

function Get-RequiredScenario {
    param([string]$Id)

    $matches = @($redRover.scenarios | Where-Object id -eq $Id)
    if ($matches.Count -ne 1) {
        throw "Expected exactly one Red Rover scenario '$Id'."
    }
    return $matches[0]
}

# Keep the checked-in fixture's inputs and expected outcomes aligned with the
# host regressions that execute these contracts. A changed expectation must be
# an intentional code-and-test change, not a silently accepted corpus edit.
$scenario = Get-RequiredScenario "frag-warning-lower-bound"
if ($scenario.kind -ne "pure-helper" -or $scenario.input.fraglimit -ne 10 -or
    (@($scenario.input.leaderScores) -join ',') -ne "7,8,9,10,11" -or
    (@($scenario.expected.warningIndexes | ForEach-Object { if ($null -eq $_) { "null" } else { [string]$_ } }) -join ',') -ne "2,1,0,null,null" -or
    $scenario.expected.neverIndexesBelowZero -ne $true) {
    throw "Red Rover frag-warning scenario no longer matches its executable host-test contract."
}

$scenario = Get-RequiredScenario "scorelimit-uses-individual-rr-score"
if ($scenario.kind -ne "pure-helper" -or $scenario.input.teams -ne $true -or
    $scenario.input.redRover -ne $true -or $scenario.expected.useTeamScoreLimit -ne $false) {
    throw "Red Rover score-limit scenario no longer matches its executable host-test contract."
}

$scenario = Get-RequiredScenario "manual-team-switch-blocked-in-progress"
if ($scenario.kind -ne "pure-helper" -or $scenario.input.currentTeam -ne "red" -or
    $scenario.input.desiredTeam -ne "blue" -or $scenario.input.spectatorTeam -ne "spectator" -or
    $scenario.input.redRover -ne $true -or $scenario.input.matchInProgress -ne $true -or
    $scenario.expected.blocked -ne $true -or $scenario.expected.spectatorJoinAllowed -ne $true -or
    $scenario.expected.leaveToSpectatorAllowed -ne $true) {
    throw "Red Rover team-switch scenario no longer matches its executable host-test contract."
}

$scenario = Get-RequiredScenario "scoreboard-footer-reserve"
if ($scenario.kind -ne "pure-helper" -or $scenario.input.maxStringChars -ne 1400 -or
    $scenario.input.warmupFooterReserve -ne 96 -or $scenario.input.intermissionFooterReserve -ne 320 -or
    $scenario.expected.footerSurvivesPlayerRows -ne $true) {
    throw "Red Rover scoreboard-footer scenario no longer matches its executable host-test contract."
}

$scenario = Get-RequiredScenario "spectator-join-during-in-progress-match"
if ($scenario.kind -ne "headless-scenario" -or
    $scenario.expected.joinFromSpectatorAllowed -ne $true -or
    $scenario.expected.manualSideSwitchBlocked -ne $true) {
    throw "Red Rover spectator-join scenario no longer matches its regression contract."
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
    $seedLines = @(Get-Content -LiteralPath $path | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    })
    if ($seedLines.Count -eq 0) {
        throw "Fuzz corpus seed contains no cases: $path"
    }
    if (@($seedLines | Where-Object { $_.Length -gt 4096 }).Count -ne 0) {
        throw "Fuzz corpus seed contains a case above the 4096-byte smoke limit: $path"
    }
}

Write-Host "Regression contracts and fuzz seed inputs are structurally valid."
