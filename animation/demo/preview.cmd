@echo off
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0preview.ps1" %*
set "PreviewResult=%errorlevel%"
if not "%PreviewResult%"=="0" pause
exit /b %PreviewResult%
