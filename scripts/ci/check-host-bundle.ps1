#requires -Version 7.0

[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$PackageRoot = "packaging/release-assets"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$packageRootPath = if ([System.IO.Path]::IsPathRooted($PackageRoot)) {
    [System.IO.Path]::GetFullPath($PackageRoot)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $PackageRoot))
}
if (-not (Test-Path -LiteralPath $packageRootPath -PathType Container)) {
    throw "Host-bundle package root does not exist: $packageRootPath"
}

$baseq2Path = Join-Path $packageRootPath "rerelease\baseq2"
if (-not (Test-Path -LiteralPath $baseq2Path -PathType Container)) {
    throw "Host-bundle baseq2 directory does not exist: $baseq2Path"
}

$expectedLobbyNames = @(
    "lobby-casual.cfg",
    "lobby-competitive.cfg",
    "lobby-horde.cfg",
    "lobby-party.cfg"
)
$requiredLobbyKeys = @(
    "maxclients",
    "g_maps_pool_file",
    "g_maps_cycle_file",
    "g_maps_random",
    "g_maps_repeat_delay",
    "g_map_pick",
    "g_ranked",
    "g_match_awards",
    "g_allow_voting",
    "g_allow_vote_midgame",
    "g_allow_spec_vote",
    "g_allow_mymap",
    "g_vote_limit",
    "g_vote_flags",
    "g_votable_gametypes",
    "g_votable_factories",
    "g_votable_rulesets",
    "g_gametype_locked",
    "g_gametype",
    "g_factory"
)

# These are policy contracts, not defaults. Keeping the matrix here makes a
# profile change deliberate and reviewable instead of silently weakening it.
$profilePolicies = @{
    "lobby-casual.cfg" = @{
        Capacity = "16"
        Settings = @{
            g_maps_pool_file = "muffmode-map-pool.json"
            g_maps_cycle_file = "muffmode-map-cycle.txt"
            g_maps_random = "1"
            g_maps_repeat_delay = "1800"
            g_map_pick = "15"
            g_ranked = "0"
            g_match_awards = "0"
            g_allow_voting = "1"
            g_allow_vote_midgame = "1"
            g_allow_spec_vote = "0"
            g_allow_mymap = "1"
            g_vote_limit = "3"
            g_vote_flags = "2056"
            g_gametype_locked = "0"
            g_gametype = "1"
            g_factory = "house_dm"
        }
        Gametypes = "ffa duel tdm ctf ca ft strike rr lms horde"
        Rulesets = "q2re mm q2reb"
        Factories = "ffa_classic house_dm duel_classic duel_mm tdm_classic ctf_classic ctf_ironman ca_classic ft_classic strike_classic rr_classic lms_classic horde_classic horde_endless horde_hard"
    }
    "lobby-competitive.cfg" = @{
        Capacity = "16"
        Settings = @{
            g_maps_pool_file = "muffmode-map-pool.json"
            g_maps_cycle_file = "muffmode-map-cycle.txt"
            g_maps_random = "0"
            g_maps_repeat_delay = "1800"
            g_map_pick = "0"
            g_ranked = "1"
            g_match_awards = "10"
            g_allow_voting = "1"
            g_allow_vote_midgame = "0"
            g_allow_spec_vote = "0"
            g_allow_mymap = "0"
            g_vote_limit = "3"
            g_vote_flags = "112697"
            g_gametype_locked = "0"
            g_gametype = "2"
            g_factory = "duel_comp"
        }
        Gametypes = "ffa duel tdm ctf ca ft strike"
        Rulesets = "q2re mm q2reb"
        Factories = "ffa_comp duel_comp tdm_comp ctf_comp ca_comp ft_comp strike_comp"
    }
    "lobby-party.cfg" = @{
        Capacity = "16"
        Settings = @{
            g_maps_pool_file = "muffmode-map-pool.json"
            g_maps_cycle_file = "muffmode-map-cycle.txt"
            g_maps_random = "1"
            g_maps_repeat_delay = "1800"
            g_map_pick = "15"
            g_ranked = "0"
            g_match_awards = "0"
            g_allow_voting = "1"
            g_allow_vote_midgame = "1"
            g_allow_spec_vote = "0"
            g_allow_mymap = "1"
            g_vote_limit = "3"
            g_vote_flags = "2056"
            g_gametype_locked = "0"
            g_gametype = "1"
            g_factory = "instagib_jump"
        }
        Gametypes = "ffa duel tdm ctf ca ft strike rr lms horde"
        Rulesets = "q2re mm q3a q2reb q qc"
        Factories = "ffa_rockets ffa_rockets_air instagib instagib_jump instagib_frenzy nadefest vampffa frenzyffa quadffa instaduel vampduel instatdm vamptdm frenzytdm quadtdm nadetdm instactf vampctf frenzyctf nadectf instaca vampca frenzyca nadeca instaft vampft frenzyft instastrike vampstrike instarr vamprr frenzyrr instalms vamplms vamphorde"
    }
    "lobby-horde.cfg" = @{
        Capacity = "8"
        Settings = @{
            g_maps_pool_file = "muffmode-map-pool.json"
            g_maps_cycle_file = "muffmode-map-cycle.txt"
            g_maps_random = "1"
            g_maps_repeat_delay = "1800"
            g_map_pick = "0"
            g_ranked = "0"
            g_match_awards = "0"
            g_allow_voting = "1"
            g_allow_vote_midgame = "0"
            g_allow_spec_vote = "0"
            g_allow_mymap = "0"
            g_vote_limit = "3"
            g_vote_flags = "112697"
            g_gametype_locked = "0"
            g_gametype = "10"
            g_factory = "horde_classic"
        }
        Gametypes = "horde"
        Rulesets = "q2reb"
        Factories = "horde_classic horde_endless horde_hard vamphorde"
    }
}

