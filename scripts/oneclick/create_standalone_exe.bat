@echo off
REM One-click script to build self-extracting standalone executable
setlocal

pushd "%~dp0\..\.."

echo [OneClick] Building standalone executable...
echo.

call "scripts\release\build_standalone_exe.bat" --no-pause %*
if errorlevel 1 goto :fail

goto :done

:fail
set "BUILD_RESULT=1"
echo.
echo [FAIL] Standalone build failed.
echo.
goto :end

:done
set "BUILD_RESULT=0"
echo.
echo [OK] Build completed successfully!
echo      Standalone exe: dist\Portable Self-Extracting\powerpatch.exe
echo.

:end
popd
for %%A in (%*) do if /i "%%~A"=="--no-pause" exit /b %BUILD_RESULT%
pause
exit /b %BUILD_RESULT%
