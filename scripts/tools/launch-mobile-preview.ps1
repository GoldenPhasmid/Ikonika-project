$qml = "C:\Qt\6.11.1\msvc2022_64\bin\qml.exe"
$main = Join-Path $PSScriptRoot\..\..\design\mobile-preview "main.qml"

Get-Process qml -ErrorAction SilentlyContinue | Stop-Process -Force

& $qml $main
