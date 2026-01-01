#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QMessageBox>
#include <QCheckBox>
#include <QTimer>
#include <QIcon>
#include <QPixmap>
#include <QPalette>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QSize>
#include <thread>

class TrayCloseFilter : public QObject
{
public:
    TrayCloseFilter(QWidget *window,
                    const QCheckBox *trayOnCloseCheck,
                    bool *allowQuit,
                    QSystemTrayIcon *trayIcon,
                    QApplication *app,
                    QObject *parent = nullptr)
        : QObject(parent),
          window_(window),
          trayOnCloseCheck_(trayOnCloseCheck),
          allowQuit_(allowQuit),
          trayIcon_(trayIcon),
          app_(app)
    {
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (obj == window_ && event->type() == QEvent::Close) {
            if (allowQuit_ && *allowQuit_)
                return QObject::eventFilter(obj, event);
            if (trayOnCloseCheck_ && trayOnCloseCheck_->isChecked()) {
                window_->hide();
                event->ignore();
                return true;
            }
            if (allowQuit_)
                *allowQuit_ = true;
            if (trayIcon_)
                trayIcon_->hide();
            if (app_)
                app_->quit();
            return QObject::eventFilter(obj, event);
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QWidget *window_;
    const QCheckBox *trayOnCloseCheck_;
    bool *allowQuit_;
    QSystemTrayIcon *trayIcon_;
    QApplication *app_;
};

#include "constants.h"
#include "m365_update/m365_update.h"
#include "store_update/store_update.h"
#include "windows_update/windows_update.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const char *kIcon1024Resource = ":/icons/assets/powerpatch_icon_1024.png";
    const char *kIcon2048Resource = ":/icons/assets/powerpatch_icon_2048.png";
    const char *kMasterIconResource = ":/icons/assets/powerpatch_master.png";
    QIcon appIcon;
    appIcon.addFile(kIcon1024Resource, QSize(1024, 1024));
    appIcon.addFile(kIcon2048Resource, QSize(2048, 2048));
    if (appIcon.isNull()) {
        appIcon.addFile("assets/powerpatch_icon_1024.png", QSize(1024, 1024));
        appIcon.addFile("assets/powerpatch_icon_2048.png", QSize(2048, 2048));
    }
    app.setWindowIcon(appIcon);

    QWidget window;
    window.setWindowTitle("Power Patch");
    const bool darkMode = app.palette().color(QPalette::Window).lightness() < 128;
    if (darkMode) {
        window.setStyleSheet(
            "QWidget { background-color: #1b1f24; }"
            "QLabel { color: #e6edf3; }"
            "QLabel#subtitleLabel { color: #9aa4b2; }"
            "QLabel#statusLabel { color: #c1c7d0; }"
            "QLabel#appIcon { background-color: #242a31; border: 1px solid #3a424c; border-radius: 8px; }"
            "QPushButton { color: #e6edf3; background-color: #242a31; border: 1px solid #3a424c; border-radius: 6px; padding: 6px 10px; }"
            "QPushButton:hover { border-color: #55606e; }"
            "QPushButton:disabled { color: #6b7480; }"
            "QCheckBox { color: #e6edf3; }");
    } else {
        window.setStyleSheet(
            "QWidget { background-color: #f7f8fa; }"
            "QLabel { color: #1f2328; }"
            "QLabel#subtitleLabel { color: #5a6470; }"
            "QLabel#statusLabel { color: #47505a; }"
            "QLabel#appIcon { background-color: #ffffff; border: 1px solid #d0d6dd; border-radius: 8px; }"
            "QPushButton { color: #1f2328; background-color: #ffffff; border: 1px solid #d0d6dd; border-radius: 6px; padding: 6px 10px; }"
            "QPushButton:hover { border-color: #aeb6bf; }"
            "QPushButton:disabled { color: #8a929b; }"
            "QCheckBox { color: #1f2328; }");
    }
    QWidget *windowPtr = &window;

    auto *mainLayout = new QVBoxLayout(&window);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto *titleLabel = new QLabel("Power Patch");
    {
        QFont f = titleLabel->font();
        f.setPointSize(17);
        f.setBold(true);
        titleLabel->setFont(f);
    }

    QPixmap appIconPixmap(kMasterIconResource);
    if (appIconPixmap.isNull()) {
        appIconPixmap.load(kIcon1024Resource);
    }
    if (appIconPixmap.isNull()) {
        appIconPixmap.load("assets/powerpatch_master.png");
    }
    if (appIconPixmap.isNull()) {
        appIconPixmap.load("assets/powerpatch_icon_1024.png");
    }
    window.setWindowIcon(appIcon);

    auto *iconLabel = new QLabel();
    iconLabel->setObjectName("appIcon");
    iconLabel->setFixedSize(40, 40);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(appIconPixmap.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    auto *subtitleLabel = new QLabel("Windows, Microsoft Store, and Microsoft 365 updates");
    subtitleLabel->setObjectName("subtitleLabel");
    subtitleLabel->setWordWrap(true);

    auto *statusLabel = new QLabel("Ready");
    statusLabel->setObjectName("statusLabel");
    statusLabel->setWordWrap(true);

    auto *closeUpdateWindowsCheck = new QCheckBox("Close update windows after starting updates");
    closeUpdateWindowsCheck->setChecked(true);

    auto *trayOnCloseCheck = new QCheckBox("Send app to system tray when closed");
    trayOnCloseCheck->setChecked(true);

    auto *allUpdateButton = new QPushButton("Run all updates");
    allUpdateButton->setMinimumHeight(36);

    auto *winUpdateButton = new QPushButton("Check Windows updates");
    winUpdateButton->setDefault(true);
    winUpdateButton->setMinimumHeight(34);

    auto *storeUpdateButton = new QPushButton("Update Microsoft Store apps");
    storeUpdateButton->setMinimumHeight(34);

    auto *m365UpdateButton = new QPushButton("Update Microsoft 365 (Office)");
    m365UpdateButton->setMinimumHeight(34);

    QSystemTrayIcon *trayIcon = nullptr;
    bool allowQuit = false;
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        app.setQuitOnLastWindowClosed(false);
        trayIcon = new QSystemTrayIcon(appIcon, &window);
        trayIcon->setToolTip("Power Patch");

        auto *trayMenu = new QMenu(&window);
        auto *openAction = trayMenu->addAction("Open");
        auto *aboutAction = trayMenu->addAction("About");
        trayMenu->addSeparator();
        auto *runAllAction = trayMenu->addAction("Run updates");
        trayMenu->addSeparator();
        auto *exitAction = trayMenu->addAction("Exit");

        QObject::connect(openAction, &QAction::triggered, [&] {
            window.show();
            window.raise();
            window.activateWindow();
        });
        QObject::connect(aboutAction, &QAction::triggered, [&] {
            QMessageBox aboutBox(&window);
            aboutBox.setWindowTitle("About Power Patch");
            aboutBox.setText("Power Patch\nQuick update launcher for Windows.");
            aboutBox.setIconPixmap(appIconPixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            aboutBox.exec();
        });
        QObject::connect(runAllAction, &QAction::triggered, [&] {
            allUpdateButton->click();
        });
        QObject::connect(exitAction, &QAction::triggered, [&] {
            allowQuit = true;
            trayIcon->hide();
            window.close();
            app.quit();
        });
        QObject::connect(trayIcon, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                window.show();
                window.raise();
                window.activateWindow();
            }
        });

        trayIcon->setContextMenu(trayMenu);
        trayIcon->show();
        window.installEventFilter(new TrayCloseFilter(&window, trayOnCloseCheck, &allowQuit, trayIcon, &app, &window));
    } else {
        trayOnCloseCheck->setChecked(false);
        trayOnCloseCheck->setEnabled(false);
    }

