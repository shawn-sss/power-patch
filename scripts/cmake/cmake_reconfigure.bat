@echo off
REM Reconfigures CMake without deleting the build folder
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

for /f "delims=" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VSINSTALLDIR=%%i"
if not defined VSINSTALLDIR (
  for /f "delims=" %%i in ('"%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VSINSTALLDIR=%%i"
)

if defined VSINSTALLDIR (
  if exist "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat" (
    echo [Reconfigure] Setting up MSVC environment...
    call "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  )
)

echo [Reconfigure] Generator: !CMAKE_GENERATOR! (!CMAKE_ARCH!)
echo [Reconfigure] Selected Qt version: !QT_VERSION!
echo [Reconfigure] Build dir: !BUILD_DIR!
echo.

if not exist "!BUILD_DIR!" (
  echo [ERROR] Build folder not found.
  goto :endfail
)

cmake -S . -B "!BUILD_DIR!" -G "!CMAKE_GENERATOR!" ^
  -DCMAKE_BUILD_TYPE=Release ^
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
