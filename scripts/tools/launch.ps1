[CmdletBinding()]
param([string]$Database, [switch]$Headless, [switch]$Mobile, [switch]$Onboarding, [switch]$Release)

$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;$env:PATH"

Get-Process IkonikaApp -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq [System.IO.Path]::GetFullPath("$PSScriptRoot\..\..\build\msvc-debug\app\Debug\IkonikaApp.exe") } | Stop-Process -Force

$Arguments = @()

if ($Mobile)
{
    $Arguments += "-mobile"
}

if ($Onboarding)
{
    $Arguments += "-onboarding"
}

if ($Headless -or $Mobile -or $Onboarding -or $Database)
{
    $Arguments += "-headless"
}

if ($Headless -or $Database)
{
    $Dictionaries = Join-Path $env:TEMP "Ikonika-headless\dictionaries"

    New-Item -ItemType Directory -Force $Dictionaries | Out-Null

    Get-ChildItem "$PSScriptRoot\..\..\data\built\*\*-dictionary.db" |
        Where-Object { -not (Test-Path (Join-Path $Dictionaries $_.Name)) } |
        Copy-Item -Destination $Dictionaries

    $Arguments += "-keep-dictionaries"
}

if ($Database)
{
    $DatabaseFile = (Resolve-Path $Database).Path

    $Arguments += "`"-database-file=$DatabaseFile`""
}

if ($Release)
{
	if ($Arguments.Count -gt 0)
	{
		Start-Process "$PSScriptRoot\..\..\build\msvc-release\app\Release\IkonikaApp.exe" -ArgumentList $Arguments
	}
	else
	{
		Start-Process "$PSScriptRoot\..\..\build\msvc-release\app\Release\IkonikaApp.exe"
	}
}
else
{
	if ($Arguments.Count -gt 0)
	{
		Start-Process "$PSScriptRoot\..\..\build\msvc-debug\app\Debug\IkonikaApp.exe" -ArgumentList $Arguments
	}
	else
	{
		Start-Process "$PSScriptRoot\..\..\build\msvc-debug\app\Debug\IkonikaApp.exe"
	}
}
