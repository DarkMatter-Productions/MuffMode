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
    throw "Map-pool package root does not exist: $packageRootPath"
}

$mapDbPath = Join-Path $packageRootPath "rerelease\baseq2\mapdb.json"
$poolPath = Join-Path $packageRootPath "rerelease\baseq2\muffmode-map-pool.json"
$cyclePath = Join-Path $packageRootPath "rerelease\baseq2\muffmode-map-cycle.txt"
$mapsPath = Join-Path $packageRootPath "rerelease\baseq2\maps"
$navigationPath = Join-Path $packageRootPath "rerelease\bots\navigation"

$expectedReferenceDmMapIds = @(
    "q2dm1", "q2dm2", "q2dm3", "q2dm4", "q2dm5", "q2dm6", "q2dm7", "q2dm8",
    "base64", "city64", "sewer64", "mgdm1",
    "xdm1", "xdm2", "xdm3", "xdm4", "xdm5", "xdm6", "xdm7",
    "rdm1", "rdm2", "rdm3", "rdm4", "rdm5", "rdm6", "rdm7",
    "rdm8", "rdm9", "rdm10", "rdm11", "rdm12", "rdm13", "rdm14",
    "q64/dm1", "q64/dm2", "q64/dm3", "q64/dm4", "q64/dm5",
    "q64/dm6", "q64/dm7", "q64/dm8", "q64/dm9", "q64/dm10"
)
$expectedReferenceCtfMapIds = @(
    "q2ctf1", "q2ctf2", "q2ctf3", "q2ctf4", "q2ctf5",
    "ndctf0", "q2kctf1", "q2kctf2"
)
$expectedMuffModeMapIds = @(
    "mm-aerow", "mm-czero", "mm-crucible", "mm-kmach",
    "mm-powertrip", "mm-rage", "mm-rail101", "mm-reclam",
    "mm-recycler"
)
$expectedAutomaticMuffModeMapIds = @(
    "mm-aerow", "mm-czero", "mm-crucible", "mm-kmach",
    "mm-powertrip", "mm-rage", "mm-reclam", "mm-recycler"
)
$expectedCycleExcludedMapIds = @(
    "base64", "city64", "sewer64", "mm-rail101"
)
$expectedTdmMapIds = @(
    $expectedReferenceDmMapIds | Where-Object { $_ -cne "q64/dm9" }
) + @(
    "mm-aerow", "mm-czero", "mm-crucible", "mm-kmach", "mm-rage"
)
$expectedDuelMapIds = @(
    "q2dm1", "q2dm2", "q2dm3", "q2dm8",
    "xdm1", "xdm2", "xdm5", "rdm4",
    "q64/dm1", "q64/dm2", "q64/dm3", "q64/dm4", "q64/dm5",
    "q64/dm6", "q64/dm8", "q64/dm9", "q64/dm10",
    "mm-aerow", "mm-crucible", "mm-kmach", "mm-powertrip", "mm-rage",
    "mm-reclam", "mm-recycler"
)
$expectedArenaMapIds = @()
$expectedPopularMapIds = @(
    "q2dm1", "q2dm2", "q2dm3", "q2dm5", "q2dm8", "mgdm1",
    "q2ctf1", "q2ctf2", "xdm1", "xdm2", "xdm7", "rdm4", "rdm14",
    "q64/dm1", "mm-aerow", "mm-czero", "mm-crucible", "mm-kmach",
    "mm-powertrip", "mm-rage", "mm-reclam", "mm-recycler"
)
$expectedPlayerBoundsByMapId = @{
    "q2dm1" = @{ Minimum = 1; Maximum = 8 }
    "q2dm2" = @{ Minimum = 2; Maximum = 8 }
    "q2dm3" = @{ Minimum = 1; Maximum = 8 }
    "q2dm4" = @{ Minimum = 6; Maximum = 16 }
    "q2dm5" = @{ Minimum = 2; Maximum = 10 }
    "q2dm6" = @{ Minimum = 2; Maximum = 10 }
    "q2dm7" = @{ Minimum = 2; Maximum = 8 }
    "q2dm8" = @{ Minimum = 2; Maximum = 8 }
    "base64" = @{ Minimum = 16; Maximum = 32 }
    "city64" = @{ Minimum = 16; Maximum = 32 }
    "sewer64" = @{ Minimum = 16; Maximum = 32 }
    "mgdm1" = @{ Minimum = 4; Maximum = 16 }
    "q2ctf1" = @{ Minimum = 4; Maximum = 16 }
    "q2ctf2" = @{ Minimum = 4; Maximum = 16 }
    "q2ctf3" = @{ Minimum = 6; Maximum = 20 }
    "q2ctf4" = @{ Minimum = 6; Maximum = 20 }
    "q2ctf5" = @{ Minimum = 6; Maximum = 24 }
    "ndctf0" = @{ Minimum = 2; Maximum = 10 }
    "q2kctf1" = @{ Minimum = 2; Maximum = 12 }
    "q2kctf2" = @{ Minimum = 4; Maximum = 18 }
    "xdm1" = @{ Minimum = 2; Maximum = 8 }
    "xdm2" = @{ Minimum = 2; Maximum = 10 }
    "xdm3" = @{ Minimum = 6; Maximum = 16 }
    "xdm4" = @{ Minimum = 4; Maximum = 12 }
    "xdm5" = @{ Minimum = 2; Maximum = 8 }
    "xdm6" = @{ Minimum = 6; Maximum = 18 }
    "xdm7" = @{ Minimum = 4; Maximum = 12 }
    "rdm1" = @{ Minimum = 4; Maximum = 12 }
    "rdm2" = @{ Minimum = 4; Maximum = 10 }
    "rdm3" = @{ Minimum = 6; Maximum = 18 }
    "rdm4" = @{ Minimum = 2; Maximum = 8 }
    "rdm5" = @{ Minimum = 6; Maximum = 16 }
    "rdm6" = @{ Minimum = 8; Maximum = 24 }
    "rdm7" = @{ Minimum = 10; Maximum = 32 }
    "rdm8" = @{ Minimum = 8; Maximum = 24 }
    "rdm9" = @{ Minimum = 6; Maximum = 18 }
    "rdm10" = @{ Minimum = 4; Maximum = 12 }
    "rdm11" = @{ Minimum = 10; Maximum = 32 }
    "rdm12" = @{ Minimum = 4; Maximum = 12 }
    "rdm13" = @{ Minimum = 8; Maximum = 24 }
    "rdm14" = @{ Minimum = 4; Maximum = 12 }
    "q64/dm1" = @{ Minimum = 1; Maximum = 6 }
    "q64/dm2" = @{ Minimum = 2; Maximum = 8 }
    "q64/dm3" = @{ Minimum = 1; Maximum = 6 }
    "q64/dm4" = @{ Minimum = 2; Maximum = 6 }
    "q64/dm5" = @{ Minimum = 2; Maximum = 6 }
    "q64/dm6" = @{ Minimum = 1; Maximum = 4 }
    "q64/dm7" = @{ Minimum = 4; Maximum = 10 }
    "q64/dm8" = @{ Minimum = 2; Maximum = 6 }
    "q64/dm9" = @{ Minimum = 1; Maximum = 4 }
    "q64/dm10" = @{ Minimum = 2; Maximum = 6 }
    "mm-aerow" = @{ Minimum = 2; Maximum = 4 }
    "mm-czero" = @{ Minimum = 3; Maximum = 8 }
    "mm-crucible" = @{ Minimum = 2; Maximum = 6 }
    "mm-kmach" = @{ Minimum = 2; Maximum = 8 }
    "mm-powertrip" = @{ Minimum = 2; Maximum = 4 }
    "mm-rage" = @{ Minimum = 2; Maximum = 8 }
    "mm-rail101" = @{ Minimum = 2; Maximum = 6 }
    "mm-reclam" = @{ Minimum = 2; Maximum = 4 }
    "mm-recycler" = @{ Minimum = 2; Maximum = 5 }
}
$expectedKexEpisodes = @(
    [pscustomobject] @{ Id = "baseq2"; Command = "newgame"; Name = "Quake II"; Activity = "start_quake" }
    [pscustomobject] @{ Id = "mg2"; Command = "newgame_mg2"; Name = "Call of the Machine"; Activity = "start_mg2" }
    [pscustomobject] @{ Id = "xatrix"; Command = "newgame_xatrix"; Name = "The Reckoning"; Activity = "start_xatrix" }
    [pscustomobject] @{ Id = "rogue"; Command = "newgame_rogue"; Name = "Ground Zero"; Activity = "start_rogue" }
    [pscustomobject] @{ Id = "n64"; Command = "newgame_n64"; Name = "Quake II 64"; Activity = "start_n64" }
)
$expectedKexMuffRows = @(
    [pscustomobject] @{ Bsp = "mm-aerow"; Title = "Aerowalk"; Bot = $true }
    [pscustomobject] @{ Bsp = "mm-czero"; Title = "Cold Zero"; Bot = $false }
    [pscustomobject] @{ Bsp = "mm-crucible"; Title = "The Crucible"; Bot = $false }
    [pscustomobject] @{ Bsp = "mm-kmach"; Title = "The Killing Machine"; Bot = $true }
    [pscustomobject] @{ Bsp = "mm-powertrip"; Title = "Powertrip"; Bot = $false }
    [pscustomobject] @{ Bsp = "mm-rage"; Title = "The Rage"; Bot = $true }
    [pscustomobject] @{ Bsp = "mm-rail101"; Title = "Railgun 101"; Bot = $false }
    [pscustomobject] @{ Bsp = "mm-reclam"; Title = "Reclamation"; Bot = $false }
    [pscustomobject] @{ Bsp = "mm-recycler"; Title = "Recycler"; Bot = $false }
)
$expectedMuffModeMapCount = $expectedMuffModeMapIds.Count
$expectedKexStockMapCount = 235
$expectedKexMapCount = 244
$expectedKexMuffInsertionIndex = 20
# SHA-256 of the compact JSON array containing the reviewed current Q2R stock
# rows plus the three 64-player rerelease rows used by the production pool.
$expectedKexStockSha256 = "6f8c1c8048c329bd4e26eda6f77468acda05cc4fc4bcfd382c7d6a0090b94561"
$expectedReferenceMapIds = @(
    $expectedReferenceDmMapIds + $expectedReferenceCtfMapIds
)
$expectedCycleMapIds = @(
    "q2dm1", "q2ctf1", "mm-aerow", "xdm1", "rdm4", "q64/dm1",
    "q2dm2", "q2ctf2", "mm-crucible", "xdm2", "rdm1", "q64/dm2",
    "q2dm3", "q2ctf3", "mm-czero", "xdm5", "rdm2", "q64/dm3",
    "q2dm5", "q2ctf4", "mm-kmach", "xdm3", "rdm3", "q64/dm4",
    "q2dm8", "q2ctf5", "mm-powertrip", "xdm4", "rdm5", "q64/dm5",
    "q2dm6", "ndctf0", "mm-rage", "xdm7", "rdm10", "q64/dm6",
    "q2dm7", "q2kctf1", "mm-reclam", "xdm6", "rdm12", "q64/dm8",
    "q2dm4", "q2kctf2", "mm-recycler", "mgdm1", "rdm14", "q64/dm10",
    "rdm6", "rdm7", "rdm8", "rdm9", "rdm11", "rdm13", "q64/dm7", "q64/dm9"
)
$expectedMapCount = $expectedReferenceMapIds.Count + $expectedMuffModeMapCount
$expectedCycleMapCount = $expectedCycleMapIds.Count
$maxMapDbBytes = 4 * 1024 * 1024
$maxPoolBytes = 4 * 1024 * 1024
$maxCycleBytes = 256 * 1024
$maxMapIdBytes = 64
$maxTitleBytes = 160
$maxEpisodeBytes = 64
$gameHeaderPath = Join-Path $repoRoot "src\shared\game.h"
$gameHeader = Get-Content -LiteralPath $gameHeaderPath -Raw
$lobbyLimitMatches = [regex]::Matches(
    $gameHeader,
    '(?m)^\s*constexpr size_t MAX_LOBBY_PLAYERS = ([0-9]+);\s*$'
)
if ($lobbyLimitMatches.Count -ne 1) {
    throw "Expected exactly one MAX_LOBBY_PLAYERS definition in $gameHeaderPath."
}
$maxPlayers = [int] $lobbyLimitMatches[0].Groups[1].Value
$script:strictUtf8 = [System.Text.UTF8Encoding]::new($false, $true)