    QObject::connect(winUpdateButton, &QPushButton::clicked, [&] {
        allUpdateButton->setEnabled(false);
        winUpdateButton->setEnabled(false);
        storeUpdateButton->setEnabled(false);
        m365UpdateButton->setEnabled(false);
        statusLabel->setText("Checking Windows updates...");

#ifdef _WIN32
        const bool closeAfter = closeUpdateWindowsCheck->isChecked();
        const bool scanOk = startWindowsUpdateScan();
        const bool uiOk = openWindowsUpdateSettings();

        if (closeAfter && uiOk) {
            std::thread([] {
                closeWindowsUpdateWindowAfterDelay(app_constants::kCloseWindowsUpdateDelayMs);
            }).detach();
        }

        if (!scanOk && !uiOk) {
            statusLabel->setText("Failed to start Windows Update");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Couldn't start a Windows Update scan or open the Windows Update settings page.\n\n"
                "This feature requires Windows 11 (or later) and access to the Settings app.");
        } else if (!scanOk && uiOk) {
            statusLabel->setText("Opened Windows Update (scan may not have started)");
            QMessageBox::information(
                windowPtr,
                "Power Patch",
                "Windows Update opened, but the scan trigger wasn't available.\n\n"
                "If it doesn't automatically start scanning, click \"Check for updates\" in the Settings window.");
        } else {
            statusLabel->setText("Windows Update scan started");
        }
#else
        statusLabel->setText("Unsupported platform");
        QMessageBox::warning(windowPtr, "Power Patch", "This feature is only supported on Windows.");
#endif

        QTimer::singleShot(app_constants::kReenableButtonsDelayMs, windowPtr, [allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton] {
            allUpdateButton->setEnabled(true);
            winUpdateButton->setEnabled(true);
            storeUpdateButton->setEnabled(true);
            m365UpdateButton->setEnabled(true);
        });
    });

    QObject::connect(m365UpdateButton, &QPushButton::clicked, [&] {
        allUpdateButton->setEnabled(false);
        winUpdateButton->setEnabled(false);
        storeUpdateButton->setEnabled(false);
        m365UpdateButton->setEnabled(false);
        statusLabel->setText("Checking Microsoft 365 updates...");

#ifdef _WIN32
        const bool closeAfter = closeUpdateWindowsCheck->isChecked();
        const bool ok = startMicrosoft365Update();
        if (!ok) {
            statusLabel->setText("Failed to start Microsoft 365 update");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Couldn't start Microsoft 365 (Office) updates.\n\n"
                "This requires a local Microsoft 365 Apps / Office Click-to-Run install.\n"
                "If you're using a different Office installation type, update it via its own updater or management tooling.");
        } else {
            statusLabel->setText("Microsoft 365 update started");
        }

        if (closeAfter && ok) {
            std::thread([] {
                closeWindowByProcessAfterDelay(L"OfficeC2RClient.exe", app_constants::kCloseOfficeDelayMs);
            }).detach();
        }
#else
        statusLabel->setText("Unsupported platform");
        QMessageBox::warning(windowPtr, "Power Patch", "This feature is only supported on Windows.");
#endif

        QTimer::singleShot(app_constants::kReenableButtonsDelayMs, windowPtr, [allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton] {
            allUpdateButton->setEnabled(true);
            winUpdateButton->setEnabled(true);
            storeUpdateButton->setEnabled(true);
            m365UpdateButton->setEnabled(true);
        });
    });

    QObject::connect(storeUpdateButton, &QPushButton::clicked, [&] {
        allUpdateButton->setEnabled(false);
        winUpdateButton->setEnabled(false);
        storeUpdateButton->setEnabled(false);
        m365UpdateButton->setEnabled(false);
        statusLabel->setText("Checking Microsoft Store app updates...");

#ifdef _WIN32
        const bool closeAfter = closeUpdateWindowsCheck->isChecked();
        std::thread([windowPtr, statusLabel, allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton, closeAfter] {
            const bool opened = openMicrosoftStoreLibrary();
            bool clicked = false;
            if (opened)
                clicked = clickMicrosoftStoreGetUpdates(closeAfter);

            QMetaObject::invokeMethod(windowPtr,
                                     [windowPtr, statusLabel, allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton, opened, clicked] {
                if (!opened) {
                    statusLabel->setText("Failed to open Microsoft Store");
                    QMessageBox::warning(
                        windowPtr,
                        "Power Patch",
                        "Couldn't open the Microsoft Store Library page.\n\n"
                        "Make sure Microsoft Store is installed and enabled on this PC.");
                } else if (!clicked) {
                    statusLabel->setText("Opened Store (couldn't click Get updates)");
                    QMessageBox::information(
                        windowPtr,
                        "Power Patch",
                        "Microsoft Store opened, but the app couldn't automatically click the \"Get updates\" button.\n\n"
                        "If updates don't start automatically, click \"Get updates\" in the Store Library.");
                } else {
                    statusLabel->setText("Microsoft Store update check started");
                }

                QTimer::singleShot(app_constants::kReenableButtonsDelayMs, windowPtr, [allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton] {
                    allUpdateButton->setEnabled(true);
                    winUpdateButton->setEnabled(true);
                    storeUpdateButton->setEnabled(true);
                    m365UpdateButton->setEnabled(true);
                });
            },
                                     Qt::QueuedConnection);
        }).detach();
#else
        statusLabel->setText("Unsupported platform");
        QMessageBox::warning(windowPtr, "Power Patch", "This feature is only supported on Windows.");
        QTimer::singleShot(app_constants::kReenableButtonsDelayMs, windowPtr, [allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton] {
            allUpdateButton->setEnabled(true);
            winUpdateButton->setEnabled(true);
            storeUpdateButton->setEnabled(true);
            m365UpdateButton->setEnabled(true);
        });
#endif
    });

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel, 1, Qt::AlignVCenter | Qt::AlignLeft);

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(subtitleLabel);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(closeUpdateWindowsCheck);
    mainLayout->addWidget(trayOnCloseCheck);
    QObject::connect(allUpdateButton, &QPushButton::clicked, [&] {
        allUpdateButton->setEnabled(false);
        winUpdateButton->setEnabled(false);
        storeUpdateButton->setEnabled(false);
        m365UpdateButton->setEnabled(false);
        statusLabel->setText("Starting all updates...");

#ifdef _WIN32
        const bool closeAfter = closeUpdateWindowsCheck->isChecked();
        std::thread([windowPtr, statusLabel, allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton, closeAfter] {
            bool winScanOk = false;
            bool winUiOk = false;
            bool storeOpened = false;
            bool storeClicked = false;
            bool officeOk = false;

            winScanOk = startWindowsUpdateScan();
            winUiOk = openWindowsUpdateSettings();
            if (closeAfter && winUiOk)
                closeWindowsUpdateWindowAfterDelay(app_constants::kCloseWindowsUpdateDelayMs);
            QMetaObject::invokeMethod(windowPtr, [statusLabel] {
                statusLabel->setText("Windows Update started. Moving to Store...");
            }, Qt::QueuedConnection);

            storeOpened = openMicrosoftStoreLibrary();
            if (storeOpened)
                storeClicked = clickMicrosoftStoreGetUpdates(closeAfter);
            QMetaObject::invokeMethod(windowPtr, [statusLabel] {
                statusLabel->setText("Store updates started. Moving to Microsoft 365...");
            }, Qt::QueuedConnection);

            officeOk = startMicrosoft365Update();
            if (closeAfter && officeOk)
                closeWindowByProcessAfterDelay(L"OfficeC2RClient.exe", app_constants::kCloseOfficeDelayMs);
            QMetaObject::invokeMethod(windowPtr,
                                     [windowPtr, statusLabel, allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton,
                                      winScanOk, winUiOk, storeOpened, storeClicked, officeOk] {
                if (!winScanOk && !winUiOk) {
                    QMessageBox::warning(
                        windowPtr,
                        "Power Patch",
                        "Windows Update did not start. The Settings page might not be available on this system.");
                } else if (!winScanOk && winUiOk) {
                    QMessageBox::information(
                        windowPtr,
                        "Power Patch",
                        "Windows Update opened, but the scan trigger wasn't available.\n\n"
                        "If it doesn't automatically start scanning, click \"Check for updates\" in the Settings window.");
                }

                if (!storeOpened) {
                    QMessageBox::warning(
                        windowPtr,
                        "Power Patch",
                        "Couldn't open the Microsoft Store Library page.\n\n"
                        "Make sure Microsoft Store is installed and enabled on this PC.");
                } else if (!storeClicked) {
                    QMessageBox::information(
                        windowPtr,
                        "Power Patch",
                        "Microsoft Store opened, but the app couldn't automatically click the \"Get updates\" button.\n\n"
                        "If updates don't start automatically, click \"Get updates\" in the Store Library.");
                }

                if (!officeOk) {
                    QMessageBox::warning(
                        windowPtr,
                        "Power Patch",
                        "Couldn't start Microsoft 365 (Office) updates.\n\n"
                        "This requires a local Microsoft 365 Apps / Office Click-to-Run install.\n"
                        "If you're using a different Office installation type, update it via its own updater or management tooling.");
                }

                if ((winScanOk || winUiOk) && storeOpened && officeOk) {
                    statusLabel->setText("All update checks started");
                } else {
                    statusLabel->setText("All updates finished with some issues");
                }

                QTimer::singleShot(app_constants::kReenableButtonsDelayMs, windowPtr, [allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton] {
                    allUpdateButton->setEnabled(true);
                    winUpdateButton->setEnabled(true);
                    storeUpdateButton->setEnabled(true);
                    m365UpdateButton->setEnabled(true);
                });
            }, Qt::QueuedConnection);
        }).detach();
#else
        statusLabel->setText("Unsupported platform");
        QMessageBox::warning(windowPtr, "Power Patch", "This feature is only supported on Windows.");
        QTimer::singleShot(app_constants::kReenableButtonsDelayMs, windowPtr, [allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton] {
            allUpdateButton->setEnabled(true);
            winUpdateButton->setEnabled(true);
            storeUpdateButton->setEnabled(true);
            m365UpdateButton->setEnabled(true);
        });
#endif
    });

    mainLayout->addWidget(allUpdateButton);
    mainLayout->addWidget(winUpdateButton);
    mainLayout->addWidget(storeUpdateButton);
    mainLayout->addWidget(m365UpdateButton);

    window.resize(420, 280);
    window.show();

    return app.exec();
}
