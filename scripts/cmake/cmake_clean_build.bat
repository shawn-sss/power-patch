@echo off
REM Delete the build folder

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

echo [Clean] Target: !BUILD_DIR!
echo.

if not exist "!BUILD_DIR!" (
  echo [OK] Build folder does not exist. Nothing to do.
  goto :endok
)

rmdir /s /q "!BUILD_DIR!"
set "EC=!ERRORLEVEL!"
echo.
if not "!EC!"=="0" (
  echo [ERROR] Failed to delete "!BUILD_DIR!".
  goto :endfail
)

echo [OK] Deleted "!BUILD_DIR!".
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