$script:strictUtf8 = [System.Text.UTF8Encoding]::new($false, $true)

function Read-StrictUtf8File {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Description,

        [long]$MaximumBytes = 4MB
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing $Description`: $Path"
    }

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -eq 0 -or $bytes.Length -gt $MaximumBytes) {
        throw "$Description must be 1..$MaximumBytes bytes: $Path"
    }
    if (
        $bytes.Length -ge 3 -and
        $bytes[0] -eq 0xef -and
        $bytes[1] -eq 0xbb -and
        $bytes[2] -eq 0xbf
    ) {
        throw "$Description must be UTF-8 without a byte-order mark: $Path"
    }

    try {
        $text = $script:strictUtf8.GetString($bytes)
    }
    catch {
        throw "$Description is not strict UTF-8: $Path"
    }
    if ($text.IndexOf([char]0) -ge 0) {
        throw "$Description contains a NUL byte: $Path"
    }
    return $text
}

function Get-LineTokens {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Line,

        [Parameter(Mandatory = $true)]
        [string]$Context
    )

    $tokens = [System.Collections.Generic.List[object]]::new()
    $index = 0
    while ($index -lt $Line.Length) {
        while ($index -lt $Line.Length -and [char]::IsWhiteSpace($Line[$index])) {
            $index++
        }
        if ($index -ge $Line.Length) {
            break
        }
        if ($Line[$index] -eq '/' -and $index + 1 -lt $Line.Length -and $Line[$index + 1] -eq '/') {
            break
        }

        if ($Line[$index] -eq '"') {
            $index++
            $start = $index
            while ($index -lt $Line.Length -and $Line[$index] -ne '"') {
                $codePoint = [int]$Line[$index]
                if ($codePoint -lt 0x20 -and -not [char]::IsWhiteSpace($Line[$index])) {
                    throw "$Context contains a control character."
                }
                $index++
            }
            if ($index -ge $Line.Length) {
                throw "$Context contains an unterminated quoted value."
            }
            $value = $Line.Substring($start, $index - $start)
            $index++
            if ($index -lt $Line.Length -and -not [char]::IsWhiteSpace($Line[$index])) {
                throw "$Context attaches text directly after a quoted value."
            }
            $tokens.Add([pscustomobject]@{ Text = $value; Quoted = $true })
            continue
        }

        $start = $index
        while ($index -lt $Line.Length -and -not [char]::IsWhiteSpace($Line[$index])) {
            if ($Line[$index] -eq '"') {
                throw "$Context contains a quote inside an unquoted token."
            }
            if ($Line[$index] -eq '/' -and $index + 1 -lt $Line.Length -and $Line[$index + 1] -eq '/') {
                break
            }
            $index++
        }
        if ($index -gt $start) {
            $tokens.Add([pscustomobject]@{
                Text = $Line.Substring($start, $index - $start)
                Quoted = $false
            })
        }
        if ($index -lt $Line.Length -and $Line[$index] -eq '/') {
            break
        }
    }
    return $tokens.ToArray()
}

function Get-RegisteredCvars {
    param([Parameter(Mandatory = $true)][string]$SourceRoot)

    $registered = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    $sourceFiles = Get-ChildItem -LiteralPath $SourceRoot -Recurse -File |
        Where-Object { $_.Extension -in @(".cpp", ".h") }
    foreach ($file in $sourceFiles) {
        $source = [System.IO.File]::ReadAllText($file.FullName)
        foreach ($match in [regex]::Matches(
            $source,
            '\bgi\.cvar\s*\(\s*"(?<name>[A-Za-z][A-Za-z0-9_]*)"'
        )) {
            [void]$registered.Add($match.Groups["name"].Value)
        }
    }
    if ($registered.Count -lt 100) {
        throw "Source cvar registration scan found only $($registered.Count) names."
    }
    return $registered
}

function Read-Config {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][bool]$IsLobby,
        [Parameter(Mandatory = $true)]
        [System.Collections.Generic.HashSet[string]]$RegisteredCvars
    )

    $name = [System.IO.Path]::GetFileName($Path)
    $text = Read-StrictUtf8File -Path $Path -Description $name -MaximumBytes 256KB
    $sets = [System.Collections.Generic.Dictionary[string,string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    $commands = [System.Collections.Generic.List[object]]::new()
    $execCount = 0
    $capacityCount = 0
    $hostnameCount = 0
    $kexCapacity = $null
    $lines = [regex]::Split($text, '\r?\n')

    for ($lineIndex = 0; $lineIndex -lt $lines.Length; $lineIndex++) {
        $context = "$name`:$($lineIndex + 1)"
        $tokens = @(Get-LineTokens -Line $lines[$lineIndex] -Context $context)
        if ($tokens.Count -eq 0) {
            continue
        }
        $command = $tokens[0].Text.ToLowerInvariant()
        $commands.Add([pscustomobject]@{ Name = $command; Line = $lineIndex + 1 })

        switch ($command) {
            "set" {
                if ($tokens.Count -ne 3 -or $tokens[1].Quoted -or -not $tokens[2].Quoted) {
                    throw "$context must use: set <registered-cvar> `"<value>`"."
                }
                $key = $tokens[1].Text
                if ($key -cne $key.ToLowerInvariant() -or $key -cnotmatch '^[a-z][a-z0-9_]*$') {
                    throw "$context has a non-canonical cvar name '$key'."
                }
                if (-not $RegisteredCvars.Contains($key)) {
                    throw "$context sets unregistered cvar '$key'."
                }
                if ($tokens[2].Text.IndexOf(';') -ge 0) {
                    throw "$context contains a command separator in a cvar value."
                }
                if (-not $sets.TryAdd($key, $tokens[2].Text)) {
                    throw "$name sets '$key' more than once."
                }
                if ($key -ceq "maxclients") {
                    $capacityCount++
                }
            }
            "exec" {
                if (-not $IsLobby -or $tokens.Count -ne 2 -or $tokens[1].Quoted -or
                    $tokens[1].Text -cne "server-base.cfg") {
                    throw "$context has an unsupported exec command."
                }
                $execCount++
            }
            "kexmultiplayer" {
                if ($tokens.Count -ne 3 -or $tokens[1].Quoted -or -not $tokens[2].Quoted -or
                    $tokens[1].Text -cne "maxplayers" -or $tokens[2].Text -cnotmatch '^[0-9]+$') {
                    throw "$context must use: kexmultiplayer maxplayers `"<count>`"."
                }
                $capacityCount++
                if ($null -ne $kexCapacity) {
                    throw "$name contains duplicate KEX capacity directives."
                }
                $kexCapacity = $tokens[2].Text
            }
            "hostname" {
                if ($IsLobby -or $tokens.Count -ne 2 -or -not $tokens[1].Quoted) {
                    throw "$context has an unsupported hostname command."
                }
                if (-not $RegisteredCvars.Contains("hostname")) {
                    throw "hostname is no longer registered by the game module."
                }
                $hostnameCount++
                if ($hostnameCount -gt 1) {
                    throw "$name contains duplicate hostname directives."
                }
            }
            default {
                throw "$context uses unsupported config command '$command'."
            }
        }
    }

    if ($commands.Count -eq 0) {
        throw "$name has no active commands."
    }
    if ($IsLobby) {
        if ($execCount -ne 1 -or $commands[0].Name -cne "exec") {
            throw "$name must have exactly one leading 'exec server-base.cfg'."
        }
    }
    elseif ($execCount -ne 0) {
        throw "$name must not exec another config."
    }
    if ($capacityCount -ne 2 -or -not $sets.ContainsKey("maxclients") -or $null -eq $kexCapacity) {
        throw "$name must contain exactly one maxclients set and one KEX maxplayers directive."
    }

    return [pscustomobject]@{
        Name = $name
        Path = $Path
        Sets = $sets
        KexCapacity = [string]$kexCapacity
        CommandCount = $commands.Count
    }
}

