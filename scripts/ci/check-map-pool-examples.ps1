#requires -Version 7.0

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$poolPath = Join-Path $repoRoot "packaging\release-assets\rerelease\baseq2\muffmode-map-pool.example.json"
$cyclePath = Join-Path $repoRoot "packaging\release-assets\rerelease\baseq2\muffmode-map-cycle.example.txt"
$mapsPath = Join-Path $repoRoot "packaging\release-assets\rerelease\maps"

$expectedMuffModeMapCount = 9
$expectedReferenceMapIds = @(
    "q2dm1", "q2dm2", "q2dm3", "q2dm4", "q2dm5", "q2dm6", "q2dm7", "q2dm8",
    "base64", "city64", "sewer64", "mgdm1",
    "q2ctf1", "q2ctf2", "q2ctf3", "q2ctf4", "q2ctf5",
    "ndctf0", "q2kctf1", "q2kctf2",
    "xdm1", "xdm2", "xdm3", "xdm4", "xdm5", "xdm6", "xdm7",
    "rdm1", "rdm2", "rdm3", "rdm4", "rdm5", "rdm6", "rdm7",
    "rdm8", "rdm9", "rdm10", "rdm11", "rdm12", "rdm13", "rdm14",
    "q64/dm1", "q64/dm2", "q64/dm3", "q64/dm4", "q64/dm5",
    "q64/dm6", "q64/dm7", "q64/dm8", "q64/dm9", "q64/dm10"
)
$expectedMapCount = $expectedReferenceMapIds.Count + $expectedMuffModeMapCount
$maxPoolBytes = 4 * 1024 * 1024
$maxCycleBytes = 256 * 1024
$maxMapIdBytes = 64
$maxTitleBytes = 160
$maxEpisodeBytes = 64
$maxPlayers = 32
$script:strictUtf8 = [System.Text.UTF8Encoding]::new($false, $true)

