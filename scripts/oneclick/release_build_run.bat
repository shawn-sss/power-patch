@echo off
REM One-click: configure, build Release, run Release.
setlocal

pushd "%~dp0\..\.."

call "scripts\cmake\cmake_configure.bat"
if errorlevel 1 goto :fail

call "scripts\cmake\cmake_build_release.bat"
if errorlevel 1 goto :fail

call "scripts\run\run_release.bat"
if errorlevel 1 goto :fail

goto :done

:fail
echo.
echo [FAIL] One-click Release build+run failed.
echo        Fix the error above and try again.
echo.

:done
popd
pause
