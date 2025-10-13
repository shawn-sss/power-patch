# Power Patch

<p align="center">
  <img src="screenshot.png" alt="Screenshot">
</p>

A tiny PowerShell GUI that updates the **three big buckets** on Windows:
- **Windows OS** (via Windows Update API)
- **Microsoft Store apps** (via `winget`)
- **Microsoft 365 (Click-to-Run)**

> Free to use, download, and edit. MIT license file will be added soon.

---

## Why?
Keeping a Windows box current usually means hopping between Settings, the Store, and Office Updater.  
**Power Patch** runs them together with one click, optional verbose output, and an auto-reboot prompt.

## Requirements
- **Windows 10/11 (build 17763+) or later**
- **Windows PowerShell 5.1 (Desktop) or later**
- Administrator rights (UAC prompt on launch)
- For Store updates: **App Installer / `winget`**
- For Microsoft 365 updates: **Click-to-Run** installation

## Quick start
1. **Download/clone** the repo.
2. Run `src\start.bat` (or use the provided shortcut).
3. Pick what to update → **Run Selected** or **Run All**.

## Notes
- Respects WSUS/policy—if online updates are blocked, Windows Update may fail.
- Office updates only run when Click-to-Run is detected.
- The app can prompt to **restart** when required (optional auto-reboot).

## Contributing
The contributing process may open up more in the near future.
