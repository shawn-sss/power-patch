# Regression checks

## Automated suite

Build with `BUILD_TESTING=ON`, Qt Widgets/Concurrent, and the Windows toolchain, then run:

```powershell
ctest --test-dir build --output-on-failure -C Release
```

For the separate Debug tree described in [development setup](dev-environment.md), use `--test-dir build-debug -C Debug`.

| CTest case | Coverage |
| --- | --- |
| `startup_checks` | Starts disposable `startup_probe` helper processes, replaces multiple older instances, checks repeated/concurrent launches, and verifies a same-named executable at another path survives. Does not kill real Power Patch instances. |
| `dashboard_checks` | Dashboard behavior and layout at 100% scale. |
| `dashboard_checks_150` | Same dashboard checks with `QT_SCALE_FACTOR=1.5`. |
| `dashboard_checks_200` | Same dashboard checks with `QT_SCALE_FACTOR=2`. |

Dashboard tests use simulated launchers and temporary settings. They cover empty selections, individual Run buttons, duplicate-click protection, workers running off the GUI thread, mixed outcomes, remembered results/preferences, queue stopping, retries, deferred close, exception recovery, and asynchronous preference saves while the settings file is deliberately locked. They verify that rapid selections save the final state and that Activity opens/closes, explains an empty session, and copies the log correctly. Preferences are checked directly on the main panel.

Layout checks cover selected sizes from 300 x 240 through 1920 x 1080 logical pixels, including narrow busy windows and completed results. Typical tested idle sizes must fit without scrolling; extremely short windows may scroll vertically. Run and Stop must remain visible and horizontal scrolling must not appear.

CTest configures Qt's DLL path and the offscreen platform. Dashboard tests explicitly load Segoe UI from the Windows Fonts directory for readable previews. Tests do not launch real Windows, Store, or Office updates. They do not cover NSIS extraction, real tray integration, Windows policy combinations, or every possible monitor/text-scale configuration.

## Preview artifacts

Each dashboard case writes to `build/previews`, `build/previews-150`, or `build/previews-200`:

- `dashboard-light.png`, `dashboard-dark.png`: initial panel.
- `dashboard-compact.png`: narrow panel.
- `dashboard-results.png`: completed simulated results with inline preferences.
- `dashboard-busy-small.png`: a small window during a simulated launch.
- `activity-empty.png`, `activity-populated.png`: the separate Activity window.
- `preferences.png`: the inline preference area.
- `dashboard-expanded.png`: a legacy filename for a main-window capture while Activity is open; it no longer depicts expandable inline panels.

Refresh the user guide's `docs/images/dashboard.png` from the current `dashboard-dark.png` after UI changes. Do not present simulated preview results as real updater results.

## Manual Windows checks

Use a suitable test PC or VM for actions that launch real updaters. Record the OS build, scale, theme, installation types, and tested outcomes.

### Selection and launch controls

- Launch each source individually, then run a batch. The batch order is Windows, Store, Office. Unselected sources are skipped by batch Run but their individual Run buttons remain available.
- Uncheck all sources: batch Run and tray Run must disable. Select all and Ctrl+Enter must start the selected batch. Repeated selection clicks must update immediately without lag.
- During a launch, source checkboxes, update buttons, auto-close, and tray Run must be disabled. About, Activity, and the main window remain responsive; opening About must not unlock controls.
- Verify the progress bar counts finished launch attempts only. It disappears when the run finishes; the final summary and per-source results remain.

### Results and manual intervention

- Test disabled Windows Update service, policy-disabled/missing Store, and missing or disabled Office Click-to-Run. Each should produce a specific result and allow other queued sources to proceed without a blocking result dialog.
- If Store automation fails after opening Store, verify Action needed and leave Store open even when auto-close is enabled. Repeat for a failed Windows scan trigger after Settings opens.
- Test Office with auto-close off and on. When its UI-based completion cannot be confirmed, report Action needed and leave its window open. Its polling can run for about 30 minutes; UI responsiveness alone is not evidence of a stuck worker.
- Hover over shortened results for complete text. Confirm the message states request/launch outcomes rather than claiming successful installation.

### Stop, close, tray, and repeat launch

- Stop a batch during its first source. Pending sources must show Skipped and never start. The current operation continues. Retry a source after the operation returns.
- With tray behavior off, close during a batch. The queue stops, the active operation finishes, then the app exits.
- With tray behavior on, close: main and Activity windows hide, but the batch continues. Tray Open restores the dashboard. Tray Exit stops queued launches and waits for the active operation.
- Check native tray menu keyboard navigation and the disabled Run item during a batch. If there is no system tray, the tray preference must be disabled.
- Launch the same executable again while visible, in the tray, and during a batch. The older process must be force-closed; the new dashboard must start. Already-launched updater processes continue. This deliberately differs from normal deferred close.
- Launch several copies rapidly and verify one matching app remains. Verify another executable path and another Windows session are not terminated. If an older instance cannot be inspected/terminated, verify the startup error and absence of a new dashboard.

### Preferences, Activity, and persistence

- Both preferences must be visible without opening another window. Toggle them, exit, and restart; selections and preferences must persist. Change a selection and immediately close to check pending saves are flushed.
- Activity starts with an explanation and Copy disabled. After a run, Copy must copy the full chronological session log, including skipped sources. Open/close/reopen Activity without moving sources or changing the main window's size. Closing the main window must hide or close Activity too.
- Restart: latest per-source results persist, while Activity starts empty. The session log is bounded to 200 entries.
- With clean user settings, place a legacy boolean `settings.ini` beside the EXE. Recognized preferences import once and must not replace newer stored preferences. Run from a read-only app directory and confirm Local AppData settings still save.
- Make the current settings location unwritable and verify the save warning. Do not confuse the legacy executable-adjacent settings file with the current Local AppData file.

### Layout and release checks

- Check live light/dark changes, keyboard focus, display scaling, long result tooltips, and narrow/short windows. Preferences and Activity controls may rearrange; horizontal scrolling must not appear.
- Restore a window saved on a disconnected monitor. Confirm it fits the available screen at startup. Check real multi-monitor behavior separately from offscreen tests.
- Follow the separate [packaging checklist](standalone-versioning.md#release-checklist) for clean deployment, version reuse, extraction, and upgrade behavior.

Power Patch checks launch outcomes. Verify download and installation status in the actual updater; the automated suite does not establish that any real update installed successfully.
