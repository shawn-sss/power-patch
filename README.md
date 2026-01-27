# Power Patch

<p align="center">
    <img src="screenshot.jpg" alt="Screenshot">
</p>

A **native Windows desktop app** that kicks off updates for the **three big buckets** from one place:

- **Windows OS** (opens Windows Update + triggers a scan when available).
- **Microsoft Store apps** (opens Library and auto-clicks **Check for updates**).
- **Microsoft 365** (launches OfficeC2RClient update).

---

## Why?

Keeping a Windows PC current often means bouncing between Settings, the Store, and M365's updater.  
**Power Patch** is a quick and easy launcher that starts those update checks with one click.

## Requirements

- Developed on **Windows 11 25H2**.
- **Microsoft Store** installed/enabled (for Store app updates).
- **Microsoft 365 Apps / Office Click-to-Run** install (for M365 updates).

### Building from source

- **Windows 11 (x64)**
- **Visual Studio 2022 + MSVC**
- **Qt 6**
- **CMake**
- **NSIS**

## Quick start

1. Launch Power Patch.
2. If Windows shows a SmartScreen warning, choose **More info** → **Run anyway**.
3. **Select which updates to run**: Check or uncheck each update type.
4. Click **Run selected updates** to run all checked updates, or use individual buttons.
5. Optional settings:
   - **Close update windows after starting updates** - Auto-closes update windows after initiating.
   - **Send app to system tray when closed** - Keeps the app running in the system tray instead of fully closing.

## Notes

- **Windows Update** can be restricted by **WSUS/MDM** and build capabilities.
- **Store updates** are UI-driven and may break if Microsoft changes the Store interface.
- **Office updates** run only when **Click-to-Run** is detected.
