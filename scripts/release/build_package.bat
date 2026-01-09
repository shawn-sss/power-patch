@echo off
REM Builds Release and packages with windeployqt for distribution
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

for /f "delims=" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VSINSTALLDIR=%%i"
if not defined VSINSTALLDIR (
  for /f "delims=" %%i in ('"%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VSINSTALLDIR=%%i"
)

if defined VSINSTALLDIR (
  if exist "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat" (
    echo [Package] Setting up MSVC environment...
    call "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    
    where nmake >nul 2>&1
    if errorlevel 1 (
      echo [Package] vcvars64.bat incomplete, applying manual path setup...
      set "VSPATH=!VSINSTALLDIR!\VC\Tools\MSVC"
      for /f "delims=" %%v in ('dir /b /ad /o-n "!VSPATH!" 2^>nul') do (
        set "VCVER=%%v"
        goto :vcver_found
      )
      :vcver_found
      if defined VCVER (
        set "PATH=!VSPATH!\!VCVER!\bin\Hostx64\x64;!PATH!"
        set "LIB=!VSPATH!\!VCVER!\lib\x64;!LIB!"
        set "LIBPATH=!VSPATH!\!VCVER!\lib\x64;!LIBPATH!"
        set "INCLUDE=!VSPATH!\!VCVER!\include;!VSPATH!\!VCVER!\atlmfc\include;!INCLUDE!"
        
        for /f "delims=" %%k in ('dir /b /ad /o-n "C:\Program Files (x86)\Windows Kits\10\bin" 2^>nul') do (
          set "SDKVER=%%k"
          goto :sdkver_found
        )
        :sdkver_found
        if defined SDKVER (
          set "PATH=C:\Program Files (x86)\Windows Kits\10\bin\!SDKVER!\x64;!PATH!"
          set "LIB=C:\Program Files (x86)\Windows Kits\10\Lib\!SDKVER!\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\!SDKVER!\ucrt\x64;!LIB!"
          set "INCLUDE=C:\Program Files (x86)\Windows Kits\10\Include\!SDKVER!\um;C:\Program Files (x86)\Windows Kits\10\Include\!SDKVER!\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\!SDKVER!\shared;!INCLUDE!"
        )
      )
    )
  )
)

set "NO_PAUSE=0"
set "RECONFIGURE=0"
for %%A in (%*) do (
  if /i "%%~A"=="--no-pause" set "NO_PAUSE=1"
  if /i "%%~A"=="--reconfigure" set "RECONFIGURE=1"
)

set "PKG_ROOT=!DIST_DIR!\Build Package\!PROJECT_NAME!"
set "PKG_EXE=!PKG_ROOT!\bin\!PROJECT_NAME!.exe"
set "WINDEPLOYQT=!QT_BIN!\windeployqt.exe"

echo [Package] Configuration: Release
echo [Package] Build dir: !BUILD_DIR!
echo [Package] Package folder: !PKG_ROOT!
echo [Package] Selected Qt version: !QT_VERSION!
echo.

if "%RECONFIGURE%"=="1" (
  echo [Package] Reconfiguring CMake project...
  if defined VSINSTALLDIR (
    cmake -S . -B "!BUILD_DIR!" -G "!CMAKE_GENERATOR!" -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="!QT6_DIR!"
  ) else (
    cmd /c "\"!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat\" && cmake -S . -B \"!BUILD_DIR!\" -G \"!CMAKE_GENERATOR!\" -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=\"!QT6_DIR!\""
  )
  set "EC=!ERRORLEVEL!"
  if not "!EC!"=="0" (
    echo.
    echo [ERROR] CMake reconfigure failed with exit code !EC!.
    goto :endfail
  )
) else (
  if not exist "!BUILD_DIR!\CMakeCache.txt" (
    echo [ERROR] Build folder is not configured.
    goto :endfail
  )
)

echo [Package] Building Release...
if defined VSINSTALLDIR (
  cmake --build "!BUILD_DIR!" --config Release --parallel
) else (
  cmd /c "\"!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat\" && cmake --build \"!BUILD_DIR!\" --config Release --parallel"
)
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
"!WINDEPLOYQT!" "!PKG_EXE!" --no-svg --no-pdf --no-network --no-translations --no-opengl-sw
set "EC=!ERRORLEVEL!"
if not "!EC!"=="0" (
  echo.
  echo [ERROR] windeployqt failed with exit code !EC!.
  goto :endfail
)

echo.
echo [OK] Package created at:
echo      !PKG_ROOT!

set "ZIP_PATH=!DIST_DIR!\Build Package\!PROJECT_NAME!_Release.zip"
if not exist "!DIST_DIR!\Build Package" mkdir "!DIST_DIR!\Build Package" >nul 2>&1
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { if (Test-Path '%ZIP_PATH%') { Remove-Item -Force '%ZIP_PATH%' }; Compress-Archive -Path '%PKG_ROOT%\*' -DestinationPath '%ZIP_PATH%' -Force; exit 0 } catch { Write-Host $_; exit 1 }" >nul 2>&1
if not "!ERRORLEVEL!"=="0" goto :zipfail
echo.
echo [OK] Zip created:
echo      !ZIP_PATH!
goto :zipdone

:zipfail
echo.
echo [WARN] Could not create zip. The package folder is still ready.

:zipdone

goto :endok

:endfail
echo.
if "%NO_PAUSE%"=="1" (
  echo [FAIL] Packaging failed.
  exit /b 1
)
echo [FAIL] Press any key to close...
pause >nul
exit /b 1

:endok
echo.
if "%NO_PAUSE%"=="1" (
  echo [DONE] Packaging complete.
  exit /b 0
)
echo [DONE] Press any key to close...
pause >nul
exit /b 0
