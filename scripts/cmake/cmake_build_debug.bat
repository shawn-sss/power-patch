@echo off
REM Build the project in Debug

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

echo [Build] Configuration: Debug
echo [Build] Build dir: !BUILD_DIR!
echo.

if not exist "!BUILD_DIR!\CMakeCache.txt" (
  echo [ERROR] Build folder is not configured.
  echo        Run scripts\cmake\cmake_configure.bat first.
  goto :endfail
)

cmake --build "!BUILD_DIR!" --config Debug --parallel
set "EC=!ERRORLEVEL!"
echo.
if not "!EC!"=="0" (
  echo [ERROR] Build failed with exit code !EC!.
  goto :endfail
)

echo [OK] Debug build completed successfully.
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
