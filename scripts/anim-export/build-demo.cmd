@echo off
call "%~dp0bake.cmd" --build-demo %*
exit /b %errorlevel%