function Get-GametypeModel {
    param([Parameter(Mandatory = $true)][string]$Path)

    $source = [System.IO.File]::ReadAllText($Path)
    $enumMatch = [regex]::Match($source, '(?s)enum\s+gametype_t\s*:\s*int\s*\{(?<body>.*?)\};')
    if (-not $enumMatch.Success) {
        throw "Could not parse gametype_t from $Path."
    }
    $enumBody = [regex]::Replace($enumMatch.Groups["body"].Value, '(?m)//.*$', '')
    $enumValues = [System.Collections.Generic.Dictionary[string,int]]::new(
        [System.StringComparer]::Ordinal
    )
    $nextValue = 0
    foreach ($part in $enumBody.Split(',')) {
        $entry = $part.Trim()
        if ($entry.Length -eq 0) { continue }
        if ($entry -notmatch '^(?<name>GT_[A-Z0-9_]+)(?:\s*=\s*(?<value>[0-9]+))?$') {
            throw "Unrecognized gametype enum entry '$entry' in $Path."
        }
        if ($Matches["value"]) { $nextValue = [int]$Matches["value"] }
        $enumValues.Add($Matches["name"], $nextValue)
        $nextValue++
    }

    $descriptorPattern = '(?s)\{\s*(?<id>GT_[A-Z0-9_]+)\s*,\s*"(?<token>[a-z0-9]+)"\s*,.*?gametype_avail_t::(?<availability>Enabled|Disabled|Removed)\s*\}'
    $descriptors = @([regex]::Matches($source, $descriptorPattern))
    if ($descriptors.Count -lt 10) {
        throw "Could not parse the gametype descriptor table from $Path."
    }
    $byToken = [System.Collections.Generic.Dictionary[string,object]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    $byValue = [System.Collections.Generic.Dictionary[int,object]]::new()
    foreach ($match in $descriptors) {
        $enumId = $match.Groups["id"].Value
        if (-not $enumValues.ContainsKey($enumId)) {
            throw "Gametype table references unknown enum '$enumId'."
        }
        $item = [pscustomobject]@{
            Enum = $enumId
            Token = $match.Groups["token"].Value
            Value = $enumValues[$enumId]
            Availability = $match.Groups["availability"].Value
        }
        $byToken.Add($item.Token, $item)
        $byValue.Add($item.Value, $item)
    }
    return [pscustomobject]@{ ByToken = $byToken; ByValue = $byValue }
}

function Get-RulesetTokens {
    param([Parameter(Mandatory = $true)][string]$Path)

    $source = [System.IO.File]::ReadAllText($Path)
    $match = [regex]::Match(
        $source,
        '(?s)constexpr\s+const\s+char\s*\*rs_short_name\s*\[[^]]+\]\s*=\s*\{(?<body>.*?)\};'
    )
    if (-not $match.Success) {
        throw "Could not parse rs_short_name from $Path."
    }
    $tokens = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    foreach ($nameMatch in [regex]::Matches($match.Groups["body"].Value, '"(?<name>[a-z0-9]*)"')) {
        $name = $nameMatch.Groups["name"].Value
        if ($name.Length -gt 0) { [void]$tokens.Add($name) }
    }
    if ($tokens.Count -lt 3) {
        throw "Ruleset table yielded too few playable tokens."
    }
    return $tokens
}

function Read-Factories {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]
        [System.Collections.Generic.HashSet[string]]$RegisteredCvars
    )

    $text = Read-StrictUtf8File -Path $Path -Description "factories.cfg" -MaximumBytes 1MB
    $factories = [System.Collections.Generic.Dictionary[string,object]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    $current = $null
    $lines = [regex]::Split($text, '\r?\n')
    for ($lineIndex = 0; $lineIndex -lt $lines.Length; $lineIndex++) {
        $context = "factories.cfg`:$($lineIndex + 1)"
        $tokens = @(Get-LineTokens -Line $lines[$lineIndex] -Context $context)
        if ($tokens.Count -eq 0) { continue }
        $keyword = $tokens[0].Text.ToLowerInvariant()

        if ($null -eq $current) {
            if ($keyword -cne "factory" -or $tokens.Count -ne 3 -or $tokens[2].Text -cne "{") {
                throw "$context must begin a 'factory <id> {' block."
            }
            $id = $tokens[1].Text
            if ($id -cne $id.ToLowerInvariant() -or $id -cnotmatch '^[a-z0-9_-]{1,31}$') {
                throw "$context has invalid factory id '$id'."
            }
            if ($factories.ContainsKey($id)) {
                throw "factories.cfg defines '$id' more than once."
            }
            $current = [pscustomobject]@{
                Id = $id
                Line = $lineIndex + 1
                Base = $null
                Ruleset = $null
                Inherit = $null
                MinPlayers = $null
                MaxPlayers = $null
                HasTitle = $false
                Resolved = $false
                ResolvedBase = $null
                ResolvedRuleset = $null
                ResolvedMinPlayers = $null
                ResolvedMaxPlayers = $null
                SetNames = [System.Collections.Generic.HashSet[string]]::new(
                    [System.StringComparer]::OrdinalIgnoreCase
                )
            }
            continue
        }

        if ($keyword -ceq "}") {
            if ($tokens.Count -ne 1 -or -not $current.HasTitle) {
                throw "$context closes a malformed or untitled factory '$($current.Id)'."
            }
            $factories.Add($current.Id, $current)
            $current = $null
            continue
        }

        switch ($keyword) {
            "title" {
                if ($tokens.Count -lt 2 -or $current.HasTitle) {
                    throw "$context has an empty or duplicate title."
                }
                $current.HasTitle = $true
            }
            { $_ -in @("desc", "description") } {
                if ($tokens.Count -lt 2) { throw "$context has an empty description." }
            }
            "base" {
                if ($tokens.Count -ne 2 -or $null -ne $current.Base) {
                    throw "$context has an invalid or duplicate base directive."
                }
                $current.Base = $tokens[1].Text.ToLowerInvariant()
            }
            "basegt" {
                if ($tokens.Count -ne 2 -or $null -ne $current.Base) {
                    throw "$context has an invalid or duplicate base directive."
                }
                $current.Base = $tokens[1].Text.ToLowerInvariant()
            }
            "ruleset" {
                if ($tokens.Count -ne 2 -or $null -ne $current.Ruleset) {
                    throw "$context has an invalid or duplicate ruleset directive."
                }
                $current.Ruleset = $tokens[1].Text.ToLowerInvariant()
            }
            "inherit" {
                if ($tokens.Count -ne 2 -or $null -ne $current.Inherit) {
                    throw "$context has an invalid or duplicate inherit directive."
                }
                $current.Inherit = $tokens[1].Text.ToLowerInvariant()
            }
            "players" {
                if ($tokens.Count -ne 3 -or $null -ne $current.MinPlayers -or
                    $tokens[1].Text -cnotmatch '^[0-9]+$' -or $tokens[2].Text -cnotmatch '^[0-9]+$') {
                    throw "$context has an invalid or duplicate players directive."
                }
                $current.MinPlayers = [int]$tokens[1].Text
                $current.MaxPlayers = [int]$tokens[2].Text
                if ($current.MinPlayers -lt 1 -or $current.MaxPlayers -lt $current.MinPlayers -or
                    $current.MaxPlayers -gt 128) {
                    throw "$context has an unsafe player range."
                }
            }
            { $_ -in @("maps", "mappool") } {
                if ($tokens.Count -lt 2) { throw "$context has an empty map directive." }
            }
            "rotation" {
                if ($tokens.Count -ne 2 -or $tokens[1].Text -cnotin @(
                    "sequential", "shuffle-on-wrap", "shuffle-per-gametype"
                )) {
                    throw "$context has an unsupported rotation."
                }
            }
            "set" {
                if ($tokens.Count -lt 3 -or $tokens[1].Text -cnotmatch '^[a-z][a-z0-9_]*$') {
                    throw "$context has a malformed set directive."
                }
                if (-not $RegisteredCvars.Contains($tokens[1].Text)) {
                    throw "$context sets unregistered cvar '$($tokens[1].Text)'."
                }
                if (-not $current.SetNames.Add($tokens[1].Text)) {
                    throw "$context sets '$($tokens[1].Text)' more than once in factory '$($current.Id)'."
                }
            }
            default {
                throw "$context uses unsupported factory directive '$keyword'."
            }
        }
    }
    if ($null -ne $current) {
        throw "Factory '$($current.Id)' is missing its closing brace."
    }
    if ($factories.Count -lt 20) {
        throw "factories.cfg contains unexpectedly few factories ($($factories.Count))."
    }
    return $factories
}

