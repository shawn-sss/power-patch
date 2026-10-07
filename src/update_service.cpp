#include "update_service.h"
#include "windows_update/windows_update.h"
#include "store_update/store_update.h"
#include "m365_update/m365_update.h"
#include <QThread>

UpdateResult launchUpdate(UpdateKind kind, bool closeAfter)
{
#ifdef _WIN32
    switch (kind) {
    case UpdateKind::Windows: {
        if (areWindowsUpdatesDisabled())
            return {UpdateOutcome::Unavailable, "Windows Update service is disabled. Enable it or contact your administrator."};
        const bool opened = openWindowsUpdateSettings();
        if (opened)
            QThread::msleep(5000);
        const bool started = startWindowsUpdateScan();
        if (closeAfter && opened && started)
            closeWindowsUpdateWindowAfterDelay(5000);
        if (started)
            return {UpdateOutcome::Started, "Scan request sent. Check Windows Settings for download and installation progress."};
        if (opened)
            return {UpdateOutcome::Attention, "Settings opened. Select Check for updates to start the scan manually."};
        return {UpdateOutcome::Failed, "Could not open Windows Update or request a scan. Try Windows Settings."};
    }
    case UpdateKind::Store: {
        const auto status = queryMicrosoftStoreStatus();
        if (status == MicrosoftStoreStatus::Disabled)
            return {UpdateOutcome::Unavailable, "Microsoft Store is disabled by policy. Contact your administrator."};
        if (status == MicrosoftStoreStatus::Uninstalled)
            return {UpdateOutcome::Unavailable, "Microsoft Store is not installed or its launch protocol is unavailable."};
        if (!openMicrosoftStoreLibrary())
            return {UpdateOutcome::Failed, "Could not open Microsoft Store. Check that it is installed and enabled."};
        if (!clickMicrosoftStoreGetUpdates(closeAfter))
            return {UpdateOutcome::Attention, "Store opened. Select Check for updates in its Library to continue manually."};
        return {UpdateOutcome::Started, "Update action requested. Check the Store Library for download and installation progress."};
    }
    case UpdateKind::Office: {
        const auto status = queryMicrosoft365UpdateStatus();
        if (status == Microsoft365UpdateStatus::Disabled)
            return {UpdateOutcome::Unavailable, "Office updates are disabled in Click-to-Run settings. Contact your administrator."};
        if (status == Microsoft365UpdateStatus::NotInstalled)
            return {UpdateOutcome::Unavailable, "Microsoft 365 / Office Click-to-Run was not found. Other Office editions use their own updater."};
        if (!startMicrosoft365Update())
            return {UpdateOutcome::Failed, "Could not start Office Click-to-Run. Try Update Now from an Office application's Account page."};
        if (closeAfter && !closeMicrosoft365UpdateAfterCompletion(5000))
            return {UpdateOutcome::Attention, "Office updater started, but completion could not be confirmed. Its window was left open."};
        return {UpdateOutcome::Started, "Office updater launched. Verify the installed version in an Office application's Account page."};
    }
    }
#else
    Q_UNUSED(kind);
    Q_UNUSED(closeAfter);
#endif
    return {UpdateOutcome::Unavailable, "Update launching is only supported on Windows."};
}