$bspLumpNames = @(
    "entities", "planes", "vertices", "visibility", "nodes", "texture info",
    "faces", "lighting", "leaves", "leaf faces", "leaf brushes", "edges",
    "surface edges", "models", "brushes", "brush sides", "pop", "areas",
    "area portals"
)
# Zero denotes a byte stream or otherwise variable-length lump. QBSP widens
# the indices carried by nodes, faces, leaves, leaf lists, edges, and brush
# sides while retaining the same 19-lump header and version.
$bspStandardLumpRecordBytes = @(
    0, 20, 12, 0, 28, 76, 20, 0, 28, 2, 2, 4, 4, 48, 12, 4, 0, 8, 8
)
$bspExtendedLumpRecordBytes = @(
    0, 20, 12, 0, 44, 76, 28, 0, 52, 4, 4, 8, 4, 48, 12, 8, 0, 8, 8
)
$bspMandatoryLumpIndices = [System.Collections.Generic.HashSet[int]]::new()
foreach ($index in @(0, 1, 2, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18)) {
    [void] $bspMandatoryLumpIndices.Add($index)
}

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

function Assert-ExactMapIdSet {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]] $Expected,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]] $Actual,

        [Parameter(Mandatory = $true)]
        [string] $Description
    )

    $expectedSet = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal
    )
    $actualSet = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal
    )
    foreach ($mapId in $Expected) {
        [void] $expectedSet.Add($mapId)
    }
    foreach ($mapId in $Actual) {
        [void] $actualSet.Add($mapId)
    }

    if (-not $expectedSet.SetEquals($actualSet)) {
        $missing = @($expectedSet | Where-Object { -not $actualSet.Contains($_) } | Sort-Object)
        $unexpected = @($actualSet | Where-Object { -not $expectedSet.Contains($_) } | Sort-Object)
        throw "$Description differs from the expected set (missing: $($missing -join ', '); unexpected: $($unexpected -join ', '))."
    }
}

