@echo off
REM One-click script to build self-extracting standalone executable
setlocal

pushd "%~dp0\..\.."

echo [OneClick] Building standalone executable...
echo.

call "scripts\release\build_standalone_exe.bat"
if errorlevel 1 goto :fail

goto :done

:fail
echo.
echo [FAIL] Standalone build failed.
echo.
goto :end

:done
echo.
echo [OK] Build completed successfully!
echo      Standalone exe: dist\Portable Self-Extracting\Power Patch.exe
echo.

:end
popd
pause
