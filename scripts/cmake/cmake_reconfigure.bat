@echo off
REM Reconfigure the project (keep build folder)

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

echo [Reconfigure] Generator: !CMAKE_GENERATOR! (!CMAKE_ARCH!)
echo [Reconfigure] Selected Qt version: !QT_VERSION!
echo [Reconfigure] Build dir: !BUILD_DIR!
echo.

if not exist "!BUILD_DIR!" (
  echo [ERROR] Build folder not found.
  echo        Run scripts\cmake\cmake_configure.bat first.
  goto :endfail
)

cmake -S . -B "!BUILD_DIR!" -G "!CMAKE_GENERATOR!" -A !CMAKE_ARCH! ^
  -DQt6_DIR="!QT6_DIR!"

set "EC=!ERRORLEVEL!"
echo.
if not "!EC!"=="0" (
  echo [ERROR] CMake reconfigure failed with exit code !EC!.
  goto :endfail
)

echo [OK] Reconfigure completed successfully.
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
