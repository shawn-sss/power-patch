@echo off
REM Run the app in Debug

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

set "APP_EXE=!BUILD_DIR!\Debug\!PROJECT_NAME!.exe"

echo [Run] Configuration: Debug
echo [Run] Executable: !APP_EXE!
echo [Run] Qt bin: !QT_BIN!
echo.

if not exist "!APP_EXE!" (
  echo [ERROR] Executable not found.
  echo        Build first: scripts\cmake\cmake_build_debug.bat
  goto :endfail
)

set "PATH=!QT_BIN!;!PATH!"
"!APP_EXE!"
set "EC=!ERRORLEVEL!"

echo.
if not "!EC!"=="0" (
  echo [ERROR] App exited with code !EC!.
  goto :endfail
)

echo [OK] App exited normally.
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
