@echo off
REM Package a Release folder (runs windeployqt)

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

set "PKG_ROOT=!DIST_DIR!\!PROJECT_NAME!"
set "PKG_EXE=!PKG_ROOT!\bin\!PROJECT_NAME!.exe"
set "WINDEPLOYQT=!QT_BIN!\windeployqt.exe"

echo [Package] Configuration: Release
echo [Package] Build dir: !BUILD_DIR!
echo [Package] Package folder: !PKG_ROOT!
echo [Package] Selected Qt version: !QT_VERSION!
echo.

if not exist "!BUILD_DIR!\CMakeCache.txt" (
  echo [ERROR] Build folder is not configured.
  echo        Run scripts\cmake\cmake_configure.bat first.
  goto :endfail
)

echo [Package] Building Release...
cmake --build "!BUILD_DIR!" --config Release --parallel
set "EC=!ERRORLEVEL!"
if not "!EC!"=="0" (
  echo.
  echo [ERROR] Release build failed with exit code !EC!.
  goto :endfail
)

echo.
echo [Package] Creating fresh package folder...
if exist "!PKG_ROOT!" rmdir /s /q "!PKG_ROOT!"
mkdir "!PKG_ROOT!" >nul 2>&1

echo [Package] Installing into package folder...
cmake --install "!BUILD_DIR!" --config Release --prefix "!PKG_ROOT!"
set "EC=!ERRORLEVEL!"
if not "!EC!"=="0" (
  echo.
  echo [ERROR] Install failed with exit code !EC!.
  goto :endfail
)

if not exist "!WINDEPLOYQT!" (
  echo.
  echo [ERROR] windeployqt.exe not found at:
  echo         !WINDEPLOYQT!
  goto :endfail
)

if not exist "!PKG_EXE!" (
  echo.
  echo [ERROR] Packaged executable not found at:
  echo         !PKG_EXE!
  goto :endfail
)

echo.
echo [Package] Running windeployqt...
"!WINDEPLOYQT!" "!PKG_EXE!"
set "EC=!ERRORLEVEL!"
if not "!EC!"=="0" (
  echo.
  echo [ERROR] windeployqt failed with exit code !EC!.
  goto :endfail
)

echo.
echo [OK] Package created at:
echo      !PKG_ROOT!

set "ZIP_PATH=!DIST_DIR!\!PROJECT_NAME!_Release.zip"
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { if (Test-Path '%ZIP_PATH%') { Remove-Item -Force '%ZIP_PATH%' }; Compress-Archive -Path '%PKG_ROOT%\*' -DestinationPath '%ZIP_PATH%' -Force; exit 0 } catch { Write-Host $_; exit 1 }" >nul 2>&1
if "!ERRORLEVEL!"=="0" (
  echo.
  echo [OK] Zip created:
  echo      !ZIP_PATH!
) else (
  echo.
  echo [WARN] Could not create zip (Compress-Archive failed). The package folder is still ready.
)

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
