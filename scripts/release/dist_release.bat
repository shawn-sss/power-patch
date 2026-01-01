@echo off
REM Package the Release build (wrapper)

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

echo [Dist] Running the Release packaging script...
echo.

call "scripts\release\package_release.bat"
set "EC=!ERRORLEVEL!"

echo.
if not "!EC!"=="0" (
  echo [ERROR] Packaging failed with exit code !EC!.
  goto :endfail
)

echo [OK] Packaging completed successfully.
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
