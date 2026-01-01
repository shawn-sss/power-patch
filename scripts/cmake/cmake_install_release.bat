@echo off
REM Install the Release build

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

echo [Install] Configuration: Release
echo [Install] Build dir: !BUILD_DIR!
echo [Install] Install prefix: !INSTALL_DIR!
echo.

if not exist "!BUILD_DIR!\CMakeCache.txt" (
  echo [ERROR] Build folder is not configured.
  echo        Run scripts\cmake\cmake_configure.bat first.
  goto :endfail
)

if exist "!INSTALL_DIR!" (
  echo [Install] Removing existing "!INSTALL_DIR!" ...
  rmdir /s /q "!INSTALL_DIR!"
)

cmake --install "!BUILD_DIR!" --config Release --prefix "!INSTALL_DIR!"
set "EC=!ERRORLEVEL!"
echo.
if not "!EC!"=="0" (
  echo [ERROR] Install failed with exit code !EC!.
  goto :endfail
)

echo [OK] Installed to "!INSTALL_DIR!".
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
