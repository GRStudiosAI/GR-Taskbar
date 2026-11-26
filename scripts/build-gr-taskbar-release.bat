@echo off
setlocal
if "%~1"=="" exit /b 2
powershell.exe -NoProfile -File "%~dp0Build-GRTaskbar.ps1" -Configuration Release -Platform "%~1" -WorkRoot "%~dp0..\release-work"
exit /b %errorlevel%
