@echo off
REM Generates a manifest listing all packaged files
if "%~1"=="" (
  echo Usage: %~nx0 path\to\payload
  exit /b 1
)

setlocal
set "PAYLOAD_DIR=%~1"
set "OUT_FILE=%PAYLOAD_DIR%\package_list.txt"

echo [generate_package_list] Writing manifest at "%OUT_FILE%"...
(dir /b /s "%PAYLOAD_DIR%" 2>nul) > "%OUT_FILE%"
