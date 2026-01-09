@echo off
REM Builds Release and creates NSIS self-extracting installer
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

call "scripts\cmake\project_settings.bat" --from-script
set "ROOT_DIR=%CD%"
set "SKIP_PACKAGE=0"

if exist "%DIST_DIR%" (
  echo [Standalone] Removing existing dist folder...
  rmdir /s /q "%DIST_DIR%"
)

for %%A in (%*) do (
  if /i "%%~A"=="--skip-package" set "SKIP_PACKAGE=1"
)

set "LOG_FILE=%ROOT_DIR%\%DIST_DIR%\Build Logs\%PROJECT_NAME%_standalone_build.log"
if not exist "%DIST_DIR%\Build Logs" mkdir "%DIST_DIR%\Build Logs" >nul 2>&1
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%" >nul 2>&1
echo [Standalone] Log start > "%LOG_FILE%"
echo [Standalone] Log file: %LOG_FILE% >> "%LOG_FILE%"
echo [Standalone] Project: %PROJECT_NAME% >> "%LOG_FILE%"
echo [Standalone] Dist dir: %DIST_DIR% >> "%LOG_FILE%"

if "%SKIP_PACKAGE%"=="0" (
  echo [Standalone] Creating release bundle for %PROJECT_NAME%...
  echo.
  echo [Standalone] Running build_package... >> "%LOG_FILE%"

  call "scripts\release\build_package.bat" --no-pause --reconfigure >> "%LOG_FILE%" 2>&1
  call :check_package_result
  if errorlevel 1 goto :endfail
) else (
  echo [Standalone] Skipping build_package (--skip-package specified)
  echo [Standalone] Skipping build_package (--skip-package specified) >> "%LOG_FILE%"
)

set "PKG_ROOT=%DIST_DIR%\Build Package\%PROJECT_NAME%"
goto :after_check

:check_package_result
if not errorlevel 1 (
  echo [Standalone] build_package completed successfully >> "%LOG_FILE%"
  exit /b 0
)
echo.
echo [ERROR] Release packaging failed; aborting standalone step.
echo [ERROR] Release packaging failed; aborting standalone step. >> "%LOG_FILE%"
exit /b 1

:after_check
set "STANDALONE_ROOT=%~dp0"
set "WORK_DIR=%ROOT_DIR%\%DIST_DIR%\Build Artifacts"
set "PAYLOAD_DIR=%WORK_DIR%\payload"
set "OUTPUT_EXE=%ROOT_DIR%\%DIST_DIR%\Portable Self-Extracting\Power Patch.exe"
set "TEMPLATE=%STANDALONE_ROOT%standalone.nsi.template"
set "NSIS_SCRIPT=%WORK_DIR%\package.nsi"
set "ICON_PATH=%ROOT_DIR%\assets\powerpatch_master.ico"
set "NSIS_LOG=%ROOT_DIR%\%DIST_DIR%\Build Logs\%PROJECT_NAME%_standalone_nsis.log"
echo [Standalone] Work dir: %WORK_DIR% >> "%LOG_FILE%"
echo [Standalone] Payload dir: %PAYLOAD_DIR% >> "%LOG_FILE%"

if not exist "%ROOT_DIR%\%DIST_DIR%\Portable Self-Extracting" mkdir "%ROOT_DIR%\%DIST_DIR%\Portable Self-Extracting" >nul 2>&1

if exist "%OUTPUT_EXE%" (
  echo [Standalone] Removing previous executable...
  del /f /q "%OUTPUT_EXE%" >nul 2>&1
)

if exist "%WORK_DIR%" rmdir /s /q "%WORK_DIR%"
mkdir "%PAYLOAD_DIR%" >nul 2>&1
echo [Standalone] Work dir created >> "%LOG_FILE%"

robocopy "%PKG_ROOT%" "%PAYLOAD_DIR%" /MIR /NFL /NDL /NJH /NJS /nc /ns /np >nul 2>&1
set "RC=%ERRORLEVEL%"
echo [Standalone] robocopy exit code: %RC% >> "%LOG_FILE%"
if %RC% GEQ 8 (
  echo.
  echo [ERROR] Failed to mirror packaged files into standalone work dir.
  echo [ERROR] Failed to mirror packaged files into standalone work dir. >> "%LOG_FILE%"
  goto :endfail
)

echo [Standalone] Minimizing Qt artifacts in payload...
call "%STANDALONE_ROOT%minimize_qt_build.bat" "%PAYLOAD_DIR%\bin"

echo [Standalone] Generating package list for reference...
call "%STANDALONE_ROOT%generate_package_list.bat" "%PAYLOAD_DIR%"

set "MAKENSIS=%NSIS_BIN%"
set "DEFAULT_MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
if not defined MAKENSIS set "MAKENSIS=makensis"

:locate_makensis
if /i "!MAKENSIS!"=="makensis" (
  for /f "delims=" %%P in ('where makensis 2^>nul') do (
    set "MAKENSIS=%%~fP"
    goto :nsis_found
  )
  if exist "!DEFAULT_MAKENSIS!" (
    set "MAKENSIS=!DEFAULT_MAKENSIS!"
    goto :nsis_found
  )
  echo.
  echo [ERROR] NSIS compiler not found. Install NSIS or set NSIS_BIN.
  echo [ERROR] NSIS compiler not found. Install NSIS or set NSIS_BIN. >> "%LOG_FILE%"
  goto :endfail
)

