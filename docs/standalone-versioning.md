# Packaging and versioning

This describes the current scripts and their limitations, reviewed on 2026-10-07. A successful build or automated test run does not establish that packaging, extraction, or upgrades work on a clean PC.

## Outputs and commands

From the repository root, with the toolchain in [development setup](dev-environment.md):

```bat
scripts\release\build_package.bat --no-pause --reconfigure
```

This configures Release, builds, installs the app into `dist\Build Package\powerpatch\bin`, and runs the selected Qt kit's `windeployqt`. It attempts to create `dist\Build Package\powerpatch_Release.zip`. ZIP failure is a warning: verify the ZIP actually exists before distributing it. Distribute the complete package, not just its EXE. Test executables are not installed into the package.

For the self-extracting wrapper:

```bat
scripts\oneclick\create_standalone_exe.bat
```

The wrapper builder deletes the existing `dist` tree first, recreates the release bundle, mirrors it into `dist\Build Artifacts\payload`, minimizes selected Qt files, generates `package.nsi`, and invokes NSIS. Output is `dist\Portable Self-Extracting\powerpatch.exe`. It also generates `payload\package_list.txt` as a build reference; extraction includes `payload\bin`, not that top-level listing.

Logs are under `dist\Build Logs`:

- `powerpatch_standalone_build.log`
- `powerpatch_standalone_nsis.log`

The wrapper's `--skip-package` option is currently unsuitable for reusing an existing bundle: the script deletes `dist` before processing that option. Use the full build path. The wrapper script pauses, even though its nested package build uses `--no-pause`.

The payload minimizer removes selected DLLs/plugins and translations. Recheck deployment dependencies when adding Qt functionality; do not assume a previously minimized runtime supports new features.

## Runtime paths

| Item | Location |
| --- | --- |
| Extracted executable and Qt runtime | `%LOCALAPPDATA%\Power Patch` |
| Wrapper's extraction version marker | `%LOCALAPPDATA%\Power Patch\version.txt` |
| Current app preferences and latest results | `%LOCALAPPDATA%\Power Patch\Power Patch\settings.ini` |
| Legacy preferences imported once by the app | `settings.ini` beside the actual app executable; for this wrapper, `%LOCALAPPDATA%\Power Patch\settings.ini` |
| Wrapper's legacy backup | `%TEMP%\powerpatch_settings_backup.ini` |

The self-extracting artifact is not fully portable: it extracts into Local AppData and launches from there. The template does not create shortcuts or an uninstaller.

## Actual extraction flow

The source of truth is [standalone.nsi.template](../scripts/release/standalone.nsi.template).

1. Read `version.txt` from the extraction directory and remove trailing CR/LF.
2. If it matches the embedded version, check for the executable and the following files:
   - `Qt6Core.dll`
   - `Qt6Gui.dll`
   - `Qt6Widgets.dll`
   - `platforms\qwindows.dll`
   - `styles\qmodernwindowsstyle.dll`
3. If those existence checks pass, launch the existing extracted app without refreshing files.
4. Otherwise, copy a legacy root-level `settings.ini`, if present, to the temporary backup, recursively remove the extraction directory, and recreate it.
5. Restore the temporary legacy settings backup if it exists, extract `payload\bin\*.*`, write the new `version.txt`, and launch the app.

The checks test **file existence**, not hashes, integrity, or the complete dependency set. `Qt6Concurrent.dll` is an application dependency but is not on the wrapper's skip-extraction checklist. The application no longer has the old AppData-only DLL validation block in `main.cpp`; there is no matching second checklist inside the app.

## Known extraction and upgrade limitations

- **Current preferences are not preserved by re-extraction.** The current settings file is nested inside the extraction root, which the wrapper deletes recursively. Only the legacy root-level file is backed up. Back up `%LOCALAPPDATA%\Power Patch\Power Patch\settings.ini` outside the extraction root before any re-extraction, and restore it after the app has fully exited. Do not describe current upgrades as preserving all settings.
- **Replacing files while the app runs is not coordinated.** The app's startup guard executes after extraction, when the inner EXE starts. It cannot unlock existing files before the wrapper removes or overwrites them. Exit all extracted copies before upgrading or repairing.
- **The temporary backup filename is shared.** The template has no per-launch extraction lock or unique backup name. Avoid running multiple wrapper extractions at once; stale backup state can also affect restoration.
- **Same-version builds can reuse old code.** If `version.txt` and the file-existence checks match, a newly built wrapper does not replace the installed executable. Increment the release version when distributing changed payloads.
- **This is not a transactional installer.** The template does not validate every delete/copy/extract operation or roll back a failed upgrade. Verify upgrades and recovery manually on a test machine.

These are limitations of the current release scripts, not guarantees provided by the application. This documentation review did not change the packaging implementation.

## App startup versus wrapper startup

Once the inner application runs, [startup_guard.cpp](../src/startup_guard.cpp) serializes startup and force-closes older processes with the same executable path in the same Windows session. It excludes itself, newer launches, other paths, and other sessions. It waits up to 15 seconds for the startup mutex and up to 5 seconds for each terminated process to exit. Failure stops startup with a message.

This affects the actual app process, not the self-extracting wrapper's file operations. It also does not terminate Windows, Store, or Office updaters. See the [user guide](user-guide.md#launching-again) for the user-visible forced-restart behavior.

## Version sources

The current source version is `1.0.1`. Update these together for a release:

1. [CMakeLists.txt](../CMakeLists.txt): `project(powerpatch VERSION X.Y.Z ...)`.
2. [src/app_resources.rc](../src/app_resources.rc): numeric `FILEVERSION` / `PRODUCTVERSION` and string `FileVersion` / `ProductVersion`.

CMake defines `POWERPATCH_VERSION`; `main.cpp` assigns it as the application version and the dashboard reads it for About. No hard-coded About version needs editing.

`build_standalone_exe.bat` extracts the third token of the CMake `project(... VERSION ...)` line and substitutes it into the NSIS template. Keep that declaration compatible with this simple parser and inspect the generated `APP_VERSION`; the script silently falls back to `0.1.0` if parsing fails. The wrapper writes that value to `version.txt`.

## Release checklist

1. Update version sources and docs/screenshots for the release.
2. Build and pass all [automated checks](update-flow-checks.md), then complete the relevant manual updater tests.
3. Build the package and wrapper, inspect their logs and actual outputs, and retain artifacts outside `dist` if needed before another wrapper build.
4. On a test Windows account without Qt on PATH, launch the deployed folder and wrapper. Check runtime dependencies and icons.
5. Test first extraction, same-version reuse, changed-version extraction, and missing-runtime recovery. Back up current settings and verify their state after each case; the current template does not preserve them automatically on re-extraction.
6. Verify About, executable version metadata, generated NSIS `APP_VERSION`, and installed `version.txt` agree. Confirm the installed executable contains the intended changes.
7. Test repeat launches with the app visible and in the tray. Verify only the intended instance is replaced.
