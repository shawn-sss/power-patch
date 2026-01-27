# Development Environment Setup

This document lists the exact tools, versions, and paths expected by the scripts so a new machine can be set up quickly and reproducibly.

## Required OS

- Windows 11 (x64)

## Required tools (exact expectations)

1) Visual Studio (MSVC)
   - VS 2022 with C++ toolset installed
   - The scripts locate VS via `vswhere.exe` and call `vcvars64.bat`
   - Default search path: `%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe`

2) CMake
   - Must be on `PATH`
   - Used by all build/package scripts

3) Qt 6 (MSVC 2022 x64)
   - Kit: `msvc2022_64`
   - Preferred version: `6.11.0`
   - Fallback version: `6.10.1`
   - Expected install root:
     - `C:\Qt\6.11.0\msvc2022_64`
     - `C:\Qt\6.10.1\msvc2022_64`
   - The scripts expect `Qt6Config.cmake` at:
     - `C:\Qt\<version>\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake`

4) NSIS (only for packaging standalone EXE)
   - `makensis.exe` on `PATH`, or
   - installed at `%ProgramFiles(x86)%\NSIS\makensis.exe`

## Project scripts and what they do

### One-click dev run

- `scripts/oneclick/run_debug_build.bat`
  - Runs:
    - `scripts/cmake/cmake_configure.bat`
    - `scripts/cmake/cmake_build_debug.bat`
    - `scripts/run/run_debug.bat`

### One-click packaging

- `scripts/oneclick/create_standalone_exe.bat`
  - Runs:
    - `scripts/release/build_standalone_exe.bat`
  - Output:
    - `dist\Portable Self-Extracting\powerpatch.exe`

## Paths and settings used by scripts

Defined in `scripts/cmake/project_settings.bat`:

- `CMAKE_GENERATOR=NMake Makefiles`
- `CMAKE_ARCH=x64`
- `QT_KIT=msvc2022_64`
- `QT_PREFERRED_VERSION=6.11.0`
- `QT_FALLBACK_VERSION=6.10.1`
- `QT_ROOT=C:\Qt\<version>\msvc2022_64`
- `QT6_DIR=<QT_ROOT>\lib\cmake\Qt6`
- `QT_BIN=<QT_ROOT>\bin`
- `BUILD_DIR=build`
- `DIST_DIR=dist`

## Common setup checklist (new PC)

1) Install Visual Studio 2022 with the Desktop C++ workload.
2) Install CMake and confirm `cmake` is on `PATH`.
3) Install Qt 6.11.0 (or 6.10.1 fallback) for `msvc2022_64` into `C:\Qt`.
4) Install NSIS if you need standalone packaging.
5) Open a new terminal and run:
   - `scripts\oneclick\run_debug_build.bat`
6) For packaging, run:
   - `scripts\oneclick\create_standalone_exe.bat`

## Troubleshooting

- If the scripts fail to find Qt, confirm:
  - The Qt version folder exists under `C:\Qt\`
  - `Qt6Config.cmake` exists under `lib\cmake\Qt6`
- If `nmake` is missing, ensure VS 2022 C++ tools are installed and that `vcvars64.bat` is available.
- If packaging fails, confirm `makensis.exe` is installed and on `PATH`.
