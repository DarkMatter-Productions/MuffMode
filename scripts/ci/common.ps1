$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

function Resolve-MSBuild {
    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "MSBuild.exe was not found. Run from a Visual Studio developer shell or install Visual Studio Build Tools."
}

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory
    )

    Push-Location $WorkingDirectory
    try {
        Write-Host "> $FilePath $($Arguments -join ' ')"
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$FilePath failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

function Read-AnalyzerBlockingList {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$EntryPattern
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Analyzer blocking list was not found: $Path"
    }

    $entries = [System.Collections.Generic.List[string]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($line in Get-Content -LiteralPath $Path) {
        $entry = $line.Trim()
        if (-not $entry -or $entry.StartsWith('#')) {
            continue
        }
        if ($entry -notmatch $EntryPattern) {
            throw "Invalid analyzer blocking-list entry '$entry' in $Path."
        }
        if (-not $seen.Add($entry)) {
            throw "Duplicate analyzer blocking-list entry '$entry' in $Path."
        }
        $entries.Add($entry)
    }
    if ($entries.Count -eq 0) {
        throw "Analyzer blocking list is empty: $Path"
    }

    $entries
}

function Get-ClangTidyDiagnosticChecks {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Line
    )

    $match = [regex]::Match($Line, '\[(?<checks>[^\]]+)\]\s*$')
    if (-not $match.Success) {
        return @()
    }

    @($match.Groups['checks'].Value -split ',' | ForEach-Object {
        $_.Trim()
    } | Where-Object {
        $_ -and $_ -ne '-warnings-as-errors'
    })
}

function Test-ClangTidyBlockingCheck {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]]$Checks,

        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [string[]]$Patterns
    )

    foreach ($check in $Checks) {
        foreach ($pattern in $Patterns) {
            if ($check -like $pattern) {
                return $true
            }
        }
    }
    return $false
}

function Get-MsvcAnalyzerFinding {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Line
    )

    $normalized = [regex]::Replace($Line.TrimStart(), '^(?:\d+>)+', '')
    $match = [regex]::Match(
        $normalized,
        '\bwarning\s+(?<code>C\d{4,5}):',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success) {
        return $null
    }

    [pscustomobject]@{
        Code = $match.Groups['code'].Value.ToUpperInvariant()
        Text = $normalized
    }
}