function Assert-FileBounds {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path,

        [Parameter(Mandatory = $true)]
        [string] $Description,

        [Parameter(Mandatory = $true)]
        [long] $MinimumBytes,

        [Parameter(Mandatory = $true)]
        [long] $MaximumBytes
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing $Description`: $Path"
    }

    $length = (Get-Item -LiteralPath $Path).Length
    if ($length -lt $MinimumBytes -or $length -gt $MaximumBytes) {
        throw "$Description size $length is outside $MinimumBytes..$MaximumBytes bytes: $Path"
    }
}

function Get-CanonicalMapId {
    param(
        [Parameter(Mandatory = $true)]
        [string] $MapId
    )

    $canonical = [System.Text.StringBuilder]::new($MapId.Length)
    foreach ($character in $MapId.ToCharArray()) {
        if ($character -ge 'A' -and $character -le 'Z') {
            [void] $canonical.Append([char]([int] $character + 32))
        }
        elseif ($character -eq '\') {
            [void] $canonical.Append('/')
        }
        else {
            [void] $canonical.Append($character)
        }
    }

    return $canonical.ToString()
}

function Assert-CanonicalMapId {
    param(
        [Parameter(Mandatory = $true)]
        [string] $MapId,

        [Parameter(Mandatory = $true)]
        [string] $Context
    )

    $byteLength = $script:strictUtf8.GetByteCount($MapId)
    if ($byteLength -eq 0 -or $byteLength -ge $maxMapIdBytes) {
        throw "$Context must be 1..$($maxMapIdBytes - 1) UTF-8 bytes."
    }

    if ($MapId.IndexOf("..", [System.StringComparison]::Ordinal) -ge 0) {
        throw "$Context contains a traversal sequence."
    }

    $segmentStart = 0
    for ($index = 0; $index -lt $MapId.Length; $index++) {
        $character = $MapId[$index]
        $codePoint = [int] $character
        if (
            $codePoint -le 0x20 -or
            $codePoint -eq 0x7f -or
            $character -in @('"', "'", ';', ':', '*', '?', '<', '>', '|')
        ) {
            throw "$Context contains an unsafe character."
        }

        if ($character -ne '/' -and $character -ne '\') {
            continue
        }

        $segment = $MapId.Substring($segmentStart, $index - $segmentStart)
        if ($segment.Length -eq 0 -or $segment -ceq "." -or $segment -ceq "..") {
            throw "$Context contains an unsafe path segment."
        }
        $segmentStart = $index + 1
    }

    $finalSegment = $MapId.Substring($segmentStart)
    if ($finalSegment.Length -eq 0 -or $finalSegment -ceq "." -or $finalSegment -ceq "..") {
        throw "$Context contains an unsafe path segment."
    }

    $canonical = Get-CanonicalMapId -MapId $MapId
    if ($MapId -cne $canonical) {
        throw "$Context is not canonical; use lowercase ASCII and forward slashes."
    }

    return $canonical
}

function Assert-DisplayString {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string] $Value,

        [Parameter(Mandatory = $true)]
        [int] $MaximumBytes,

        [Parameter(Mandatory = $true)]
        [string] $Context
    )

    if ($script:strictUtf8.GetByteCount($Value) -gt $MaximumBytes) {
        throw "$Context exceeds $MaximumBytes UTF-8 bytes."
    }

    foreach ($character in $Value.ToCharArray()) {
        $codePoint = [int] $character
        if ($codePoint -lt 0x20 -or $codePoint -eq 0x7f) {
            throw "$Context contains a control character."
        }
    }
}

function Get-UniqueJsonProperties {
    param(
        [Parameter(Mandatory = $true)]
        [System.Text.Json.JsonElement] $Object,

        [Parameter(Mandatory = $true)]
        [string] $Context
    )

    $properties = [System.Collections.Generic.List[System.Text.Json.JsonProperty]]::new()
    $names = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($property in $Object.EnumerateObject()) {
        if (-not $names.Add($property.Name)) {
            throw "$Context contains duplicate property '$($property.Name)'."
        }
        [void] $properties.Add($property)
    }

    return $properties.ToArray()
}

function Add-CycleToken {
    param(
        [Parameter(Mandatory = $true)]
        [System.Text.StringBuilder] $Builder,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.List[string]] $Tokens,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]] $Seen,

        [Parameter(Mandatory = $true)]
        [ref] $RawTokenCount
    )

    if ($Builder.Length -eq 0) {
        return
    }

    $RawTokenCount.Value = [int] $RawTokenCount.Value + 1
    if ($RawTokenCount.Value -gt 4096) {
        throw "Map cycle contains more than 4096 tokens."
    }

    $token = $Builder.ToString()
    [void] $Builder.Clear()
    $canonical = Assert-CanonicalMapId -MapId $token -Context "Map cycle token '$token'"
    if (-not $Seen.Add($canonical)) {
        throw "Map cycle contains duplicate canonical token '$canonical'."
    }
    [void] $Tokens.Add($canonical)
}

function Test-CycleWhitespace {
    param(
        [Parameter(Mandatory = $true)]
        [char] $Character
    )

    $codePoint = [int] $Character
    return $codePoint -eq 0x20 -or ($codePoint -ge 0x09 -and $codePoint -le 0x0d)
}

function ConvertFrom-MapCycle {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string] $Text
    )

    $tokens = [System.Collections.Generic.List[string]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $builder = [System.Text.StringBuilder]::new()
    $rawTokenCount = 0
    $lineComment = $false
    $blockComment = $false

    for ($index = 0; $index -lt $Text.Length; $index++) {
        $character = $Text[$index]
        if ([int] $character -eq 0) {
            throw "Map cycle contains an embedded NUL."
        }

        if ($lineComment) {
            if ($character -eq "`n" -or $character -eq "`r") {
                $lineComment = $false
            }
            continue
        }

        if ($blockComment) {
            if (
                $character -eq '*' -and
                $index + 1 -lt $Text.Length -and
                $Text[$index + 1] -eq '/'
            ) {
                $blockComment = $false
                $index++
            }
            continue
        }

        if ($character -eq '/' -and $index + 1 -lt $Text.Length) {
            $next = $Text[$index + 1]
            if ($next -eq '/' -or $next -eq '*') {
                Add-CycleToken -Builder $builder -Tokens $tokens -Seen $seen -RawTokenCount ([ref] $rawTokenCount)
                $lineComment = $next -eq '/'
                $blockComment = $next -eq '*'
                $index++
                continue
            }
        }

        if (Test-CycleWhitespace -Character $character) {
            Add-CycleToken -Builder $builder -Tokens $tokens -Seen $seen -RawTokenCount ([ref] $rawTokenCount)
            continue
        }

        [void] $builder.Append($character)
    }

    if ($blockComment) {
        throw "Map cycle contains an unterminated block comment."
    }

    Add-CycleToken -Builder $builder -Tokens $tokens -Seen $seen -RawTokenCount ([ref] $rawTokenCount)
    return $tokens.ToArray()
}

function Assert-CycleParserContract {
    $probe = @(ConvertFrom-MapCycle -Text "alpha/* block */beta // line`ngamma")
    if (
        $probe.Count -ne 3 -or
        $probe[0] -cne "alpha" -or
        $probe[1] -cne "beta" -or
        $probe[2] -cne "gamma"
    ) {
        throw "Map-cycle parser comment self-test failed."
    }

    foreach ($invalid in @(
        "alpha /* unterminated",
        "alpha */ beta",
        "alpha bad;map"
    )) {
        $rejected = $false
        try {
            $null = @(ConvertFrom-MapCycle -Text $invalid)
        }
        catch {
            $rejected = $true
        }

        if (-not $rejected) {
            throw "Map-cycle parser rejection self-test failed."
        }
    }
}

