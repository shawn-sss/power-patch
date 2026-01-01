@echo off
REM Delete build, dist, and install folders

setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."
call "scripts\cmake\project_settings.bat" --from-script

echo [Clean] Deleting folders:
echo         !BUILD_DIR!
echo         !DIST_DIR!
echo         !INSTALL_DIR!
echo.

for %%D in ("!BUILD_DIR!" "!DIST_DIR!" "!INSTALL_DIR!") do (
  if exist %%D (
    echo [Clean] Removing %%D ...
    rmdir /s /q %%D
  ) else (
    echo [Clean] Skipping %%D (not found)
  )
)

set "EC=!ERRORLEVEL!"
if not "!EC!"=="0" (
  echo.
  echo [ERROR] One or more folders could not be removed.
  goto :endfail
)

echo.
echo [OK] Cleanup complete.
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