function Assert-Quake2Bsp {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $headerBytes = 8 + (19 * 8)
    $file = Get-Item -LiteralPath $Path
    if ($file.Length -lt $headerBytes) {
        throw "Packaged map is too small for a Quake II BSP header: $Path"
    }

    $stream = [System.IO.File]::Open(
        $file.FullName,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read
    )
    $reader = $null
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        $magic = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        if ($magic -cnotin @("IBSP", "QBSP")) {
            throw "Packaged map is not a Quake II IBSP/QBSP file: $Path"
        }

        $version = $reader.ReadInt32()
        if ($version -ne 38) {
            throw "Packaged map has unsupported BSP version $version (expected 38): $Path"
        }

        $lumps = [System.Collections.Generic.List[object]]::new(19)
        for ($lumpIndex = 0; $lumpIndex -lt 19; $lumpIndex++) {
            [uint64] $offset = $reader.ReadUInt32()
            [uint64] $length = $reader.ReadUInt32()
            [uint64] $fileLength = $file.Length
            if ($offset -gt $fileLength -or $length -gt ($fileLength - $offset)) {
                throw "Packaged map lump $lumpIndex is outside the file: $Path"
            }
            if ($length -gt 0 -and $offset -lt $headerBytes) {
                throw "Packaged map lump $lumpIndex overlaps the BSP header: $Path"
            }

            $lumpName = $bspLumpNames[$lumpIndex]
            if ($length -eq 0 -and $bspMandatoryLumpIndices.Contains($lumpIndex)) {
                throw "Packaged map has an empty mandatory $lumpName lump ($lumpIndex): $Path"
            }

            $recordBytes = if ($magic -ceq "QBSP") {
                $bspExtendedLumpRecordBytes[$lumpIndex]
            }
            else {
                $bspStandardLumpRecordBytes[$lumpIndex]
            }
            if ($recordBytes -gt 0 -and ($length % $recordBytes) -ne 0) {
                throw "Packaged map $lumpName lump ($lumpIndex) length $length is not a multiple of its $recordBytes-byte record size: $Path"
            }

            if ($lumpIndex -eq 16 -and $length -notin @(0, 256)) {
                throw "Packaged map pop lump (16) must be empty or exactly 256 bytes: $Path"
            }

            [void] $lumps.Add([pscustomobject] @{
                Index = $lumpIndex
                Name = $lumpName
                Offset = $offset
                End = $offset + $length
                Length = $length
            })
        }

        for ($leftIndex = 0; $leftIndex -lt $lumps.Count; $leftIndex++) {
            $left = $lumps[$leftIndex]
            if ($left.Length -eq 0) {
                continue
            }

            for ($rightIndex = $leftIndex + 1; $rightIndex -lt $lumps.Count; $rightIndex++) {
                $right = $lumps[$rightIndex]
                if ($right.Length -eq 0) {
                    continue
                }

                if ($left.Offset -lt $right.End -and $right.Offset -lt $left.End) {
                    throw "Packaged map has overlapping nonempty lumps $($left.Index) ($($left.Name)) and $($right.Index) ($($right.Name)): $Path"
                }
            }
        }
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        else {
            $stream.Dispose()
        }
    }
}

