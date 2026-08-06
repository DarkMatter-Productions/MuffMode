[CmdletBinding()]
param(
    [string]$Executable = "build\fuzz\fuzz_numeric_parsers.exe",
    [string[]]$SeedCorpus = @(
        "docs-dev\test-assets\fuzz-corpus\numeric-parsers",
        "docs-dev\test-assets\fuzz-corpus\gt-cfg"
    ),

    [ValidateRange(1, 1000000)]
    [int]$Runs = 1000,

    [ValidateRange(1, 1048576)]
    [int]$MaxInputLength = 4096
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$executablePath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Executable))
$seedCorpusPaths = @($SeedCorpus | ForEach-Object {
    if ([System.IO.Path]::IsPathRooted($_)) {
        [System.IO.Path]::GetFullPath($_)
    }
    else {
        [System.IO.Path]::GetFullPath((Join-Path $repoRoot $_))
    }
})
$fuzzRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build\fuzz"))
$workingCorpus = [System.IO.Path]::GetFullPath((Join-Path $fuzzRoot "smoke-corpus"))
$fuzzPrefix = $fuzzRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar

if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "Fuzz target was not found: $executablePath"
}
foreach ($seedCorpusPath in $seedCorpusPaths) {
    if (-not (Test-Path -LiteralPath $seedCorpusPath -PathType Container)) {
        throw "Fuzz seed corpus was not found: $seedCorpusPath"
    }
}
if (-not $workingCorpus.StartsWith($fuzzPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to prepare a fuzz corpus outside the generated fuzz directory: $workingCorpus"
}

if (Test-Path -LiteralPath $workingCorpus) {
    Remove-Item -LiteralPath $workingCorpus -Recurse -Force
}
New-Item -ItemType Directory -Path $workingCorpus | Out-Null

$generatedSeeds = 0
for ($corpusIndex = 0; $corpusIndex -lt $seedCorpusPaths.Count; $corpusIndex++) {
    $seedCorpusPath = $seedCorpusPaths[$corpusIndex]
    $seedFiles = @(Get-ChildItem -LiteralPath $seedCorpusPath -File | Sort-Object Name)
    if ($seedFiles.Count -eq 0) {
        throw "Fuzz seed corpus contains no files: $seedCorpusPath"
    }

    for ($fileIndex = 0; $fileIndex -lt $seedFiles.Count; $fileIndex++) {
        $seed = $seedFiles[$fileIndex]
        $wholeFileName = "corpus-$corpusIndex-file-$fileIndex"
        Copy-Item -LiteralPath $seed.FullName -Destination (Join-Path $workingCorpus $wholeFileName)
        $generatedSeeds++

        # Checked-in text corpora are human-reviewable lists. Give libFuzzer
        # each non-empty case independently as well as the whole source file.
        $lineIndex = 0
        foreach ($line in Get-Content -LiteralPath $seed.FullName) {
            if (-not [string]::IsNullOrWhiteSpace($line)) {
                $linePath = Join-Path $workingCorpus "corpus-$corpusIndex-file-$fileIndex-line-$lineIndex"
                [System.IO.File]::WriteAllText($linePath, $line, [System.Text.UTF8Encoding]::new($false))
                $generatedSeeds++
            }
            $lineIndex++
        }
    }
}
if ($generatedSeeds -eq 0) {
    throw "No fuzz seeds were generated."
}

$arguments = @(
    "-runs=$Runs",
    "-max_len=$MaxInputLength",
    "-timeout=5",
    $workingCorpus
)
Invoke-NativeCommand -FilePath $executablePath -Arguments $arguments -WorkingDirectory $fuzzRoot

Write-Host "libFuzzer smoke completed: $Runs runs from $generatedSeeds generated seeds."
