@echo off
REM Set shared script settings (Qt, generator, and folder names)

if /i "%~1"=="--from-script" goto :fromscript

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

:fromscript
set "PROJECT_NAME=PowerPatch"
set "BUILD_DIR=build"
set "DIST_DIR=dist"
set "INSTALL_DIR=install"

set "CMAKE_GENERATOR=Visual Studio 18 2026"
set "CMAKE_ARCH=x64"

set "QT_KIT=msvc2022_64"
set "QT_PREFERRED_VERSION=6.11.0"
set "QT_FALLBACK_VERSION=6.10.1"

set "QT_VERSION=%QT_PREFERRED_VERSION%"
set "QT_ROOT=C:\Qt\!QT_VERSION!\!QT_KIT!"
set "QT6_DIR=!QT_ROOT!\lib\cmake\Qt6"

if not exist "!QT6_DIR!\Qt6Config.cmake" (
  set "QT_VERSION=%QT_FALLBACK_VERSION%"
  set "QT_ROOT=C:\Qt\!QT_VERSION!\!QT_KIT!"
  set "QT6_DIR=!QT_ROOT!\lib\cmake\Qt6"
)

set "QT_BIN=!QT_ROOT!\bin"

if /i "%~1"=="--from-script" exit /b 0

echo [Settings] Project: !PROJECT_NAME!
echo [Settings] Generator: !CMAKE_GENERATOR! (!CMAKE_ARCH!)
echo [Settings] Qt version: !QT_VERSION!
echo [Settings] Qt root: !QT_ROOT!
echo [Settings] Qt6_DIR: !QT6_DIR!
echo.

echo [DONE] Press any key to close...
pause >nul
exit /b 0
