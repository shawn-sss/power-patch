# Code structure and maintenance

## Application components

| Source | Responsibility |
| --- | --- |
| [main.cpp](../src/main.cpp) | Creates QApplication, assigns app identity/version/icon, replaces older instances, creates the settings directory, and shows the dashboard. |
| [startup_guard.cpp](../src/startup_guard.cpp) | Windows startup mutex, process-path/session/creation-time checks, forced replacement of older matching processes, and startup error reporting. |
| [dashboard.h](../src/dashboard.h), [dashboard.cpp](../src/dashboard.cpp) | Compact Qt Widgets UI, source selection, sequential launch queue, results/activity, preferences, theme, tray, and closing behavior. |
| [update_service.h](../src/update_service.h), [update_service.cpp](../src/update_service.cpp) | UI-independent `launchUpdate` dispatcher and typed outcomes: Started, Attention, Unavailable, Failed. |
| [windows_update](../src/windows_update/windows_update.cpp) | Windows Update service check, Settings launch, scan-command launch, and optional window closing. |
| [store_update](../src/store_update/store_update.cpp) | Store policy/protocol checks, Library launch, UI Automation update actions, and optional successful-action window closing. |
| [m365_update](../src/m365_update/m365_update.cpp) | Click-to-Run lookup/configuration checks, Office updater launch, and optional UI-based completion polling/window closing. |
| [app_resources.rc](../src/app_resources.rc) | Windows icon and executable version metadata. |

## Launch queue and lifetime

The dashboard accepts an injectable `Launcher` function. Production uses `launchUpdate`; tests supply simulated results or controlled blocking behavior.

Individual Run and batch Run use the same queue. Only one source launches at a time through `QtConcurrent::run`. Worker lambdas capture value data and the launcher, not widget pointers. A `QFutureWatcher<UpdateResult>` delivers completion to the GUI thread, which updates results, settings, activity, and the next queued source. Unexpected launcher exceptions become failed results.

`Stop queued` removes pending sources without terminating the current worker or external updater. Normal close either hides to the tray or stops pending sources and waits responsively for the current launch to return. Destruction also waits for outstanding workers. Startup replacement is intentionally different: Windows forcibly terminates the older process, so it does not execute normal cleanup.

## Preference persistence

Selections and preferences update the UI immediately. Select all blocks individual checkbox signals, changes the group, then refreshes controls once. A 200 ms single-shot timer combines preference changes; a separate worker-owned QSettings instance writes them. Saves are serialized. If another change arrives during a save, the timer retries with a fresh snapshot. Normal close/destruction waits for the active save and flushes pending current values.

Results and geometry use the dashboard's QSettings instance. Stored keys are:

| Key | Meaning / default |
| --- | --- |
| `enableWindowsUpdate`, `enableStoreUpdate`, `enableM365Update` | Batch selections; default true. |
| `closeUpdateWindows` | Auto-close preference; default false. |
| `trayOnClose` | Hide-to-tray preference; default false. |
| `lastResult/0`, `lastResult/1`, `lastResult/2` | Latest display result for Windows, Store, Office respectively. |
| `geometry` | Qt window-geometry bytes. |
| `legacyImported` | Marks completion of the one-time executable-adjacent boolean import. |
| `compactLayout` | Marks the one-time reset of legacy oversized geometry. |

Activity is an in-memory QPlainTextEdit document capped at 200 blocks; opening its separate modeless dialog does not start any updater. Copy writes its current text to the clipboard.

## UI and scaling

Source rows keep their result to one elided line. The QLabel still contains the full text for accessibility, and the tooltip exposes it visually. Preferences remain inline. Activity owns its own resizable dialog. The primary action area sits outside the source scroll area. Resize handling rearranges preferences/Activity and Run/Stop when their size hints no longer fit horizontally.

Default size is 560 x 440 logical pixels, nominal minimum 300 x 240, with the startup minimum reduced if the available screen is smaller. Main content width is capped at 760 pixels. Do not promise that every state fits without vertical scrolling at the minimum size. Validate layout at multiple scales and with real Windows monitors in addition to the offscreen tests.

## Testing and release maintenance

- [tests/dashboard_checks.cpp](../tests/dashboard_checks.cpp) exercises queue, persistence, controls, dialogs, clipboard, and selected responsive layouts without real updates.
- [tests/startup_checks.cpp](../tests/startup_checks.cpp) drives [startup_probe.cpp](../tests/startup_probe.cpp) to check process replacement without launching the real application.
- CMake supplies the Qt runtime path and per-case offscreen/scale environment. The source version is compiled into the application; Windows resource versions remain separately maintained.
- When changing labels, window behavior, settings, or startup, update the [user guide](user-guide.md), [regression checklist](update-flow-checks.md), and relevant tests. Refresh the user guide screenshot from the rendered test preview. Keep the [README](../README.md) limited to a short summary, setup, and documentation links.
- When changing dependencies, versioning, or extraction, update [packaging documentation](standalone-versioning.md) and verify the actual package on a clean machine. The current wrapper does not preserve the nested settings file on re-extraction.
