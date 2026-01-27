# Standalone Versioning + File Checks

This doc describes how the standalone self-extracting EXE decides whether to reuse an existing AppData install or re-extract, and which files are considered critical.

## Install location and settings

- Install root: `%LOCALAPPDATA%\Power Patch`
- User settings file: `settings.ini` in the install root
- Version file: `version.txt` in the install root

## Launch/extraction flow (standalone EXE)

Defined in `scripts/release/standalone.nsi.template`.

1) On launch, the standalone EXE reads `version.txt` from the install folder.
2) If the version matches the EXE's embedded version, it checks for critical files.
3) If version + files are valid, it launches the app directly.
4) Otherwise it:
   - Copies `settings.ini` to `%TEMP%\powerpatch_settings_backup.ini` if present
   - Deletes the install folder
   - Extracts the payload into the install folder
   - Restores `settings.ini`
   - Writes the new `version.txt`
   - Launches the app

## Critical files checked by the installer

Defined in `scripts/release/standalone.nsi.template`:

- `@@EXE_NAME@@`
- `Qt6Core.dll`
- `Qt6Gui.dll`
- `Qt6Widgets.dll`
- `platforms\qwindows.dll`
- `styles\qmodernwindowsstyle.dll`

## Critical files checked by the app itself

Defined in `src/main.cpp` (only when running from AppData):

- `Qt6Core.dll`
- `Qt6Gui.dll`
- `Qt6Widgets.dll`
- `platforms\qwindows.dll`
- `styles\qmodernwindowsstyle.dll`

If any are missing, the app shows a “Critical files missing or corrupted” error and exits.

## Sync status (current)

The installer and the app check the same Qt runtime files. This keeps the “skip extraction” logic aligned with what the app expects at runtime.

## When to update this list

If you add or remove Qt plugins, styles, or other runtime DLLs that are required at startup:

- Update the file checks in `scripts/release/standalone.nsi.template`
- Update the runtime validation list in `src/main.cpp`

## When updating the app version

Update all of these to keep the version consistent across the app, metadata, and standalone packaging:

- `CMakeLists.txt` -> `project(powerpatch VERSION X.Y.Z ...)`
- `src/app_resources.rc` -> `FILEVERSION`, `PRODUCTVERSION`, and string values `FileVersion` / `ProductVersion`
- `src/main.cpp` -> About dialog text (`Power Patch vX.Y`)

The standalone EXE pulls its version from `CMakeLists.txt` in `scripts/release/build_standalone_exe.bat`, which then writes it into `version.txt` at install time.
