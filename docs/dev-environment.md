# Development environment

These instructions describe the repository's current configuration and scripts, reviewed on 2026-10-07. Run commands from the repository root unless stated otherwise.

## Toolchain

| Tool | Project requirement / script behavior |
| --- | --- |
| Windows | Windows 11 x64. The updater implementation and startup replacement tests use Windows APIs. |
| Visual Studio / MSVC | Desktop C++ workload and a Windows SDK. Scripts use `vswhere.exe -latest` and `VC\Auxiliary\Build\vcvars64.bat`, rather than pinning a Visual Studio release. The latest local validation used Visual Studio 18 Community. |
| CMake | 3.22 or newer, on PATH. The minimum is declared in [CMakeLists.txt](../CMakeLists.txt). |
| Qt | Qt 6 Widgets and Concurrent, with the `msvc2022_64` kit. Scripts prefer `C:\Qt\6.11.0\msvc2022_64`, then `C:\Qt\6.10.1\msvc2022_64`. Local validation used 6.10.1. |
| NSIS | Only for the self-extracting wrapper. The script accepts `NSIS_BIN` as the full path to `makensis.exe`, otherwise searches PATH and `%ProgramFiles(x86)%\NSIS\makensis.exe`. |

The batch defaults live in [project_settings.bat](../scripts/cmake/project_settings.bat): `NMake Makefiles`, `build`, `dist`, and `install`. For a different Qt location, use explicit CMake commands or edit those defaults. The scripts overwrite their Qt path variables; setting a different `QT_ROOT` in your shell is not an override.

## Configure and build directly

Open an **x64 Native Tools Command Prompt** for the installed Visual Studio version. This supplies MSVC, the Windows SDK, and NMake. Substitute your Qt version/path as needed.

```bat
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="C:\Qt\6.10.1\msvc2022_64\lib\cmake\Qt6" -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure -C Release
scripts\run\run_release.bat
```

With the default single-configuration NMake generator, build type is chosen at **configure time**. `--config Debug` does not turn a Release NMake tree into Debug. For an actual Debug build, use a separate directory:

```bat
cmake -S . -B build-debug -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DQt6_DIR="C:\Qt\6.10.1\msvc2022_64\lib\cmake\Qt6" -DBUILD_TESTING=ON
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure -C Debug
set "PATH=C:\Qt\6.10.1\msvc2022_64\bin;%PATH%"
build-debug\powerpatch.exe
```

The existing run scripts use `build`, not `build-debug`. NMake ignores the scripts' `--parallel` flag. Configure with `-DBUILD_TESTING=OFF` to omit test executables. No Qt Test module is required.

Close all copies of the target executable before rebuilding it; Windows can lock the file and produce `LNK1104`. Startup replacement happens when the app runs, not before the linker writes an executable.

## Batch script reference

Most scripts pause before returning, including when invoked by the one-click scripts. Use direct CMake commands for unattended development.

| Script (under `scripts/`) | Actual behavior |
| --- | --- |
| `cmake/project_settings.bat` | Prints defaults; `--from-script` sets variables in its caller without pausing. |
| `cmake/cmake_configure.bat` | **Deletes `build`**, then configures a Release NMake tree. |
| `cmake/cmake_reconfigure.bat` | Requires an existing `build` directory and reconfigures it as Release without deleting it. |
| `cmake/cmake_build_release.bat` | Builds the existing tree with `--config Release`. |
| `cmake/cmake_build_debug.bat` | Builds the existing tree with `--config Debug`; it does not change NMake's configured build type. |
| `run/run_release.bat`, `run/run_debug.bat` | Add the selected Qt bin directory to PATH and run the corresponding configuration subdirectory, falling back to `build\powerpatch.exe`. |
| `oneclick/run_debug_build.bat` | Calls configure, Debug build, then Debug run. Despite its name, its configure step currently selects **Release** and deletes `build`. Use the direct Debug commands above for debug symbols/behavior. |
| `cmake/cmake_install_release.bat` | Deletes `install`, then installs the app executable there; this alone does not deploy Qt runtime dependencies. |
| `cmake/cmake_clean_build.bat` | Deletes `build`. |
| `cmake/cmake_clean_all.bat` | Deletes `build`, `dist`, and `install`. Custom directories such as `build-debug` are not included. |
| `release/build_package.bat` | Builds, installs, deploys Qt with `windeployqt`, and attempts a ZIP. Supports `--reconfigure` and `--no-pause`. |
| `release/build_standalone_exe.bat` | Deletes `dist`, builds a release bundle, minimizes a copied payload, then compiles an NSIS wrapper. Its `--skip-package` option currently cannot reliably reuse a bundle because `dist` is deleted first. |
| `oneclick/create_standalone_exe.bat` | Calls the standalone builder and pauses. |
| `release/minimize_qt_build.bat` | Removes selected runtime modules, plugins, translations, and debug files from the supplied payload directory. |
| `release/generate_package_list.bat` | Writes a recursive package listing to `package_list.txt` in the supplied payload directory. |

See [packaging and versioning](standalone-versioning.md) before distributing an artifact.

## Tests and previews

`BUILD_TESTING` defaults to ON. On Windows, CTest registers `startup_checks`, `dashboard_checks`, `dashboard_checks_150`, and `dashboard_checks_200`. CTest supplies the Qt DLL directory through PATH and uses the offscreen Qt platform for dashboard checks. No desktop interaction or real updater launch is required.

Dashboard previews are saved under the active build directory in `previews`, `previews-150`, and `previews-200`. The user guide image is copied from `previews/dashboard-dark.png`; regenerate and refresh `docs/images/dashboard.png` after UI changes. The root `screenshot.jpg` is a legacy asset and is no longer used in the documentation.

See [the test checklist](update-flow-checks.md) for coverage and manual Windows checks.

## Troubleshooting

- **Qt6Config.cmake not found:** check `lib\cmake\Qt6` under the selected kit and use an explicit `-DQt6_DIR` if the scripts' defaults do not apply.
- **NMake, compiler, or Windows headers missing:** use the x64 developer prompt or repair the C++ workload/SDK. Most scripts search `vswhere.exe` under Program Files (x86), then Program Files.
- **Qt DLL or platform plugin missing:** use the run script with the matching Qt kit, or distribute the complete `windeployqt` package. A raw build EXE is not standalone.
- **Preferences do not persist in tests:** tests create files in the system temporary directory. Restricted execution environments must allow those files and QSettings lock/atomic-save operations.
- **CMake reports a generator mismatch:** use a different build directory for a different generator; do not reuse a configured NMake tree as a Visual Studio tree.
- **Standalone compilation fails:** inspect `dist\Build Logs\powerpatch_standalone_build.log` and `powerpatch_standalone_nsis.log`; verify `NSIS_BIN` or NSIS discovery.