function Assert-Quake2Nav {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $file = Get-Item -LiteralPath $Path
    if ($file.Length -lt 28) {
        throw "Packaged navigation file is too small for a NAV3 header and edict count: $Path"
    }

    $stream = [System.IO.File]::Open(
        $file.FullName,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read
    )
    $reader = $null
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        $magic = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        if ($magic -cne "NAV3") {
            throw "Packaged navigation file does not have NAV3 magic: $Path"
        }

        $version = $reader.ReadUInt32()
        if ($version -ne 6) {
            throw "Packaged navigation file has unsupported version $version (expected 6): $Path"
        }

        [uint64] $nodeCount = $reader.ReadUInt32()
        [uint64] $linkCount = $reader.ReadUInt32()
        [uint64] $traversalCount = $reader.ReadUInt32()
        $tuning = $reader.ReadSingle()
        if (
            [single]::IsNaN($tuning) -or
            [single]::IsInfinity($tuning) -or
            [math]::Abs($tuning - 0.8) -gt 0.000001
        ) {
            throw "Packaged navigation file has invalid global tuning value '$tuning' (expected 0.8): $Path"
        }
        if ($nodeCount -eq 0 -or $nodeCount -gt 65536) {
            throw "Packaged navigation file node count $nodeCount is outside 1..65536: $Path"
        }
        if ($traversalCount -gt 65535) {
            throw "Packaged navigation file traversal count $traversalCount exceeds its 16-bit index space: $Path"
        }

        [uint64] $expectedLength = 28
        $expectedLength += $nodeCount * 20
        $expectedLength += $linkCount * 6
        $expectedLength += $traversalCount * 48
        if ($expectedLength -ne [uint64] $file.Length) {
            throw "Packaged navigation file length $($file.Length) does not match its NAV3 section counts (expected $expectedLength): $Path"
        }

        [uint64] $nextLink = 0
        for ([uint64] $nodeIndex = 0; $nodeIndex -lt $nodeCount; $nodeIndex++) {
            [void] $reader.ReadUInt16() # flags
            [uint64] $numLinks = $reader.ReadUInt16()
            [uint64] $firstLink = $reader.ReadUInt16()
            $radius = $reader.ReadUInt16()
            if ($firstLink -ne $nextLink) {
                throw "Packaged navigation node $nodeIndex starts at link $firstLink; expected contiguous link $nextLink`: $Path"
            }
            if ($radius -eq 0) {
                throw "Packaged navigation node $nodeIndex has a zero acceptance radius: $Path"
            }
            $nextLink += $numLinks
            if ($nextLink -gt $linkCount) {
                throw "Packaged navigation node $nodeIndex extends beyond the declared link table: $Path"
            }
        }
        if ($nextLink -ne $linkCount) {
            throw "Packaged navigation node table references $nextLink links; header declares $linkCount`: $Path"
        }

        for ([uint64] $coordinateIndex = 0; $coordinateIndex -lt ($nodeCount * 3); $coordinateIndex++) {
            $coordinate = $reader.ReadSingle()
            if ([single]::IsNaN($coordinate) -or [single]::IsInfinity($coordinate)) {
                throw "Packaged navigation origin contains a non-finite coordinate: $Path"
            }
        }

        for ([uint64] $linkIndex = 0; $linkIndex -lt $linkCount; $linkIndex++) {
            [uint64] $targetNode = $reader.ReadUInt16()
            [void] $reader.ReadByte() # type
            [void] $reader.ReadByte() # flags
            [uint64] $traversalIndex = $reader.ReadUInt16()
            if ($targetNode -ge $nodeCount) {
                throw "Packaged navigation link $linkIndex targets nonexistent node $targetNode`: $Path"
            }
            if ($traversalIndex -ne 0xffff -and $traversalIndex -ge $traversalCount) {
                throw "Packaged navigation link $linkIndex references nonexistent traversal $traversalIndex`: $Path"
            }
        }

        for (
            [uint64] $coordinateIndex = 0;
            $coordinateIndex -lt ($traversalCount * 12);
            $coordinateIndex++
        ) {
            $coordinate = $reader.ReadSingle()
            if ([single]::IsNaN($coordinate) -or [single]::IsInfinity($coordinate)) {
                throw "Packaged navigation traversal contains a non-finite coordinate: $Path"
            }
        }

        $edictCount = $reader.ReadUInt32()
        if ($edictCount -ne 0) {
            throw "Packaged navigation file has unsupported nonzero edict count $edictCount`: $Path"
        }
        if ($stream.Position -ne $stream.Length) {
            throw "Packaged navigation file has trailing bytes after its declared sections: $Path"
        }
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        else {
            $stream.Dispose()
        }
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
    if (
        $MapId.StartsWith("!", [System.StringComparison]::Ordinal) -or
        $MapId.IndexOf('+') -ge 0 -or
        $MapId.IndexOf('$') -ge 0
    ) {
        throw "$Context contains engine map-command syntax."
    }
    foreach ($extension in @(".bsp", ".cin", ".dm2", ".pcx", ".png")) {
        if ($MapId.EndsWith($extension, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "$Context must be a BSP stem, not an engine state filename."
        }
    }

    $normalized = $MapId.Replace('\', '/')
    if ($normalized.StartsWith("maps/", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Context must not include the engine-added 'maps/' prefix."
    }

    $segmentStart = 0
    for ($index = 0; $index -lt $MapId.Length; $index++) {
        $character = $MapId[$index]
        $codePoint = [int] $character
        if (
            $codePoint -le 0x20 -or
            $codePoint -ge 0x7f -or
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
        Assert-PortableMapSegment -Segment $segment -Context $Context
        $segmentStart = $index + 1
    }

    $finalSegment = $MapId.Substring($segmentStart)
    if ($finalSegment.Length -eq 0 -or $finalSegment -ceq "." -or $finalSegment -ceq "..") {
        throw "$Context contains an unsafe path segment."
    }
    Assert-PortableMapSegment -Segment $finalSegment -Context $Context

    $canonical = Get-CanonicalMapId -MapId $MapId
    if ($MapId -cne $canonical) {
        throw "$Context is not canonical; use lowercase ASCII and forward slashes."
    }

    return $canonical
}

function Assert-PortableMapSegment {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Segment,

        [Parameter(Mandatory = $true)]
        [string] $Context
    )

    if ($Segment.EndsWith('.', [System.StringComparison]::Ordinal)) {
        throw "$Context contains a path segment with a trailing dot."
    }

    $stem = $Segment.Split('.', 2)[0]
    if (
        $stem -match '^(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])$'
    ) {
        throw "$Context contains a reserved Windows device segment."
    }
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
        if (
            $codePoint -lt 0x20 -or
            ($codePoint -ge 0x7f -and $codePoint -le 0x9f) -or
            $codePoint -eq 0x061c -or
            ($codePoint -ge 0x200b -and $codePoint -le 0x200f) -or
            ($codePoint -ge 0x2028 -and $codePoint -le 0x202e) -or
            ($codePoint -ge 0x2066 -and $codePoint -le 0x2069) -or
            $codePoint -eq 0xfeff
        ) {
            throw "$Context contains an unsafe display control."
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

Assert-FileBounds -Path $mapDbPath -Description "KEX map database" -MinimumBytes 128 -MaximumBytes $maxMapDbBytes
Assert-FileBounds -Path $poolPath -Description "MuffMode map pool" -MinimumBytes 32 -MaximumBytes $maxPoolBytes
Assert-FileBounds -Path $cyclePath -Description "MuffMode map cycle" -MinimumBytes 1 -MaximumBytes $maxCycleBytes
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
        throw "MuffMode map-pool root must be an object."
    }

    $rootProperties = @(Get-UniqueJsonProperties -Object $root -Context "Map-pool root")
    if ($rootProperties.Count -ne 1 -or $rootProperties[0].Name -cne "maps") {
        throw "MuffMode map-pool root must contain only a 'maps' property."
    }

    $maps = $rootProperties[0].Value
    if ($maps.ValueKind -ne [System.Text.Json.JsonValueKind]::Array) {
        throw "Map-pool 'maps' property must be an array."
    }
    if ($maps.GetArrayLength() -ne $expectedMapCount) {
        throw "MuffMode map pool must contain exactly $expectedMapCount entries."
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
            throw "MuffMode map pool contains duplicate canonical BSP '$canonicalBsp'."
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

        $dm = $fields.ContainsKey("dm") -and
            ([System.Text.Json.JsonElement] $fields["dm"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
        $tdm = $fields.ContainsKey("tdm") -and
            ([System.Text.Json.JsonElement] $fields["tdm"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
        $ctf = $fields.ContainsKey("ctf") -and
            ([System.Text.Json.JsonElement] $fields["ctf"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
        $duel = $fields.ContainsKey("duel") -and
            ([System.Text.Json.JsonElement] $fields["duel"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
        $arena = $fields.ContainsKey("arena") -and
            ([System.Text.Json.JsonElement] $fields["arena"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
        $popular = $fields.ContainsKey("popular") -and
            ([System.Text.Json.JsonElement] $fields["popular"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
        $custom = $false
        foreach ($customProperty in @("custom", "custom_textures", "custom_sounds")) {
            if (
                $fields.ContainsKey($customProperty) -and
                ([System.Text.Json.JsonElement] $fields[$customProperty]).ValueKind -eq
                    [System.Text.Json.JsonValueKind]::True
            ) {
                $custom = $true
            }
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

        $title = ""
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
            if ($propertyName -ceq "title") {
                $title = $text
            }
            else {
                $episode = $text
            }
        }
        if ([string]::IsNullOrWhiteSpace($title)) {
            throw "$context must include a nonempty display title."
        }
        if ([string]::IsNullOrWhiteSpace($episode)) {
            throw "$context must include a nonempty episode."
        }

        [void] $poolEntries.Add([pscustomobject] @{
            Bsp = $canonicalBsp
            Title = $title
            Episode = $episode
            Dm = $dm
            Tdm = $tdm
            Ctf = $ctf
            Duel = $duel
            Arena = $arena
            Popular = $popular
            MinimumPlayers = $minimumPlayers
            MaximumPlayers = $maximumPlayers
            Custom = $custom
        })
    }
}
catch {
    throw "Invalid MuffMode map pool: $($_.Exception.Message)"
}
finally {
    if ($null -ne $jsonDocument) {
        $jsonDocument.Dispose()
    }
}

if ($poolEntries.Count -ne $expectedMapCount -or $poolIds.Count -ne $expectedMapCount) {
    throw "MuffMode map pool must contain exactly $expectedMapCount unique usable maps."
}

Assert-ExactMapIdSet `
    -Expected @($expectedReferenceDmMapIds + $expectedMuffModeMapIds) `
    -Actual @($poolEntries | Where-Object { $_.Dm } | ForEach-Object { $_.Bsp }) `
    -Description "Map-pool dm=true membership"
Assert-ExactMapIdSet `
    -Expected $expectedReferenceCtfMapIds `
    -Actual @($poolEntries | Where-Object { $_.Ctf } | ForEach-Object { $_.Bsp }) `
    -Description "Map-pool ctf=true membership"
Assert-ExactMapIdSet `
    -Expected $expectedTdmMapIds `
    -Actual @($poolEntries | Where-Object { $_.Tdm } | ForEach-Object { $_.Bsp }) `
    -Description "Map-pool tdm=true membership"
Assert-ExactMapIdSet `
    -Expected $expectedDuelMapIds `
    -Actual @($poolEntries | Where-Object { $_.Duel } | ForEach-Object { $_.Bsp }) `
    -Description "Map-pool duel=true membership"
Assert-ExactMapIdSet `
    -Expected $expectedArenaMapIds `
    -Actual @($poolEntries | Where-Object { $_.Arena } | ForEach-Object { $_.Bsp }) `
    -Description "Map-pool arena=true membership"
Assert-ExactMapIdSet `
    -Expected $expectedPopularMapIds `
    -Actual @($poolEntries | Where-Object { $_.Popular } | ForEach-Object { $_.Bsp }) `
    -Description "Map-pool popular=true membership"
Assert-ExactMapIdSet `
    -Expected $expectedMuffModeMapIds `
    -Actual @($poolEntries | Where-Object { $_.Custom } | ForEach-Object { $_.Bsp }) `
    -Description "Map-pool effective custom=true membership"
Assert-ExactMapIdSet `
    -Expected $expectedMuffModeMapIds `
    -Actual @($poolEntries | Where-Object { $_.Episode -ceq "muffmode" } | ForEach-Object { $_.Bsp }) `
    -Description "Map-pool episode=muffmode membership"
Assert-ExactMapIdSet `
    -Expected @($poolIds) `
    -Actual @($expectedPlayerBoundsByMapId.Keys) `
    -Description "Map-pool explicit player-bounds policy"

foreach ($expectedBounds in $expectedPlayerBoundsByMapId.GetEnumerator()) {
    if (-not $poolIds.Contains($expectedBounds.Key)) {
        throw "Expected player-bounds policy refers to unknown BSP '$($expectedBounds.Key)'."
    }
    if (
        -not $expectedBounds.Value.ContainsKey("Minimum") -or
        -not $expectedBounds.Value.ContainsKey("Maximum")
    ) {
        throw "Expected player-bounds policy for '$($expectedBounds.Key)' must define Minimum and Maximum."
    }
}
foreach ($entry in $poolEntries) {
    $expectedMinimum = 0
    $expectedMaximum = 0
    if ($expectedPlayerBoundsByMapId.ContainsKey($entry.Bsp)) {
        $expectedMinimum = $expectedPlayerBoundsByMapId[$entry.Bsp].Minimum
        $expectedMaximum = $expectedPlayerBoundsByMapId[$entry.Bsp].Maximum
    }

    if (
        $entry.MinimumPlayers -ne $expectedMinimum -or
        $entry.MaximumPlayers -ne $expectedMaximum
    ) {
        throw "Map-pool player bounds for '$($entry.Bsp)' are $($entry.MinimumPlayers)..$($entry.MaximumPlayers); expected $expectedMinimum..$expectedMaximum."
    }
}

$cycleBytes = [System.IO.File]::ReadAllBytes($cyclePath)
if (
    $cycleBytes.Length -ge 3 -and
    $cycleBytes[0] -eq 0xef -and
    $cycleBytes[1] -eq 0xbb -and
    $cycleBytes[2] -eq 0xbf
) {
    throw "MuffMode map cycle must not contain a UTF-8 BOM."
}
$cycleText = $script:strictUtf8.GetString($cycleBytes)
$cycleIds = @(ConvertFrom-MapCycle -Text $cycleText)
if ($cycleIds.Count -ne $expectedCycleMapCount) {
    throw "MuffMode map cycle must contain exactly $expectedCycleMapCount unique canonical tokens."
}
for ($index = 0; $index -lt $expectedCycleMapCount; $index++) {
    if ($cycleIds[$index] -cne $expectedCycleMapIds[$index]) {
        throw "MuffMode map cycle order differs at position $($index + 1): expected '$($expectedCycleMapIds[$index])', found '$($cycleIds[$index])'."
    }
}

$cycleSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($mapId in $cycleIds) {
    if (-not $cycleSet.Add($mapId)) {
        throw "MuffMode map cycle contains duplicate canonical token '$mapId'."
    }
}
Assert-ExactMapIdSet `
    -Expected $expectedCycleMapIds `
    -Actual @($cycleSet) `
    -Description "MuffMode automatic map-cycle membership"
Assert-ExactMapIdSet `
    -Expected @($poolIds) `
    -Actual @($expectedCycleMapIds + $expectedCycleExcludedMapIds) `
    -Description "Automatic-cycle plus explicit-exclusion policy"
foreach ($mapId in $expectedCycleExcludedMapIds) {
    if (-not $poolIds.Contains($mapId)) {
        throw "Cycle-excluded map '$mapId' must remain available in the map pool."
    }
    if ($cycleSet.Contains($mapId)) {
        throw "Cycle-excluded map '$mapId' must not appear in the automatic map cycle."
    }
}

# The shipped factories top out at 12 active players (connected spectator slots
# do not count). Keep at least two in-bounds automatic choices at representative
# factory loads so current-map exclusion and cooldown relaxation do not collapse
# normal selection.
$automaticCoverage = @(
    [pscustomobject] @{ Mode = "Dm"; Label = "DM/Horde"; Loads = @(1, 2, 4, 6, 8, 10, 12) }
    [pscustomobject] @{ Mode = "Tdm"; Label = "team modes"; Loads = @(2, 4, 6, 8, 10, 12) }
    [pscustomobject] @{ Mode = "Ctf"; Label = "CTF modes"; Loads = @(2, 4, 6, 8, 10) }
    [pscustomobject] @{ Mode = "Duel"; Label = "Duel"; Loads = @(2) }
)
foreach ($coverage in $automaticCoverage) {
    foreach ($playerCount in $coverage.Loads) {
        $eligible = @(
            $poolEntries | Where-Object {
                $cycleSet.Contains($_.Bsp) -and
                $_.PSObject.Properties[$coverage.Mode].Value -and
                ($_.MinimumPlayers -eq 0 -or $_.MinimumPlayers -le $playerCount) -and
                ($_.MaximumPlayers -eq 0 -or $_.MaximumPlayers -ge $playerCount)
            }
        )
        if ($eligible.Count -lt 2) {
            throw "Automatic $($coverage.Label) coverage at $playerCount players has $($eligible.Count) in-bounds maps; expected at least 2."
        }
    }
}

if (-not (Test-Path -LiteralPath $mapsPath -PathType Container)) {
    throw "Missing packaged maps directory: $mapsPath"
}
$packagedMaps = @(
    Get-ChildItem -LiteralPath $mapsPath -Filter "*.bsp" -File -Recurse
)
if ($packagedMaps.Count -ne $expectedMuffModeMapCount) {
    throw "Expected exactly $expectedMuffModeMapCount packaged BSP files; found $($packagedMaps.Count)."
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
    Assert-Quake2Bsp -Path $mapFile.FullName
}

Assert-ExactMapIdSet `
    -Expected $expectedMuffModeMapIds `
    -Actual @($packagedIds) `
    -Description "Packaged MuffMode BSP membership"

if (-not (Test-Path -LiteralPath $navigationPath -PathType Container)) {
    throw "Missing packaged bot-navigation directory: $navigationPath"
}
$packagedNavigationFiles = @(
    Get-ChildItem -LiteralPath $navigationPath -Filter "*.nav" -File -Recurse
)
if ($packagedNavigationFiles.Count -eq 0) {
    throw "Packaged bot-navigation directory contains no NAV3 files: $navigationPath"
}
$packagedNavigationIds = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::Ordinal
)
foreach ($navigationFile in $packagedNavigationFiles) {
    $navigationId = Assert-CanonicalMapId `
        -MapId $navigationFile.BaseName `
        -Context "Packaged navigation file '$($navigationFile.Name)'"
    Assert-Quake2Nav -Path $navigationFile.FullName
    if (
        $packagedIds.Contains($navigationId) -and
        -not $packagedNavigationIds.Add($navigationId)
    ) {
        throw "Packaged navigation assets contain duplicate exact-match NAV3 files for '$navigationId'."
    }
}

$kexRows = [System.Collections.Generic.List[object]]::new()
$kexMuffIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$kexRowIdentities = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$kexJsonDocument = $null
try {
    $kexJsonBytes = [System.IO.File]::ReadAllBytes($mapDbPath)
    $kexJsonText = if (
        $kexJsonBytes.Length -ge 3 -and
        $kexJsonBytes[0] -eq 0xef -and
        $kexJsonBytes[1] -eq 0xbb -and
        $kexJsonBytes[2] -eq 0xbf
    ) {
        $script:strictUtf8.GetString($kexJsonBytes, 3, $kexJsonBytes.Length - 3)
    }
    else {
        $script:strictUtf8.GetString($kexJsonBytes)
    }
    $kexJsonDocument = [System.Text.Json.JsonDocument]::Parse($kexJsonText, $jsonOptions)
    $kexRoot = $kexJsonDocument.RootElement
    if ($kexRoot.ValueKind -ne [System.Text.Json.JsonValueKind]::Object) {
        throw "root must be an object."
    }

    $kexRootProperties = @(Get-UniqueJsonProperties -Object $kexRoot -Context "KEX mapdb root")
    $kexRootFields = @{}
    foreach ($property in $kexRootProperties) {
        $kexRootFields[$property.Name] = $property.Value
    }
    Assert-ExactMapIdSet `
        -Expected @("episodes", "maps") `
        -Actual @($kexRootFields.Keys) `
        -Description "KEX mapdb root properties"

    $episodes = [System.Text.Json.JsonElement] $kexRootFields["episodes"]
    $maps = [System.Text.Json.JsonElement] $kexRootFields["maps"]
    if ($episodes.ValueKind -ne [System.Text.Json.JsonValueKind]::Array) {
        throw "root property 'episodes' must be an array."
    }
    if ($maps.ValueKind -ne [System.Text.Json.JsonValueKind]::Array) {
        throw "root property 'maps' must be an array."
    }
    if ($episodes.GetArrayLength() -ne $expectedKexEpisodes.Count) {
        throw "must contain exactly $($expectedKexEpisodes.Count) canonical stock episodes."
    }
    if ($maps.GetArrayLength() -ne $expectedKexMapCount) {
        throw "must contain exactly $expectedKexMapCount map rows."
    }

    $expectedEpisodeIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($expectedEpisode in $expectedKexEpisodes) {
        [void] $expectedEpisodeIds.Add($expectedEpisode.Id)
    }

    $episodeIndex = 0
    foreach ($episode in $episodes.EnumerateArray()) {
        $context = "KEX mapdb episode $($episodeIndex + 1)"
        if ($episode.ValueKind -ne [System.Text.Json.JsonValueKind]::Object) {
            throw "$context must be an object."
        }

        $episodeProperties = @(Get-UniqueJsonProperties -Object $episode -Context $context)
        $episodeFields = @{}
        foreach ($property in $episodeProperties) {
            $episodeFields[$property.Name] = $property.Value
        }
        Assert-ExactMapIdSet `
            -Expected @("id", "command", "name", "activity", "needsSkillSelect") `
            -Actual @($episodeFields.Keys) `
            -Description "$context properties"

        foreach ($propertyName in @("id", "command", "name", "activity")) {
            if (
                ([System.Text.Json.JsonElement] $episodeFields[$propertyName]).ValueKind -ne
                    [System.Text.Json.JsonValueKind]::String
            ) {
                throw "$context property '$propertyName' must be a string."
            }
        }
        if (
            ([System.Text.Json.JsonElement] $episodeFields["needsSkillSelect"]).ValueKind -ne
                [System.Text.Json.JsonValueKind]::True
        ) {
            throw "$context property 'needsSkillSelect' must be true."
        }

        $expectedEpisode = $expectedKexEpisodes[$episodeIndex]
        $actualId = ([System.Text.Json.JsonElement] $episodeFields["id"]).GetString()
        $actualCommand = ([System.Text.Json.JsonElement] $episodeFields["command"]).GetString()
        $actualName = ([System.Text.Json.JsonElement] $episodeFields["name"]).GetString()
        $actualActivity = ([System.Text.Json.JsonElement] $episodeFields["activity"]).GetString()
        if (
            $actualId -cne $expectedEpisode.Id -or
            $actualCommand -cne $expectedEpisode.Command -or
            $actualName -cne $expectedEpisode.Name -or
            $actualActivity -cne $expectedEpisode.Activity
        ) {
            throw "$context does not match the canonical '$($expectedEpisode.Id)' episode row."
        }
        $episodeIndex++
    }

    $mapIndex = 0
    foreach ($map in $maps.EnumerateArray()) {
        $context = "KEX mapdb map row $($mapIndex + 1)"
        if ($map.ValueKind -ne [System.Text.Json.JsonValueKind]::Object) {
            throw "$context must be an object."
        }

        # KEX map rows deliberately do not use the MuffMode pool's 14-property
        # allowlist. Campaign rows contain engine-owned fields and command forms
        # such as cinematics, `+*map` transitions, start_items, and unit metadata.
        $mapProperties = @(Get-UniqueJsonProperties -Object $map -Context $context)
        $mapFields = @{}
        foreach ($property in $mapProperties) {
            $mapFields[$property.Name] = $property.Value
        }
        foreach ($propertyName in @("bsp", "title", "episode")) {
            if (-not $mapFields.ContainsKey($propertyName)) {
                throw "$context is missing required string property '$propertyName'."
            }
            if (
                ([System.Text.Json.JsonElement] $mapFields[$propertyName]).ValueKind -ne
                    [System.Text.Json.JsonValueKind]::String
            ) {
                throw "$context property '$propertyName' must be a string."
            }
        }

        $bsp = ([System.Text.Json.JsonElement] $mapFields["bsp"]).GetString()
        $title = ([System.Text.Json.JsonElement] $mapFields["title"]).GetString()
        $episodeId = ([System.Text.Json.JsonElement] $mapFields["episode"]).GetString()
        if ([string]::IsNullOrWhiteSpace($bsp)) {
            throw "$context property 'bsp' must not be empty."
        }
        if ([string]::IsNullOrWhiteSpace($title)) {
            throw "$context property 'title' must not be empty."
        }
        if (-not $expectedEpisodeIds.Contains($episodeId)) {
            throw "$context property 'episode' names unknown stock episode '$episodeId'."
        }

        # A raw BSP is not globally unique in the stock catalog: for example,
        # q64/rtest is both a unit command row and its separately titled intro.
        # The full UI row identity must still be unique.
        $identity = "$($bsp.Length):${bsp}|$($title.Length):${title}|$($episodeId.Length):${episodeId}"
        if (-not $kexRowIdentities.Add($identity)) {
            throw "$context duplicates the BSP/title/episode identity '$bsp' / '$title' / '$episodeId'."
        }

        $isMuffModeMap = $bsp.StartsWith("mm-", [System.StringComparison]::Ordinal)
        $dm = $false
        $tdm = $false
        $ctf = $false
        $bot = $false
        if ($isMuffModeMap) {
            $canonicalBsp = Assert-CanonicalMapId -MapId $bsp -Context "$context BSP '$bsp'"
            if (-not $kexMuffIds.Add($canonicalBsp)) {
                throw "KEX mapdb contains duplicate MuffMode BSP '$canonicalBsp'."
            }

            foreach ($propertyName in @("dm", "tdm", "ctf", "bot")) {
                if (-not $mapFields.ContainsKey($propertyName)) {
                    throw "$context is missing required boolean property '$propertyName'."
                }
                if (
                    ([System.Text.Json.JsonElement] $mapFields[$propertyName]).ValueKind -notin @(
                        [System.Text.Json.JsonValueKind]::True,
                        [System.Text.Json.JsonValueKind]::False
                    )
                ) {
                    throw "$context property '$propertyName' must be a boolean."
                }
            }
            $dm = ([System.Text.Json.JsonElement] $mapFields["dm"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
            $tdm = ([System.Text.Json.JsonElement] $mapFields["tdm"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
            $ctf = ([System.Text.Json.JsonElement] $mapFields["ctf"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
            $bot = ([System.Text.Json.JsonElement] $mapFields["bot"]).ValueKind -eq
                [System.Text.Json.JsonValueKind]::True
        }

        [void] $kexRows.Add([pscustomobject] @{
            Index = $mapIndex
            Bsp = $bsp
            Title = $title
            Episode = $episodeId
            Dm = $dm
            Tdm = $tdm
            Ctf = $ctf
            Bot = $bot
            IsMuffMode = $isMuffModeMap
            RawJson = [System.Text.Json.JsonSerializer]::Serialize(
                $map,
                [System.Text.Json.JsonElement]
            )
        })
        $mapIndex++
    }
}
catch {
    throw "Invalid KEX mapdb.json: $($_.Exception.Message)"
}
finally {
    if ($null -ne $kexJsonDocument) {
        $kexJsonDocument.Dispose()
    }
}

if (
    $kexRows.Count -ne $expectedKexMapCount -or
    $kexRowIdentities.Count -ne $expectedKexMapCount
) {
    throw "KEX mapdb.json must contain exactly $expectedKexMapCount unique BSP/title/episode rows."
}

$kexStockRows = @($kexRows | Where-Object { -not $_.IsMuffMode })
$kexMuffRows = @($kexRows | Where-Object { $_.IsMuffMode })
if ($kexStockRows.Count -ne $expectedKexStockMapCount) {
    throw "KEX mapdb.json must contain exactly $expectedKexStockMapCount non-mm stock rows."
}
$stockJson = '[' + (($kexStockRows | ForEach-Object { $_.RawJson }) -join ',') + ']'
$stockHashBytes = [System.Security.Cryptography.SHA256]::HashData(
    [System.Text.Encoding]::UTF8.GetBytes($stockJson)
)
$stockHash = [System.Convert]::ToHexString($stockHashBytes).ToLowerInvariant()
if ($stockHash -cne $expectedKexStockSha256) {
    throw "KEX mapdb.json stock baseline differs from the reviewed Q2R catalog (expected $expectedKexStockSha256, found $stockHash)."
}
if ($kexMuffRows.Count -ne $expectedKexMuffRows.Count) {
    throw "KEX mapdb.json must contain exactly $($expectedKexMuffRows.Count) mm-* rows."
}
for ($index = 0; $index -lt $expectedKexMuffRows.Count; $index++) {
    $expected = $expectedKexMuffRows[$index]
    $actual = $kexMuffRows[$index]
    $expectedIndex = $expectedKexMuffInsertionIndex + $index
    if ($actual.Index -ne $expectedIndex) {
        throw "KEX MuffMode row '$($actual.Bsp)' is not immediately after the stock multiplayer rows."
    }
    if (
        $actual.Bsp -cne $expected.Bsp -or
        $actual.Title -cne $expected.Title -or
        $actual.Episode -cne "baseq2" -or
        -not $actual.Dm -or
        -not $actual.Tdm -or
        $actual.Ctf -or
        $actual.Bot -ne $expected.Bot
    ) {
        throw "KEX MuffMode row $($index + 1) does not match the required metadata for '$($expected.Bsp)'."
    }
}

Assert-ExactMapIdSet `
    -Expected @($packagedNavigationIds) `
    -Actual @($kexMuffRows | Where-Object { $_.Bot } | ForEach-Object { $_.Bsp }) `
    -Description "KEX bot=true and exact packaged BSP/NAV3 membership"
Assert-ExactMapIdSet `
    -Expected @($packagedIds) `
    -Actual @($kexMuffIds) `
    -Description "KEX mapdb packaged MuffMode BSP membership"

$muffModeIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($entry in @($poolEntries | Where-Object { $_.Episode -ceq "muffmode" })) {
    [void] $muffModeIds.Add($entry.Bsp)
}
if (
    $muffModeIds.Count -ne $expectedMuffModeMapCount -or
    -not $muffModeIds.SetEquals($packagedIds)
) {
    throw "The episode=muffmode pool entries must exactly match the packaged validated BSP files."
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

Write-Host "Map assets valid: $expectedKexMapCount KEX rows ($expectedKexStockMapCount stock + $expectedMuffModeMapCount MuffMode), $expectedMapCount pool maps, $expectedCycleMapCount automatic cycle maps, $expectedMuffModeMapCount packaged BSPs, $($packagedNavigationIds.Count) exact bot-navigation matches."