function Resolve-Factory {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)]
        [System.Collections.Generic.Dictionary[string,object]]$Factories,
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$Stack
    )

    if (-not $Factories.ContainsKey($Id)) {
        throw "Factory '$Id' is not defined by factories.cfg."
    }
    $factory = $Factories[$Id]
    if ($factory.Resolved) { return $factory }
    if (-not $Stack.Add($Id)) {
        throw "Factory inheritance cycle reaches '$Id'."
    }

    $parent = $null
    if ($null -ne $factory.Inherit) {
        $parent = Resolve-Factory -Id $factory.Inherit -Factories $Factories -Stack $Stack
    }
    $factory.ResolvedBase = if ($null -ne $factory.Base) { $factory.Base } elseif ($null -ne $parent) { $parent.ResolvedBase } else { $null }
    $factory.ResolvedRuleset = if ($null -ne $factory.Ruleset) { $factory.Ruleset } elseif ($null -ne $parent) { $parent.ResolvedRuleset } else { $null }
    $factory.ResolvedMinPlayers = if ($null -ne $factory.MinPlayers) { $factory.MinPlayers } elseif ($null -ne $parent) { $parent.ResolvedMinPlayers } else { $null }
    $factory.ResolvedMaxPlayers = if ($null -ne $factory.MaxPlayers) { $factory.MaxPlayers } elseif ($null -ne $parent) { $parent.ResolvedMaxPlayers } else { $null }
    $factory.Resolved = $true
    [void]$Stack.Remove($Id)
    return $factory
}

