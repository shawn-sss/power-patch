@echo off
REM Configure the project (generate Visual Studio solution)

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

echo [Configure] Generator: !CMAKE_GENERATOR! (!CMAKE_ARCH!)
echo [Configure] Selected Qt version: !QT_VERSION!
echo [Configure] Qt root: !QT_ROOT!
echo [Configure] Qt6_DIR: !QT6_DIR!
echo [Configure] Build dir: !BUILD_DIR!
echo.

if not exist "!QT6_DIR!\Qt6Config.cmake" (
  echo [ERROR] Qt6Config.cmake not found at:
  echo         !QT6_DIR!\Qt6Config.cmake
  echo.
  echo Fix: install/repair the Qt kit so it contains lib\cmake\Qt6\Qt6Config.cmake
  goto :endfail
)

if exist "!BUILD_DIR!" (
  echo [Configure] Removing existing "!BUILD_DIR!" ...
  rmdir /s /q "!BUILD_DIR!"
)

cmake -S . -B "!BUILD_DIR!" -G "!CMAKE_GENERATOR!" -A !CMAKE_ARCH! ^
  -DQt6_DIR="!QT6_DIR!"

set "EC=!ERRORLEVEL!"
echo.
if not "!EC!"=="0" (
  echo [ERROR] CMake configure failed with exit code !EC!.
  goto :endfail
)

echo [OK] Configure completed successfully.
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
