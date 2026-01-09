@echo off
REM One-click script to configure, build, and run Debug
setlocal

pushd "%~dp0\..\.."

call "scripts\cmake\cmake_configure.bat"
if errorlevel 1 goto :fail

call "scripts\cmake\cmake_build_debug.bat"
if errorlevel 1 goto :fail

call "scripts\run\run_debug.bat"
if errorlevel 1 goto :fail

echo.
echo [OK] Debug build+run completed successfully.
popd
endlocal
pause
exit /b 0

:fail
echo.
echo [FAIL] One-click Debug build+run failed.
popd
endlocal
pause
exit /b 1