function Get-UniqueTokens {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value,
        [Parameter(Mandatory = $true)][string]$Context
    )

    if ([string]::IsNullOrWhiteSpace($Value)) { return @() }
    $seen = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    $result = [System.Collections.Generic.List[string]]::new()
    foreach ($token in [regex]::Split($Value.Trim(), '\s+')) {
        if ($token -cne $token.ToLowerInvariant() -or $token -cnotmatch '^[a-z0-9_-]+$') {
            throw "$Context contains invalid token '$token'."
        }
        if (-not $seen.Add($token)) {
            throw "$Context contains duplicate token '$token'."
        }
        $result.Add($token)
    }
    return $result.ToArray()
}

function Assert-TokenSetEqual {
    param(
        [Parameter(Mandatory = $true)][string[]]$Expected,
        [Parameter(Mandatory = $true)][string[]]$Actual,
        [Parameter(Mandatory = $true)][string]$Context
    )

    $expectedSet = [System.Collections.Generic.HashSet[string]]::new(
        $Expected,
        [System.StringComparer]::OrdinalIgnoreCase
    )
    $actualSet = [System.Collections.Generic.HashSet[string]]::new(
        $Actual,
        [System.StringComparer]::OrdinalIgnoreCase
    )
    if (-not $expectedSet.SetEquals($actualSet)) {
        $missing = @($expectedSet | Where-Object { -not $actualSet.Contains($_) } | Sort-Object)
        $extra = @($actualSet | Where-Object { -not $expectedSet.Contains($_) } | Sort-Object)
        throw "$Context differs from policy (missing: $($missing -join ', '); extra: $($extra -join ', '))."
    }
}

function Assert-SafeLeafReference {
    param(
        [Parameter(Mandatory = $true)][string]$Value,
        [Parameter(Mandatory = $true)][string]$ExpectedExtension,
        [Parameter(Mandatory = $true)][string]$Context
    )

    if (
        $Value.Length -gt 64 -or
        $Value -cne $Value.ToLowerInvariant() -or
        $Value -cnotmatch '^[a-z0-9][a-z0-9._-]*$' -or
        $Value.Contains("..") -or
        [System.IO.Path]::GetFileName($Value) -cne $Value -or
        [System.IO.Path]::GetExtension($Value) -cne $ExpectedExtension
    ) {
        throw "$Context is not a safe $ExpectedExtension leaf filename: '$Value'."
    }
    $path = Join-Path $baseq2Path $Value
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "$Context references missing file '$Value'."
    }
    return $path
}

