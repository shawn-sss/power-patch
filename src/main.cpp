#include "dashboard.h"
#include "startup_guard.h"
#include <QApplication>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Power Patch");
    app.setApplicationDisplayName("Power Patch");
    app.setOrganizationName("Power Patch");
    app.setApplicationVersion(POWERPATCH_VERSION);
    app.setFont(QFont("Segoe UI", 10));
    app.setWindowIcon(QIcon(":/icons/assets/img/powerpatch_1024.png"));
    StartupGuard startup;
    QString startupError;
    if (!startup.replacePreviousInstances(startupError)) {
        startup.release();
        QMessageBox::warning(nullptr, "Power Patch - Could not restart", startupError);
        return 1;
    }
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(directory);
    Dashboard window(directory + "/settings.ini");
    window.show();
    startup.release();
    return app.exec();
}
