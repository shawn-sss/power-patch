@echo off
REM Open the Visual Studio solution

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

set "SLN=!BUILD_DIR!\!PROJECT_NAME!.sln"

echo [Open] Solution: !SLN!
echo.

if not exist "!SLN!" (
  echo [ERROR] Solution not found.
  echo        Run scripts\cmake\cmake_configure.bat first.
  goto :endfail
)

start "" "!SLN!"
echo [OK] Opened solution.
goto :endok

:endfail
echo.
echo [FAIL] Press any key to close...
pause >nul
exit /b 1

:endok
echo.
echo [DONE] Press any key to close...
pause >nul
exit /b 0
