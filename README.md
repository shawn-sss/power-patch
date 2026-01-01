# Power Patch

<p align="center">
    <img src="screenshot.jpg" alt="Screenshot">
</p>

A small **native Windows desktop app** that kicks off updates for the **three big buckets** in one place:

- **Windows OS** (opens Windows Update + triggers a scan when available)
- **Microsoft Store apps** (opens the Store Library and attempts to press **Check for updates** automatically)
- **Microsoft 365 (Click-to-Run)** (launches OfficeC2RClient update)

> Free to use, download, and edit. A license file will be added soon.

---

## Why?

Keeping a Windows PC current often means bouncing between Settings, the Store, and Office’s updater.  
**Power Patch** is a quick launcher that starts those update checks with one click.

## Requirements

- **Windows 11**
- **Microsoft Store** installed/enabled (for Store app updates)
- **Microsoft 365 Apps / Office Click-to-Run** install (for M365 updates)
- Optional: **System tray** support (for “minimize to tray” behavior)

### Building from source

- **Visual Studio + MSVC** (the included scripts default to `Visual Studio 18 2026`)
- **CMake 3.21+**
- **Qt 6 (Widgets)** (scripts prefer **6.11.0**, fall back to **6.10.1**)

## Quick start

1. Clone/download the repo.
2. Run `scripts\oneclick\release_build_run.bat`  
   _(or do it step-by-step with the scripts in `scripts\cmake\` + `scripts\run\`)_.
3. In the app, choose **Run all updates** or run each bucket individually.

## Notes

- **Windows Update** can be restricted by **WSUS/MDM** and build capabilities.
- **Store updates** are UI-driven; if the Store UI changes, you may need to click manually.
- **Office updates** run only when **Click-to-Run** is detected.
- Optional toggles:
  - **Close update windows after starting updates**
  - **Send app to system tray when closed**

## Contributing

Issues/PRs welcome—especially around reliability across Windows versions, Store UI changes, and packaging improvements.
