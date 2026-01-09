@echo off
REM Configures the CMake project with Qt paths and MSVC environment
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

for /f "delims=" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VSINSTALLDIR=%%i"
if not defined VSINSTALLDIR (
  for /f "delims=" %%i in ('"%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VSINSTALLDIR=%%i"
)

if defined VSINSTALLDIR (
  if exist "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat" (
    echo [Configure] Setting up MSVC environment...
    call "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    
    where nmake >nul 2>&1
    if errorlevel 1 (
      echo [Configure] vcvars64.bat incomplete, applying manual path setup...
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
          set "SDKROOT=C:\Program Files (x86)\Windows Kits\10"
          set "PATH=!SDKROOT!\bin\!SDKVER!\x64;!PATH!"
          set "LIB=!SDKROOT!\Lib\!SDKVER!\um\x64;!SDKROOT!\Lib\!SDKVER!\ucrt\x64;!LIB!"
          set "INCLUDE=!SDKROOT!\Include\!SDKVER!\um;!SDKROOT!\Include\!SDKVER!\ucrt;!SDKROOT!\Include\!SDKVER!\shared;!INCLUDE!"
          echo [Configure] Applied VS 2026 workaround with VC !VCVER! and SDK !SDKVER!
        )
      )
    )
  )
)

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
  goto :endfail
)

if exist "!BUILD_DIR!" (
  echo [Configure] Removing existing "!BUILD_DIR!" ...
  rmdir /s /q "!BUILD_DIR!"
)

cmake -S . -B "!BUILD_DIR!" -G "!CMAKE_GENERATOR!" ^
  -DCMAKE_BUILD_TYPE=Release ^
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
