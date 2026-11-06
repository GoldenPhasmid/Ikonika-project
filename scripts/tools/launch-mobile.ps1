param([string]$Database, [switch]$Onboarding)

& "$PSScriptRoot\launch.ps1" -Database $Database -Mobile -Onboarding:$Onboarding