function Read-MapBundle {
    param([Parameter(Mandatory = $true)][string]$PoolPath,
          [Parameter(Mandatory = $true)][string]$CyclePath)

    $poolText = Read-StrictUtf8File -Path $PoolPath -Description "map pool" -MaximumBytes 4MB
    try {
        $document = [System.Text.Json.JsonDocument]::Parse($poolText)
    }
    catch {
        throw "Map pool is not strict JSON: $PoolPath"
    }
    try {
        if ($document.RootElement.ValueKind -ne [System.Text.Json.JsonValueKind]::Object) {
            throw "Map pool root must be an object."
        }
        $maps = $document.RootElement.GetProperty("maps")
        if ($maps.ValueKind -ne [System.Text.Json.JsonValueKind]::Array) {
            throw "Map pool 'maps' must be an array."
        }
        $poolIds = [System.Collections.Generic.HashSet[string]]::new(
            [System.StringComparer]::OrdinalIgnoreCase
        )
        foreach ($map in $maps.EnumerateArray()) {
            $id = $map.GetProperty("bsp").GetString()
            if ([string]::IsNullOrWhiteSpace($id) -or -not $poolIds.Add($id)) {
                throw "Map pool contains an empty or duplicate bsp '$id'."
            }
            $arenaProperty = [System.Text.Json.JsonElement]::new()
            if ($map.TryGetProperty("arena", [ref]$arenaProperty) -and
                $arenaProperty.ValueKind -eq [System.Text.Json.JsonValueKind]::True) {
                throw "Map pool enables Arena on '$id', but this bundle ships no Arena-compatible maps."
            }
        }
        if ($poolIds.Count -eq 0) { throw "Map pool contains no maps." }
    }
    finally {
        $document.Dispose()
    }

    $cycleText = Read-StrictUtf8File -Path $CyclePath -Description "map cycle" -MaximumBytes 256KB
    $cycleIds = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    $lines = [regex]::Split($cycleText, '\r?\n')
    for ($lineIndex = 0; $lineIndex -lt $lines.Length; $lineIndex++) {
        foreach ($token in @(Get-LineTokens -Line $lines[$lineIndex] -Context "map cycle:$($lineIndex + 1)")) {
            if ($token.Quoted -or -not $poolIds.Contains($token.Text)) {
                throw "Map cycle names unknown or quoted pool map '$($token.Text)'."
            }
            if (-not $cycleIds.Add($token.Text)) {
                throw "Map cycle repeats '$($token.Text)'."
            }
        }
    }
    if ($cycleIds.Count -eq 0) {
        throw "Map cycle contains no maps."
    }
    if (-not $cycleIds.IsSubsetOf($poolIds)) {
        throw "Map cycle must be a subset of the map pool."
    }
    $expectedPoolOnly = @("base64", "city64", "sewer64", "mm-rail101")
    foreach ($mapId in $expectedPoolOnly) {
        if (-not $poolIds.Contains($mapId) -or $cycleIds.Contains($mapId)) {
            throw "$mapId must remain pool-only under the shipped lobby/factory policy."
        }
    }
    if ($poolIds.Count -ne ($cycleIds.Count + $expectedPoolOnly.Count)) {
        throw "The cycle must contain every pool map except the reviewed pool-only set."
    }
    return [pscustomobject]@{ PoolCount = $poolIds.Count; CycleCount = $cycleIds.Count }
}

# The lobby file set itself is part of the host-facing API.
$actualLobbyNames = @(
    Get-ChildItem -LiteralPath $baseq2Path -File -Filter "lobby-*.cfg" |
        ForEach-Object Name |
        Sort-Object
)
if ($actualLobbyNames.Count -ne $expectedLobbyNames.Count) {
    throw "Expected exactly these lobby configs: $($expectedLobbyNames -join ', '); found: $($actualLobbyNames -join ', ')."
}
for ($index = 0; $index -lt $expectedLobbyNames.Count; $index++) {
    if ($actualLobbyNames[$index] -cne $expectedLobbyNames[$index]) {
        throw "Expected exactly these lobby configs: $($expectedLobbyNames -join ', '); found: $($actualLobbyNames -join ', ')."
    }
}

$registeredCvars = Get-RegisteredCvars -SourceRoot (Join-Path $repoRoot "src\sgame")
$gametypes = Get-GametypeModel -Path (Join-Path $repoRoot "src\sgame\muffmode\mm_gametype_table.h")
$rulesets = Get-RulesetTokens -Path (Join-Path $repoRoot "src\sgame\g_local.h")
$factoriesPath = Join-Path $baseq2Path "factories.cfg"
$factories = Read-Factories -Path $factoriesPath -RegisteredCvars $registeredCvars

