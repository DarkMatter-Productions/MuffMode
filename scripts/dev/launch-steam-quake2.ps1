$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$launchUrl = "steam://rungameid/2320"
Start-Process $launchUrl
Write-Host "Launched Steam Quake II Rerelease: $launchUrl"
