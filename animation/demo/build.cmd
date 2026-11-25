@echo off
call "%~dp0..\..\scripts\anim-export\bake.cmd" --package-demo %*
exit /b %errorlevel%