:nsis_found
echo [Standalone] makensis: %MAKENSIS% >> "%LOG_FILE%"
if not exist "!MAKENSIS!" (
  if exist "!DEFAULT_MAKENSIS!" (
    set "MAKENSIS=!DEFAULT_MAKENSIS!"
    echo [Standalone] makensis: !MAKENSIS! >> "%LOG_FILE%"
    goto :nsis_found
  )
  echo.
  echo [ERROR] NSIS compiler not found at "!MAKENSIS!".
  echo [ERROR] NSIS compiler not found at "!MAKENSIS!". >> "%LOG_FILE%"
  goto :endfail
)

echo [Standalone] Emitting NSIS script...
set "EXE_NAME=%PROJECT_NAME%.exe"

set "APP_VERSION="
for /f "tokens=3" %%V in ('findstr /r "project.*VERSION" "%ROOT_DIR%\CMakeLists.txt"') do (
  set "APP_VERSION=%%V"
)
if not defined APP_VERSION set "APP_VERSION=0.1.0"

set "NSIS_OUTPUT=%NSIS_SCRIPT%"
echo [Standalone] NSIS script: %NSIS_SCRIPT% >> "%LOG_FILE%"
echo [Standalone] Template: %TEMPLATE% >> "%LOG_FILE%"
echo [Standalone] Output exe: %OUTPUT_EXE% >> "%LOG_FILE%"
echo [Standalone] Version: %APP_VERSION% >> "%LOG_FILE%"
dir "%WORK_DIR%" >> "%LOG_FILE%" 2>&1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$template = Get-Content $env:TEMPLATE -Raw; $template = $template.Replace('@@OUTFILE@@', $env:OUTPUT_EXE); $template = $template.Replace('@@ICON@@', $env:ICON_PATH); $template = $template.Replace('@@EXE_NAME@@', $env:EXE_NAME); $template = $template.Replace('@@VERSION@@', $env:APP_VERSION); Set-Content $env:NSIS_OUTPUT -Encoding ASCII -Value $template;"
set "EC=%ERRORLEVEL%"
echo [Standalone] NSIS template emit exit code: %EC% >> "%LOG_FILE%"
if not "%EC%"=="0" goto :ps_emit_fail

goto :ps_emit_ok

:ps_emit_fail
echo.
echo [ERROR] Failed to generate NSIS script (PowerShell error).
echo [ERROR] Failed to generate NSIS script (PowerShell error). >> "%LOG_FILE%"
goto :endfail

:ps_emit_ok

if not exist "%NSIS_SCRIPT%" goto :nsis_missing
echo [Standalone] NSIS script exists >> "%LOG_FILE%"
dir "%NSIS_SCRIPT%" >> "%LOG_FILE%" 2>&1
goto :nsis_ok

:nsis_missing
echo [Standalone] NSIS script missing >> "%LOG_FILE%"
echo.
echo [ERROR] Failed to create NSIS script from template.
echo [ERROR] Failed to create NSIS script from template. >> "%LOG_FILE%"
echo [ERROR] Expected NSIS script at: %NSIS_SCRIPT% >> "%LOG_FILE%"
dir "%WORK_DIR%" >> "%LOG_FILE%" 2>&1
goto :endfail

:nsis_ok

echo [Standalone] Compiling self-extracting exe...
if not exist "%WORK_DIR%" (
  echo.
  echo [ERROR] Work dir not found at "%WORK_DIR%".
  echo [ERROR] Work dir not found at "%WORK_DIR%". >> "%LOG_FILE%"
  goto :endfail
)
echo [Standalone] Starting makensis... >> "%LOG_FILE%"
echo [Standalone] Running makensis (this can take a while)...
echo [Standalone] NSIS log: %NSIS_LOG%
pushd "%WORK_DIR%" >nul
"%MAKENSIS%" /V2 "%NSIS_SCRIPT%" > "%NSIS_LOG%" 2>&1
set "EC=%ERRORLEVEL%"
popd >nul
echo [Standalone] makensis exit code: %EC% >> "%LOG_FILE%"
echo [Standalone] makensis exit code: %EC% >> "%NSIS_LOG%"

if not "%EC%"=="0" (
  echo.
  echo [ERROR] NSIS compilation failed with exit code %EC%.
  if exist "%NSIS_LOG%" (
    echo.
    echo [ERROR] --- NSIS log ---
    type "%NSIS_LOG%"
    echo [ERROR] --- end log ---
  )
  goto :endfail
)

if not exist "%OUTPUT_EXE%" (
  echo.
  echo [ERROR] Output exe not found at "%OUTPUT_EXE%".
  echo [ERROR] Output exe not found at "%OUTPUT_EXE%". >> "%LOG_FILE%"
  goto :endfail
)

echo [Standalone] Modifying PE subsystem to Windows GUI...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes = [System.IO.File]::ReadAllBytes($env:OUTPUT_EXE); $subsystemOffset = [BitConverter]::ToInt32($bytes, 0x3C) + 0x5C; $bytes[$subsystemOffset] = 2; [System.IO.File]::WriteAllBytes($env:OUTPUT_EXE, $bytes);"
set "EC=%ERRORLEVEL%"
echo [Standalone] PE modification exit code: %EC% >> "%LOG_FILE%"
if not "%EC%"=="0" (
  echo [WARN] Failed to modify PE subsystem, executable may show console window.
  echo [WARN] Failed to modify PE subsystem >> "%LOG_FILE%"
)

if not exist "%OUTPUT_EXE%" (
  echo.
  echo [ERROR] Output exe not found at "%OUTPUT_EXE%".
  echo [ERROR] Output exe not found at "%OUTPUT_EXE%". >> "%LOG_FILE%"
  goto :endfail
)

echo.
echo [OK] Standalone exe built:
echo       %OUTPUT_EXE%

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
