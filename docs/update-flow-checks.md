# Update-flow regression checks

Build with the Qt Widgets and Concurrent modules installed, then check these flows on Windows. Running an update action starts the real updater, so use a suitable test PC or VM.

- Start each individual updater, and then Run selected updates. While startup is in progress, confirm that all update buttons, selection checkboxes, and the close-update-windows option are disabled. The busy indicator should be visible. Opening and closing the About dialog must not unlock the controls.
- During an active run, use Run from the tray. It must not launch a second run. The tray Open action and the main window should remain responsive.
- After each run returns to Ready, confirm the busy indicator disappears and buttons reflect the selected checkboxes. Uncheck all three selections: Run selected updates must be disabled. Recheck one: only that updater and Run selected updates should be enabled.
- On a VM where Microsoft Store is unavailable or disabled by policy, run Store updates and dismiss the warning. All eligible controls must become usable again after the short delay. Repeat for unavailable Microsoft 365 and disabled Windows Update.
- Exercise a failed Store launch and a Store launch where automatic clicking fails. Controls must recover after dismissing the message. A successful Store launch must stay busy until automation finishes.
- Run a selection where every selected updater is unavailable. After dismissing the warnings, controls must recover.
- Check light and dark themes and Windows display scaling. The busy indicator should fit without clipping the last update row. The About button should expose its name to accessibility tools.

The busy indicator covers starting update checks, not the download or installation of updates. Verify installation status in the Windows, Store, or Microsoft 365 updater.
