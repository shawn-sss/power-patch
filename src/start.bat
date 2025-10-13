@echo off
setlocal enableextensions
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "SCRIPT=%~dp0PowerPatch.UI.ps1"
"%PS%" -NoLogo -NoProfile -ExecutionPolicy Bypass -STA -WindowStyle Hidden -File "%SCRIPT%"
set "ec=%errorlevel%"
endlocal & exit /b %ec%
