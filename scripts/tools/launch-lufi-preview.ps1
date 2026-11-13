$qml = "C:\Qt\6.11.1\msvc2022_64\bin\qml.exe"
$main = Join-Path $PSScriptRoot "..\..\design\lufi-preview\main.qml"

& $qml $main