Assert-FileBounds -Path $poolPath -Description "map-pool example" -MinimumBytes 32 -MaximumBytes $maxPoolBytes
Assert-FileBounds -Path $cyclePath -Description "map-cycle example" -MinimumBytes 1 -MaximumBytes $maxCycleBytes
Assert-CycleParserContract

$supportedProperties = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($name in @(
    "bsp",
    "title",
    "episode",
    "min",
    "max",
    "dm",
    "tdm",
    "ctf",
    "duel",
    "arena",
    "popular",
    "custom",
    "custom_textures",
    "custom_sounds"
)) {
    [void] $supportedProperties.Add($name)
}

$booleanProperties = @(
    "dm",
    "tdm",
    "ctf",
    "duel",
    "arena",
    "popular",
    "custom",
    "custom_textures",
    "custom_sounds"
)
$modeProperties = @("dm", "tdm", "ctf", "duel", "arena")
$poolEntries = [System.Collections.Generic.List[object]]::new()
$poolIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

$jsonText = [System.IO.File]::ReadAllText($poolPath, $script:strictUtf8)
$jsonOptions = [System.Text.Json.JsonDocumentOptions]::new()
$jsonOptions.AllowTrailingCommas = $false
$jsonOptions.CommentHandling = [System.Text.Json.JsonCommentHandling]::Disallow
$jsonOptions.MaxDepth = 64
$jsonDocument = $null

