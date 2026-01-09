@echo off
REM Builds the project in Release configuration
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

for /f "delims=" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VSINSTALLDIR=%%i"
if not defined VSINSTALLDIR (
  for /f "delims=" %%i in ('"%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VSINSTALLDIR=%%i"
)

if defined VSINSTALLDIR (
  if exist "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat" (
    call "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    
    where nmake >nul 2>&1
    if errorlevel 1 (
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
        )
      )
    )
  )
)

echo [Build] Configuration: Release
echo [Build] Build dir: !BUILD_DIR!
echo.

if not exist "!BUILD_DIR!\CMakeCache.txt" (
  echo [ERROR] Build folder is not configured.
  goto :endfail
)

cmake --build "!BUILD_DIR!" --config Release --parallel
set "EC=!ERRORLEVEL!"
echo.
if not "!EC!"=="0" (
  echo [ERROR] Build failed with exit code !EC!.
  goto :endfail
)

echo [OK] Release build completed successfully.
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