foreach ($factoryId in @($factories.Keys)) {
    $factory = Resolve-Factory -Id $factoryId -Factories $factories -Stack (
        [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    )
    if ($null -eq $factory.ResolvedBase -or -not $gametypes.ByToken.ContainsKey($factory.ResolvedBase)) {
        throw "Factory '$factoryId' resolves to unknown gametype '$($factory.ResolvedBase)'."
    }
    if ($gametypes.ByToken[$factory.ResolvedBase].Availability -cne "Enabled") {
        throw "Factory '$factoryId' resolves to unavailable gametype '$($factory.ResolvedBase)'."
    }
    if ($null -ne $factory.ResolvedRuleset -and -not $rulesets.Contains($factory.ResolvedRuleset)) {
        throw "Factory '$factoryId' resolves to unknown ruleset '$($factory.ResolvedRuleset)'."
    }
    if (($null -eq $factory.ResolvedMinPlayers) -ne ($null -eq $factory.ResolvedMaxPlayers)) {
        throw "Factory '$factoryId' resolves only half of a player range."
    }
}

$serverBasePath = Join-Path $baseq2Path "server-base.cfg"
$serverBaseLines = [System.IO.File]::ReadAllLines($serverBasePath)
for ($lineIndex = 0; $lineIndex -lt $serverBaseLines.Count; $lineIndex++) {
    if ($serverBaseLines[$lineIndex] -match '^\s*//.*;') {
        throw "server-base.cfg line $($lineIndex + 1) contains a semicolon in a comment; the engine would execute the text after it."
    }
}
$serverBase = Read-Config -Path $serverBasePath -IsLobby $false -RegisteredCvars $registeredCvars
$criticalServerBaseSettings = @{
    g_factory_file = "factories.cfg"
    g_dm_same_level = "0"
    g_allow_admin = "1"
    g_allow_voting = "1"
    g_allow_vote_midgame = "0"
    g_gametype = "1"
    g_factory = ""
    g_map_pool = ""
    coop = "0"
    deathmatch = "1"
}
foreach ($key in $criticalServerBaseSettings.Keys) {
    if (-not $serverBase.Sets.ContainsKey($key) -or
        $serverBase.Sets[$key] -cne $criticalServerBaseSettings[$key]) {
        throw "server-base.cfg must set '$key' to '$($criticalServerBaseSettings[$key])'."
    }
}
$factoryRegistryPath = Join-Path $baseq2Path $serverBase.Sets["g_factory_file"]
if (-not (Test-Path -LiteralPath $factoryRegistryPath -PathType Leaf)) {
    throw "server-base.cfg references a missing factory registry '$($serverBase.Sets['g_factory_file'])'."
}
$sourceLobbyLimit = [regex]::Match(
    [System.IO.File]::ReadAllText((Join-Path $repoRoot "src\shared\game.h")),
    '(?m)^\s*constexpr\s+size_t\s+MAX_LOBBY_PLAYERS\s*=\s*(?<value>[0-9]+);'
)
if (-not $sourceLobbyLimit.Success) {
    throw "Could not parse MAX_LOBBY_PLAYERS from src/shared/game.h."
}
$maximumLobbyCapacity = $sourceLobbyLimit.Groups["value"].Value
if ($serverBase.Sets["maxclients"] -cne $maximumLobbyCapacity -or
    $serverBase.KexCapacity -cne $maximumLobbyCapacity) {
    throw "server-base.cfg capacity must match MAX_LOBBY_PLAYERS ($maximumLobbyCapacity)."
}

$referencedPoolPath = $null
$referencedCyclePath = $null
foreach ($lobbyName in $expectedLobbyNames) {
    $path = Join-Path $baseq2Path $lobbyName
    $config = Read-Config -Path $path -IsLobby $true -RegisteredCvars $registeredCvars
    $policy = $profilePolicies[$lobbyName]

    foreach ($key in $requiredLobbyKeys) {
        if (-not $config.Sets.ContainsKey($key)) {
            throw "$lobbyName must explicitly set '$key' to prevent inherited lobby state."
        }
    }
    if ($config.Sets.Count -ne $requiredLobbyKeys.Count) {
        $extraKeys = @($config.Sets.Keys | Where-Object { $_ -notin $requiredLobbyKeys })
        throw "$lobbyName must set exactly the reviewed lobby keys; unexpected: $($extraKeys -join ', ')."
    }
    if ($config.Sets["maxclients"] -cne $policy.Capacity -or
        $config.KexCapacity -cne $policy.Capacity) {
        throw "$lobbyName capacity must be $($policy.Capacity) in both directives."
    }
    if ([int]$policy.Capacity -gt [int]$maximumLobbyCapacity) {
        throw "$lobbyName exceeds MAX_LOBBY_PLAYERS ($maximumLobbyCapacity)."
    }
    foreach ($key in $policy.Settings.Keys) {
        if ($config.Sets[$key] -cne $policy.Settings[$key]) {
            throw "$lobbyName policy '$key' must be '$($policy.Settings[$key])', found '$($config.Sets[$key])'."
        }
    }

    $poolPath = Assert-SafeLeafReference -Value $config.Sets["g_maps_pool_file"] -ExpectedExtension ".json" -Context "$lobbyName g_maps_pool_file"
    $cyclePath = Assert-SafeLeafReference -Value $config.Sets["g_maps_cycle_file"] -ExpectedExtension ".txt" -Context "$lobbyName g_maps_cycle_file"
    if ($null -eq $referencedPoolPath) {
        $referencedPoolPath = $poolPath
        $referencedCyclePath = $cyclePath
    }
    elseif ($poolPath -cne $referencedPoolPath -or $cyclePath -cne $referencedCyclePath) {
        throw "$lobbyName must use the shared production pool and cycle."
    }

    $gtTokens = @(Get-UniqueTokens -Value $config.Sets["g_votable_gametypes"] -Context "$lobbyName gametype whitelist")
    $rulesetTokens = @(Get-UniqueTokens -Value $config.Sets["g_votable_rulesets"] -Context "$lobbyName ruleset whitelist")
    $factoryTokens = @(Get-UniqueTokens -Value $config.Sets["g_votable_factories"] -Context "$lobbyName factory whitelist")
    Assert-TokenSetEqual -Expected @(Get-UniqueTokens -Value $policy.Gametypes -Context "$lobbyName expected gametypes") -Actual $gtTokens -Context "$lobbyName gametype whitelist"
    Assert-TokenSetEqual -Expected @(Get-UniqueTokens -Value $policy.Rulesets -Context "$lobbyName expected rulesets") -Actual $rulesetTokens -Context "$lobbyName ruleset whitelist"
    Assert-TokenSetEqual -Expected @(Get-UniqueTokens -Value $policy.Factories -Context "$lobbyName expected factories") -Actual $factoryTokens -Context "$lobbyName factory whitelist"

    $gtSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($token in $gtTokens) { [void]$gtSet.Add($token) }
    $rulesetSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($token in $rulesetTokens) { [void]$rulesetSet.Add($token) }
    $factoryBaseSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($token in $gtTokens) {
        if (-not $gametypes.ByToken.ContainsKey($token) -or
            $gametypes.ByToken[$token].Availability -cne "Enabled") {
            throw "$lobbyName whitelists unknown or unavailable gametype '$token'."
        }
        if ($token -ceq "arena") {
            throw "$lobbyName must not whitelist Arena without compatible maps."
        }
    }
    foreach ($token in $rulesetTokens) {
        if (-not $rulesets.Contains($token)) {
            throw "$lobbyName whitelists unknown ruleset '$token'."
        }
    }
    foreach ($token in $factoryTokens) {
        if ($token.StartsWith('_') -or -not $factories.ContainsKey($token)) {
            throw "$lobbyName whitelists hidden or unknown factory '$token'."
        }
        $factory = $factories[$token]
        if ($factory.ResolvedBase -ceq "arena") {
            throw "$lobbyName whitelists Arena factory '$token' without compatible maps."
        }
        if (-not $gtSet.Contains($factory.ResolvedBase)) {
            throw "$lobbyName factory '$token' has base '$($factory.ResolvedBase)' outside the gametype whitelist."
        }
        if ($null -ne $factory.ResolvedRuleset -and
            -not $rulesetSet.Contains($factory.ResolvedRuleset)) {
            throw "$lobbyName factory '$token' selects ruleset '$($factory.ResolvedRuleset)' outside the ruleset whitelist."
        }
        if ([int]$factory.ResolvedMaxPlayers -gt [int]$policy.Capacity) {
            throw "$lobbyName factory '$token' admits $($factory.ResolvedMaxPlayers) active players but the lobby has only $($policy.Capacity) slots."
        }
        [void]$factoryBaseSet.Add($factory.ResolvedBase)
    }
    if (-not $factoryBaseSet.SetEquals($gtSet)) {
        throw "$lobbyName gametype whitelist must exactly match its votable factory bases."
    }

    $startupValue = 0
    if (-not [int]::TryParse($config.Sets["g_gametype"], [ref]$startupValue) -or
        -not $gametypes.ByValue.ContainsKey($startupValue) -or
        $gametypes.ByValue[$startupValue].Availability -cne "Enabled") {
        throw "$lobbyName has unknown or unavailable startup g_gametype '$($config.Sets["g_gametype"])'."
    }
    $startupFactoryId = $config.Sets["g_factory"]
    if (-not $factories.ContainsKey($startupFactoryId)) {
        throw "$lobbyName starts unknown factory '$startupFactoryId'."
    }
    if ($factories[$startupFactoryId].ResolvedBase -cne $gametypes.ByValue[$startupValue].Token) {
        throw "$lobbyName startup g_gametype does not match factory '$startupFactoryId'."
    }
    if (-not $factoryTokens.Contains($startupFactoryId)) {
        throw "$lobbyName startup factory '$startupFactoryId' is not votable in its own profile."
    }

    $voteFlags = [int]$config.Sets["g_vote_flags"]
    $manualMapsEnabled = $config.Sets["g_allow_mymap"] -ceq "1"
    $directMapVotesEnabled = ($voteFlags -band 1) -eq 0
    if ($manualMapsEnabled -ne $directMapVotesEnabled) {
        throw "$lobbyName must keep MyMap and direct map-vote policy aligned."
    }
    if (($voteFlags -band 8) -eq 0 -or ($voteFlags -band 2048) -eq 0) {
        throw "$lobbyName must disable direct gametype and ruleset votes; factories own both."
    }
    if (($voteFlags -band 131072) -ne 0) {
        throw "$lobbyName disables factory votes despite supplying a factory whitelist."
    }
}

$mapBundle = Read-MapBundle -PoolPath $referencedPoolPath -CyclePath $referencedCyclePath
Write-Host "Host bundle valid: $($expectedLobbyNames.Count) lobby profiles, $($factories.Count) factories, $($mapBundle.PoolCount) pool maps, $($mapBundle.CycleCount) cycle maps."