try {
    $jsonDocument = [System.Text.Json.JsonDocument]::Parse($jsonText, $jsonOptions)
    $root = $jsonDocument.RootElement
    if ($root.ValueKind -ne [System.Text.Json.JsonValueKind]::Object) {
        throw "Map-pool example root must be an object."
    }

    $rootProperties = @(Get-UniqueJsonProperties -Object $root -Context "Map-pool root")
    if ($rootProperties.Count -ne 1 -or $rootProperties[0].Name -cne "maps") {
        throw "Map-pool example root must contain only a 'maps' property."
    }

    $maps = $rootProperties[0].Value
    if ($maps.ValueKind -ne [System.Text.Json.JsonValueKind]::Array) {
        throw "Map-pool 'maps' property must be an array."
    }
    if ($maps.GetArrayLength() -ne $expectedMapCount) {
        throw "Map-pool example must contain exactly $expectedMapCount entries."
    }

    $entryIndex = 0
    foreach ($entry in $maps.EnumerateArray()) {
        $entryIndex++
        $context = "Map-pool entry $entryIndex"
        if ($entry.ValueKind -ne [System.Text.Json.JsonValueKind]::Object) {
            throw "$context must be an object."
        }

        $properties = @(Get-UniqueJsonProperties -Object $entry -Context $context)
        $fields = @{}
        foreach ($property in $properties) {
            if (-not $supportedProperties.Contains($property.Name)) {
                throw "$context contains unsupported property '$($property.Name)'."
            }
            $fields[$property.Name] = $property.Value
        }

        if (-not $fields.ContainsKey("bsp")) {
            throw "$context is missing required string property 'bsp'."
        }
        $bspElement = [System.Text.Json.JsonElement] $fields["bsp"]
        if ($bspElement.ValueKind -ne [System.Text.Json.JsonValueKind]::String) {
            throw "$context property 'bsp' must be a string."
        }
        $bsp = $bspElement.GetString()
        $canonicalBsp = Assert-CanonicalMapId -MapId $bsp -Context "$context BSP '$bsp'"
        if (-not $poolIds.Add($canonicalBsp)) {
            throw "Map-pool example contains duplicate canonical BSP '$canonicalBsp'."
        }

        foreach ($propertyName in $booleanProperties) {
            if (
                $fields.ContainsKey($propertyName) -and
                ([System.Text.Json.JsonElement] $fields[$propertyName]).ValueKind -notin @(
                    [System.Text.Json.JsonValueKind]::True,
                    [System.Text.Json.JsonValueKind]::False
                )
            ) {
                throw "$context property '$propertyName' must be a boolean."
            }
        }

        $hasMode = $false
        foreach ($propertyName in $modeProperties) {
            if (
                $fields.ContainsKey($propertyName) -and
                ([System.Text.Json.JsonElement] $fields[$propertyName]).ValueKind -eq
                    [System.Text.Json.JsonValueKind]::True
            ) {
                $hasMode = $true
            }
        }
        if (-not $hasMode) {
            throw "$context must enable at least one multiplayer mode."
        }

        $minimumPlayers = 0
        $maximumPlayers = 0
        foreach ($propertyName in @("min", "max")) {
            if (-not $fields.ContainsKey($propertyName)) {
                continue
            }

            $number = [System.Text.Json.JsonElement] $fields[$propertyName]
            [int] $parsed = 0
            if (
                $number.ValueKind -ne [System.Text.Json.JsonValueKind]::Number -or
                -not $number.TryGetInt32([ref] $parsed) -or
                $parsed -lt 0 -or
                $parsed -gt $maxPlayers
            ) {
                throw "$context property '$propertyName' must be an integer in 0..$maxPlayers."
            }

            if ($propertyName -ceq "min") {
                $minimumPlayers = $parsed
            }
            else {
                $maximumPlayers = $parsed
            }
        }
        if (
            $minimumPlayers -gt 0 -and
            $maximumPlayers -gt 0 -and
            $minimumPlayers -gt $maximumPlayers
        ) {
            throw "$context property 'min' exceeds 'max'."
        }

        $episode = ""
        foreach ($propertySpec in @(
            @{ Name = "title"; MaximumBytes = $maxTitleBytes },
            @{ Name = "episode"; MaximumBytes = $maxEpisodeBytes }
        )) {
            $propertyName = $propertySpec.Name
            if (-not $fields.ContainsKey($propertyName)) {
                continue
            }

            $textElement = [System.Text.Json.JsonElement] $fields[$propertyName]
            if ($textElement.ValueKind -ne [System.Text.Json.JsonValueKind]::String) {
                throw "$context property '$propertyName' must be a string."
            }
            $text = $textElement.GetString()
            Assert-DisplayString -Value $text -MaximumBytes $propertySpec.MaximumBytes -Context "$context property '$propertyName'"
            if ($propertyName -ceq "episode") {
                $episode = $text
            }
        }

        [void] $poolEntries.Add([pscustomobject] @{
            Bsp = $canonicalBsp
            Episode = $episode
        })
    }
}
catch {
    throw "Invalid map-pool example: $($_.Exception.Message)"
}
finally {
    if ($null -ne $jsonDocument) {
        $jsonDocument.Dispose()
    }
}

