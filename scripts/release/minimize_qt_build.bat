@echo off
REM Removes unnecessary Qt files from windeployqt output
if "%~1"=="" (
  echo Usage: %~nx0 path\to\payload
  exit /b 1
)

setlocal EnableDelayedExpansion
set "PAYLOAD_DIR=%~1"

for %%G in (pdb qmlc qmltypes prl libqgc) do (
  for /r "%PAYLOAD_DIR%" %%F in (*.%%G) do (
    del /f /q "%%F" >nul 2>&1
  )
)

for %%D in (Qt6Svg.dll Qt6Pdf.dll Qt6Network.dll Qt6OpenGL.dll Qt6QmlModels.dll Qt6Quick.dll Qt6Qml.dll opengl32sw.dll D3Dcompiler_47.dll dxcompiler.dll dxil.dll) do (
  if exist "%PAYLOAD_DIR%\%%D" (
    del /f /q "%PAYLOAD_DIR%\%%D" >nul 2>&1
    echo [MinimizeQt]   Removed %%D
  )
)

for %%D in (networkinformation sqldrivers generic) do (
  if exist "%PAYLOAD_DIR%\%%D" (
    rmdir /s /q "%PAYLOAD_DIR%\%%D" >nul 2>&1
    echo [MinimizeQt]   Removed %%D folder
  )
)

if exist "%PAYLOAD_DIR%\imageformats" (
  for %%F in (qgif.dll qicns.dll qtga.dll qtiff.dll qwbmp.dll qwebp.dll) do (
    if exist "%PAYLOAD_DIR%\imageformats\%%F" (
      del /f /q "%PAYLOAD_DIR%\imageformats\%%F" >nul 2>&1
      echo [MinimizeQt]   Removed imageformats\%%F
    )
  )
)

if exist "%PAYLOAD_DIR%\translations" (
  rmdir /s /q "%PAYLOAD_DIR%\translations" >nul 2>&1
  echo [MinimizeQt]   Removed translations folder
)

if exist "%PAYLOAD_DIR%\icuin*.dll" (
  del /f /q "%PAYLOAD_DIR%\icuin*.dll" >nul 2>&1
  echo [MinimizeQt]   Removed ICU data DLLs
)
if exist "%PAYLOAD_DIR%\icudt*.dll" (
  del /f /q "%PAYLOAD_DIR%\icudt*.dll" >nul 2>&1
  echo [MinimizeQt]   Removed ICU data DLLs
)

echo [MinimizeQt] Removed debug helpers and unused modules from "%PAYLOAD_DIR%".
