$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;$env:PATH"

$Cdb = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
$App = Join-Path $PSScriptRoot "..\..\build\msvc-debug\app\Debug\IkonikaApp.exe"

if (-not (Test-Path $Cdb))
{
    Write-Output "cdb.exe not found at $Cdb - install the Windows SDK Debugging Tools"
    exit 1
}

Get-Process IkonikaApp -ErrorAction SilentlyContinue | Stop-Process -Force

& $Cdb -g $App