if ($poolEntries.Count -ne $expectedMapCount -or $poolIds.Count -ne $expectedMapCount) {
    throw "Map-pool example must contain exactly $expectedMapCount unique usable maps."
}

$cycleBytes = [System.IO.File]::ReadAllBytes($cyclePath)
if (
    $cycleBytes.Length -ge 3 -and
    $cycleBytes[0] -eq 0xef -and
    $cycleBytes[1] -eq 0xbb -and
    $cycleBytes[2] -eq 0xbf
) {
    throw "Map-cycle example must not contain a UTF-8 BOM."
}
$cycleText = $script:strictUtf8.GetString($cycleBytes)
$cycleIds = @(ConvertFrom-MapCycle -Text $cycleText)
if ($cycleIds.Count -ne $expectedMapCount) {
    throw "Map-cycle example must contain exactly $expectedMapCount unique canonical tokens."
}

$cycleSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($mapId in $cycleIds) {
    if (-not $cycleSet.Add($mapId)) {
        throw "Map-cycle example contains duplicate canonical token '$mapId'."
    }
}
if (-not $cycleSet.SetEquals($poolIds)) {
    $missing = @($poolIds | Where-Object { -not $cycleSet.Contains($_) } | Sort-Object)
    $unknown = @($cycleSet | Where-Object { -not $poolIds.Contains($_) } | Sort-Object)
    throw "Map cycle and pool differ (missing: $($missing -join ', '); unknown: $($unknown -join ', '))."
}

if (-not (Test-Path -LiteralPath $mapsPath -PathType Container)) {
    throw "Missing packaged maps directory: $mapsPath"
}
$packagedMaps = @(Get-ChildItem -LiteralPath $mapsPath -Filter "mm-*.bsp" -File)
if ($packagedMaps.Count -ne $expectedMuffModeMapCount) {
    throw "Expected exactly $expectedMuffModeMapCount packaged mm-*.bsp files; found $($packagedMaps.Count)."
}

$packagedIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($mapFile in $packagedMaps) {
    if ($mapFile.Length -le 0) {
        throw "Packaged map is empty: $($mapFile.FullName)"
    }
    $mapId = Assert-CanonicalMapId -MapId $mapFile.BaseName -Context "Packaged map '$($mapFile.Name)'"
    if (-not $packagedIds.Add($mapId)) {
        throw "Packaged maps contain duplicate canonical BSP '$mapId'."
    }
}

$muffModeIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($entry in @($poolEntries | Where-Object { $_.Episode -ceq "muffmode" })) {
    [void] $muffModeIds.Add($entry.Bsp)
}
if (
    $muffModeIds.Count -ne $expectedMuffModeMapCount -or
    -not $muffModeIds.SetEquals($packagedIds)
) {
    throw "The episode=muffmode pool entries must exactly match the packaged nonempty mm-*.bsp files."
}

$expectedReferenceIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($mapId in $expectedReferenceMapIds) {
    [void] $expectedReferenceIds.Add($mapId)
}
$referenceIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($entry in @($poolEntries | Where-Object { $_.Episode -cne "muffmode" })) {
    [void] $referenceIds.Add($entry.Bsp)
}
if (-not $referenceIds.SetEquals($expectedReferenceIds)) {
    $missing = @($expectedReferenceIds | Where-Object { -not $referenceIds.Contains($_) } | Sort-Object)
    $unknown = @($referenceIds | Where-Object { -not $expectedReferenceIds.Contains($_) } | Sort-Object)
    throw "The reference catalog differs from the expected Q2R multiplayer set (missing: $($missing -join ', '); unknown: $($unknown -join ', '))."
}

Write-Host "Map-pool assets valid: $expectedMapCount pool maps, $expectedMapCount cycle maps, $expectedMuffModeMapCount packaged MuffMode BSPs."
