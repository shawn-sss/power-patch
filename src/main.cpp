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
#include <QStyle>
#include <QStyleOptionButton>
#include <QCursor>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QAbstractButton>
#include <QFile>
#include <QTextStream>
#include <QPointer>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

#include "m365_update/m365_update.h"
#include "store_update/store_update.h"
#include "windows_update/windows_update.h"

class SimpleSettings {
public:
    explicit SimpleSettings(const QString &filePath) : filePath_(filePath) {
        load();
    }
    
    bool value(const QString &key, bool defaultValue) const {
        return data_.value(key, defaultValue ? "true" : "false") == "true";
    }
    
    void setValue(const QString &key, bool value) {
        data_[key] = value ? "true" : "false";
        save();
    }
    
private:
    void load() {
        QFile file(filePath_);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith('#')) {
                    int idx = line.indexOf('=');
                    if (idx > 0) {
                        data_[line.left(idx).trimmed()] = line.mid(idx + 1).trimmed();
                    }
                }
            }
        }
    }
    
    void save() {
        QFile file(filePath_);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            for (auto it = data_.constBegin(); it != data_.constEnd(); ++it) {
                out << it.key() << "=" << it.value() << "\n";
            }
        }
    }
    
    QString filePath_;
    QMap<QString, QString> data_;
};

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
                    trayOnCloseCheck_(const_cast<QCheckBox *>(trayOnCloseCheck)),
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
                if (window_)
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
    QPointer<QWidget> window_;
    QPointer<QCheckBox> trayOnCloseCheck_;
    bool *allowQuit_;
    QPointer<QSystemTrayIcon> trayIcon_;
    QPointer<QApplication> app_;
};

class TrayMenuLeaveFilter : public QObject
{
public:
    explicit TrayMenuLeaveFilter(QMenu *menu, QObject *parent = nullptr)
        : QObject(parent), menu_(menu) {}

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (obj == menu_) {
            if (event->type() == QEvent::Leave) {
                if (menu_ && menu_->isVisible())
                    menu_->close();
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QPointer<QMenu> menu_;
};

class TrayMenuAutoDismissController : public QObject
{
public:
    explicit TrayMenuAutoDismissController(QMenu *menu, QObject *parent = nullptr)
        : QObject(parent), menu_(menu), timer_(new QTimer(this)), iconRadius_(48)
    {
        timer_->setInterval(150);
        connect(timer_, &QTimer::timeout, this, &TrayMenuAutoDismissController::checkMouse);

        connect(menu_, &QMenu::aboutToShow, this, [this]() {
            iconPos_ = QCursor::pos();
            timer_->start();
        });

        connect(menu_, &QMenu::aboutToHide, this, [this]() {
            timer_->stop();
        });
    }

private:
    QPointer<QMenu> menu_;
    QTimer *timer_;
    QPoint iconPos_;
    int iconRadius_;

    void checkMouse()
    {
        if (!menu_ || !menu_->isVisible()) {
            timer_->stop();
            return;
        }

        const QPoint pos = QCursor::pos();

        const QRect menuLocalRect = menu_->rect();
        const QPoint menuTopLeft = menu_->mapToGlobal(menuLocalRect.topLeft());
        const QRect menuGlobalRect(menuTopLeft, menuLocalRect.size());

        if (menuGlobalRect.contains(pos))
            return;

        const QRect iconRect(iconPos_.x() - iconRadius_, iconPos_.y() - iconRadius_, iconRadius_ * 2, iconRadius_ * 2);
        if (iconRect.contains(pos))
            return;

        menu_->close();
    }
};

class CheckboxHoverCursorFilter : public QObject
{
public:
    explicit CheckboxHoverCursorFilter(QCheckBox *checkbox, QObject *parent = nullptr)
        : QObject(parent), checkbox_(checkbox)
    {
        if (checkbox_) {
            checkbox_->setMouseTracking(true);
        }
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (obj != checkbox_ || !checkbox_)
            return QObject::eventFilter(obj, event);

        if (event->type() == QEvent::Leave) {
            checkbox_->unsetCursor();
            return QObject::eventFilter(obj, event);
        }

        if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            QStyleOptionButton opt;
            opt.initFrom(checkbox_);
            opt.text = checkbox_->text();
            const QRect indicator = checkbox_->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &opt, checkbox_);
            const int spacing = checkbox_->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, &opt, checkbox_);
            QFontMetrics fm(checkbox_->font());
            const QSize textSize = fm.size(Qt::TextSingleLine, checkbox_->text());
            const int textX = indicator.right() + spacing + 1;
            const int textY = (checkbox_->height() - textSize.height()) / 2;
            const QRect textRect(QPoint(textX, textY), textSize);
            if (indicator.contains(mouseEvent->pos()) || textRect.contains(mouseEvent->pos())) {
                checkbox_->setCursor(Qt::PointingHandCursor);
            } else {
                checkbox_->unsetCursor();
            }
        }

        return QObject::eventFilter(obj, event);
    }

private:
    QPointer<QCheckBox> checkbox_;
};

namespace {
constexpr int kReenableButtonsDelayMs = 1200;
constexpr int kWaitBeforeWindowsScanMs = 5000;
constexpr int kWaitBeforeCloseMs = 5000;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Power Patch");
    app.setApplicationDisplayName("Power Patch");
    app.setOrganizationName("Power Patch");
    
    const QString exeDir = QCoreApplication::applicationDirPath();

    const bool isDeployed = exeDir.contains("AppData", Qt::CaseInsensitive);
    
    if (isDeployed) {
        QStringList criticalFiles = {
            "Qt6Core.dll",
            "Qt6Gui.dll", 
            "Qt6Widgets.dll",
            "platforms/qwindows.dll",
            "styles/qmodernwindowsstyle.dll"
        };
        
        QStringList missingFiles;
        for (const QString &file : criticalFiles) {
            if (!QFile::exists(exeDir + "/" + file)) {
                missingFiles.append(file);
            }
        }
        
        if (!missingFiles.isEmpty()) {
            QString errorMsg = "Critical files are missing or corrupted:\n\n";
            for (const QString &file : missingFiles) {
                errorMsg += "• " + file + "\n";
            }
            errorMsg += "\nPlease re-run the Power Patch installer to repair the application.";
            
            QMessageBox::critical(nullptr, "Power Patch - Installation Error", errorMsg);
            return 1;
        }
    }
    
    const QString settingsPath = exeDir + "/settings.ini";
    SimpleSettings settings(settingsPath);
    
    const char *kIcon1024Resource = ":/icons/assets/img/powerpatch_1024.png";
    const char *kIcon2048Resource = ":/icons/assets/img/powerpatch_2048.png";
    const char *kMasterIconResource = ":/icons/assets/img/powerpatch.png";
    QIcon appIcon;
    appIcon.addFile(kIcon1024Resource, QSize(1024, 1024));
    appIcon.addFile(kIcon2048Resource, QSize(2048, 2048));
    if (appIcon.isNull()) {
        appIcon.addFile("assets/img/powerpatch_1024.png", QSize(1024, 1024));
        appIcon.addFile("assets/img/powerpatch_2048.png", QSize(2048, 2048));
    }
    app.setWindowIcon(appIcon);

    QWidget window;
    window.setWindowTitle("Power Patch");
    window.setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);
    window.setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    window.setWindowFlags(window.windowFlags() & ~Qt::WindowMaximizeButtonHint);
    auto isDarkMode = [&app] {
        return app.palette().color(QPalette::Window).lightness() < 128;
    };
    auto applyTheme = [&](bool darkMode, QMenu *trayMenu, QPushButton *aboutButton) {
        if (darkMode) {
            window.setStyleSheet(
                "QWidget { background-color: #1b1f24; }"
                "QLabel { color: #e6edf3; }"
                "QLabel#subtitleLabel { color: #9aa4b2; }"
                "QLabel#statusLabel { color: #c1c7d0; }"
                "QLabel#appIcon { background-color: transparent; border: none; border-radius: 8px; }"
                "QPushButton { color: #e6edf3; background-color: #2a313b; border: 1px solid #3a4452; border-radius: 8px; padding: 7px 12px; }"
                "QPushButton:hover { background-color: #323b46; border-color: #4a5666; }"
                "QPushButton:pressed { background-color: #262d36; border-color: #4a5666; }"
                "QPushButton:disabled { color: #6b7480; background-color: #232831; border-color: #323a45; }"
                "QCheckBox { color: #e6edf3; }"
                "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #55606e; border-radius: 4px; background: #1f242b; }"
                "QCheckBox::indicator:checked { border-color: #7b8796; background-color: #2e3743; image: url(:/icons/assets/img/check_dark.png); }"
                "QCheckBox::indicator:disabled { border-color: #3a424d; background: #1c2127; }");
            if (trayMenu) {
                trayMenu->setStyleSheet(
                    "QMenu { background-color: #242a31; border: 1px solid #3a424c; border-radius: 8px; }"
                    "QMenu::item { padding: 6px 18px; border-radius: 4px; }"
                    "QMenu::item:selected { background-color: #2f3741; }"
                    "QMenu::separator { height: 1px; background: #3a424c; margin: 4px 6px; }");
            }
            if (aboutButton) {
                aboutButton->setStyleSheet(
                    "QPushButton { border-radius: 15px; padding: 0px; color: #e6edf3; background-color: transparent; border: none; }"
                    "QPushButton:hover { border-color: #55606e; }");
            }
        } else {
            window.setStyleSheet(
                "QWidget { background-color: #f7f8fa; }"
                "QLabel { color: #1f2328; }"
                "QLabel#subtitleLabel { color: #5a6470; }"
                "QLabel#statusLabel { color: #47505a; }"
                "QLabel#appIcon { background-color: transparent; border: none; border-radius: 8px; }"
                "QPushButton { color: #1f2328; background-color: #ffffff; border: 1px solid #d0d6dd; border-radius: 8px; padding: 7px 12px; }"
                "QPushButton:hover { background-color: #f1f3f6; border-color: #b8c0c9; }"
                "QPushButton:pressed { background-color: #e8edf2; border-color: #b8c0c9; }"
                "QPushButton:disabled { color: #8a929b; background-color: #f4f6f8; border-color: #d9dee4; }"
                "QCheckBox { color: #1f2328; }"
                "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #aeb6bf; border-radius: 4px; background: #ffffff; }"
                "QCheckBox::indicator:checked { border-color: #7b8796; background-color: #e9edf2; image: url(:/icons/assets/img/check_light.png); }"
                "QCheckBox::indicator:disabled { border-color: #c8cfd8; background: #f3f5f8; }");
            if (trayMenu) {
                trayMenu->setStyleSheet(
                    "QMenu { background-color: #ffffff; border: 1px solid #d0d6dd; border-radius: 8px; }"
                    "QMenu::item { padding: 6px 18px; border-radius: 4px; }"
                    "QMenu::item:selected { background-color: #eef1f4; }"
                    "QMenu::separator { height: 1px; background: #d0d6dd; margin: 4px 6px; }");
            }
            if (aboutButton) {
                aboutButton->setStyleSheet(
                    "QPushButton { border-radius: 15px; padding: 0px; color: #1f2328; background-color: transparent; border: none; }"
                    "QPushButton:hover { border-color: #aeb6bf; }");
            }
        }
    };
    QWidget *windowPtr = &window;

    auto *mainLayout = new QVBoxLayout(&window);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

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
        appIconPixmap.load("assets/img/powerpatch.png");
    }
    if (appIconPixmap.isNull()) {
        appIconPixmap.load("assets/img/powerpatch_1024.png");
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
    closeUpdateWindowsCheck->setChecked(settings.value("closeUpdateWindows", false));

    auto *trayOnCloseCheck = new QCheckBox("Send app to system tray when closed");
    trayOnCloseCheck->setChecked(settings.value("trayOnClose", false));

    auto *enableWindowsUpdateCheck = new QCheckBox("");
    enableWindowsUpdateCheck->setChecked(settings.value("enableWindowsUpdate", true));
    enableWindowsUpdateCheck->setAccessibleName("Enable Windows Update");

    auto *enableStoreUpdateCheck = new QCheckBox("");
    enableStoreUpdateCheck->setChecked(settings.value("enableStoreUpdate", true));
    enableStoreUpdateCheck->setAccessibleName("Enable Microsoft Store updates");

    auto *enableM365UpdateCheck = new QCheckBox("");
    enableM365UpdateCheck->setChecked(settings.value("enableM365Update", true));

    closeUpdateWindowsCheck->installEventFilter(new CheckboxHoverCursorFilter(closeUpdateWindowsCheck, closeUpdateWindowsCheck));
    trayOnCloseCheck->installEventFilter(new CheckboxHoverCursorFilter(trayOnCloseCheck, trayOnCloseCheck));
    enableWindowsUpdateCheck->installEventFilter(new CheckboxHoverCursorFilter(enableWindowsUpdateCheck, enableWindowsUpdateCheck));
    enableStoreUpdateCheck->installEventFilter(new CheckboxHoverCursorFilter(enableStoreUpdateCheck, enableStoreUpdateCheck));
    enableM365UpdateCheck->installEventFilter(new CheckboxHoverCursorFilter(enableM365UpdateCheck, enableM365UpdateCheck));
    enableM365UpdateCheck->setAccessibleName("Enable Microsoft 365 updates");
    
    QObject::connect(closeUpdateWindowsCheck, &QCheckBox::toggled, [&settings](bool checked) {
        settings.setValue("closeUpdateWindows", checked);
    });
    QObject::connect(trayOnCloseCheck, &QCheckBox::toggled, [&settings](bool checked) {
        settings.setValue("trayOnClose", checked);
    });
    QObject::connect(enableWindowsUpdateCheck, &QCheckBox::toggled, [&settings](bool checked) {
        settings.setValue("enableWindowsUpdate", checked);
    });
    QObject::connect(enableStoreUpdateCheck, &QCheckBox::toggled, [&settings](bool checked) {
        settings.setValue("enableStoreUpdate", checked);
    });
    QObject::connect(enableM365UpdateCheck, &QCheckBox::toggled, [&settings](bool checked) {
        settings.setValue("enableM365Update", checked);
    });

    auto *allUpdateButton = new QPushButton("Run selected updates");
    allUpdateButton->setMinimumHeight(36);
    allUpdateButton->setCursor(Qt::PointingHandCursor);

    auto *winUpdateButton = new QPushButton("Run Windows updates");
    winUpdateButton->setDefault(true);
    winUpdateButton->setMinimumHeight(34);
    winUpdateButton->setCursor(Qt::PointingHandCursor);

    auto *storeUpdateButton = new QPushButton("Run Microsoft Store updates");
    storeUpdateButton->setMinimumHeight(34);
    storeUpdateButton->setCursor(Qt::PointingHandCursor);

    auto *m365UpdateButton = new QPushButton("Run Microsoft 365 updates");
    m365UpdateButton->setMinimumHeight(34);
    m365UpdateButton->setCursor(Qt::PointingHandCursor);

    auto updateButtonStates = [allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton,
                               enableWindowsUpdateCheck, enableStoreUpdateCheck, enableM365UpdateCheck] {
        const bool anyEnabled = enableWindowsUpdateCheck->isChecked()
                                || enableStoreUpdateCheck->isChecked()
                                || enableM365UpdateCheck->isChecked();
        allUpdateButton->setEnabled(anyEnabled);
        winUpdateButton->setEnabled(enableWindowsUpdateCheck->isChecked());
        storeUpdateButton->setEnabled(enableStoreUpdateCheck->isChecked());
        m365UpdateButton->setEnabled(enableM365UpdateCheck->isChecked());
    };

    QObject::connect(enableWindowsUpdateCheck, &QCheckBox::toggled, [updateButtonStates](bool) {
        updateButtonStates();
    });
    QObject::connect(enableStoreUpdateCheck, &QCheckBox::toggled, [updateButtonStates](bool) {
        updateButtonStates();
    });
    QObject::connect(enableM365UpdateCheck, &QCheckBox::toggled, [updateButtonStates](bool) {
        updateButtonStates();
    });
    updateButtonStates();

    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    bool allowQuit = false;
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        app.setQuitOnLastWindowClosed(false);
        trayIcon = new QSystemTrayIcon(appIcon, &window);
        trayIcon->setToolTip("Power Patch");

        trayMenu = new QMenu(&window);
        auto *openAction = trayMenu->addAction("Open");
        auto *runAllAction = trayMenu->addAction("Run");
        auto *exitAction = trayMenu->addAction("Exit");

        trayMenu->ensurePolished();
        QFontMetrics fm(trayMenu->font());
        int maxTextWidth = 0;
        const QList<QAction*> actions = trayMenu->actions();
        for (QAction *a : actions) {
            if (!a) continue;
            const int w = fm.horizontalAdvance(a->text());
            if (w > maxTextWidth) maxTextWidth = w;
        }
        const int horizPadding = 18 * 2;
        const int extra = 12;
        trayMenu->setFixedWidth(maxTextWidth + horizPadding + extra);
        trayMenu->installEventFilter(new TrayMenuLeaveFilter(trayMenu, trayMenu));

        new TrayMenuAutoDismissController(trayMenu, trayMenu);

        QObject::connect(openAction, &QAction::triggered, [&] {
            window.show();
            window.raise();
            window.activateWindow();
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
        auto showTrayMenuAtCursor = [trayMenu] {
            trayMenu->ensurePolished();
            const QPoint cursorPos = QCursor::pos();
            QSize menuSize = trayMenu->sizeHint();
            QScreen *screen = QGuiApplication::screenAt(cursorPos);
            if (!screen)
                screen = QGuiApplication::primaryScreen();
            if (screen) {
                const QRect bounds = screen->availableGeometry();
                if (cursorPos.x() + menuSize.width() > bounds.right())
                    menuSize.setWidth(bounds.right() - cursorPos.x());
                int y = cursorPos.y() - menuSize.height();
                if (y < bounds.top())
                    y = bounds.top();
                trayMenu->popup(QPoint(cursorPos.x(), y));
            } else {
                trayMenu->popup(QPoint(cursorPos.x(), cursorPos.y() - menuSize.height()));
            }
        };

        QObject::connect(trayIcon, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                window.show();
                window.raise();
                window.activateWindow();
            } else if (reason == QSystemTrayIcon::Context) {
                showTrayMenuAtCursor();
            }
        });

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

#ifdef _WIN32
        const bool windowsDisabled = areWindowsUpdatesDisabled();
        if (windowsDisabled) {
            statusLabel->setText("Windows Update disabled");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Windows Update is disabled by the service or policy on this device.\n"
                "Enable Windows Update before trying again.");
            QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
                updateButtonStates();
                statusLabel->setText("Ready");
            });
        } else {
            statusLabel->setText("Checking Windows updates...");
            const bool closeAfter = closeUpdateWindowsCheck->isChecked();
            (void)QtConcurrent::run([windowPtr, statusLabel, updateButtonStates, closeAfter] {
                const bool uiOk = openWindowsUpdateSettings();
                if (uiOk) {
                    QThread::msleep(static_cast<unsigned long>(kWaitBeforeWindowsScanMs));
                }
                const bool scanOk = startWindowsUpdateScan();

                if (closeAfter && uiOk) {
                    QThread::msleep(static_cast<unsigned long>(kWaitBeforeCloseMs));
                    closeWindowsUpdateWindowAfterDelay(0);
                }

                QMetaObject::invokeMethod(windowPtr, [windowPtr, statusLabel, updateButtonStates, scanOk, uiOk] {
                    if (!scanOk && !uiOk) {
                        QMessageBox::warning(
                            windowPtr,
                            "Power Patch",
                            "Couldn't start a Windows Update scan or open the Windows Update settings page.\n"
                            "This feature requires Windows 11 (or later) and access to the Settings app.");
                    } else if (!scanOk && uiOk) {
                        QMessageBox::information(
                            windowPtr,
                            "Power Patch",
                            "Windows Update opened, but the scan trigger wasn't available.\n"
                            "If it doesn't automatically start scanning, click \"Check for updates\" in the Settings window.");
                    }

                    QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
                        updateButtonStates();
                        statusLabel->setText("Ready");
                    });
                }, Qt::QueuedConnection);
            });
        }
#else
        statusLabel->setText("Unsupported platform");
        QMessageBox::warning(windowPtr, "Power Patch", "This feature is only supported on Windows.");
        QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
            updateButtonStates();
            statusLabel->setText("Ready");
        });
#endif
    });

    QObject::connect(m365UpdateButton, &QPushButton::clicked, [&] {
        allUpdateButton->setEnabled(false);
        winUpdateButton->setEnabled(false);
        storeUpdateButton->setEnabled(false);
        m365UpdateButton->setEnabled(false);

#ifdef _WIN32
        const Microsoft365UpdateStatus m365Status = queryMicrosoft365UpdateStatus();
        if (m365Status == Microsoft365UpdateStatus::Disabled) {
            statusLabel->setText("Microsoft 365 updates disabled");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Microsoft 365 updates are disabled in the Click-to-Run configuration.\nYou must enable them before retrying.");
        } else if (m365Status == Microsoft365UpdateStatus::NotInstalled) {
            statusLabel->setText("Microsoft 365 Apps not installed");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Microsoft 365 Apps do not appear to be installed on this machine.\nUpdates cannot run.");
        } else {
            statusLabel->setText("Checking Microsoft 365 updates...");
            const bool closeAfter = closeUpdateWindowsCheck->isChecked();
            const bool ok = startMicrosoft365Update();
            if (!ok) {
                QMessageBox::warning(
                    windowPtr,
                    "Power Patch",
                    "Couldn't start Microsoft 365 updates.\n"
                    "This requires a local Microsoft 365 Apps / Office Click-to-Run install.\n"
                    "If you're using a different Office installation type, update it via its own updater or management tooling.");
            }

                if (closeAfter && ok) {
                    (void)QtConcurrent::run([] {
                        closeMicrosoft365UpdateAfterCompletion(kWaitBeforeCloseMs);
                    });
                }
        }
#else
        statusLabel->setText("Unsupported platform");
        QMessageBox::warning(windowPtr, "Power Patch", "This feature is only supported on Windows.");
#endif

        QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
            updateButtonStates();
            statusLabel->setText("Ready");
        });
    });

    QObject::connect(storeUpdateButton, &QPushButton::clicked, [&] {
        allUpdateButton->setEnabled(false);
        winUpdateButton->setEnabled(false);
        storeUpdateButton->setEnabled(false);
        m365UpdateButton->setEnabled(false);

#ifdef _WIN32
        const MicrosoftStoreStatus storeStatus = queryMicrosoftStoreStatus();
        if (storeStatus == MicrosoftStoreStatus::Disabled) {
            statusLabel->setText("Microsoft Store updates disabled");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Microsoft Store updates are disabled by policy on this device.\nEnable the Store before retrying.");
        } else if (storeStatus == MicrosoftStoreStatus::Uninstalled) {
            statusLabel->setText("Microsoft Store not installed");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Microsoft Store appears to be uninstalled on this PC.\nStore updates cannot run.");
        } else {
            statusLabel->setText("Checking Microsoft Store app updates...");
            const bool closeAfter = closeUpdateWindowsCheck->isChecked();
            (void)QtConcurrent::run([windowPtr, statusLabel, allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton, closeAfter, updateButtonStates] {
                const bool opened = openMicrosoftStoreLibrary();
                bool clicked = false;
                if (opened)
                    clicked = clickMicrosoftStoreGetUpdates(closeAfter);

                QMetaObject::invokeMethod(windowPtr,
                                         [windowPtr, statusLabel, allUpdateButton, winUpdateButton, storeUpdateButton, m365UpdateButton, opened, clicked, updateButtonStates] {
                    if (!opened) {
                        QMessageBox::warning(
                            windowPtr,
                            "Power Patch",
                            "Couldn't open the Microsoft Store Library page.\n"
                            "Make sure Microsoft Store is installed and enabled on this PC.");
                    } else if (!clicked) {
                        QMessageBox::information(
                            windowPtr,
                            "Power Patch",
                            "Microsoft Store opened, but the app couldn't automatically click \"Check for updates\" or \"Update\".\n"
                            "If updates don't start automatically, click \"Check for updates\" in the Store Library.");
                    }

                    QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
                        updateButtonStates();
                        statusLabel->setText("Ready");
                    });
                },
                                         Qt::QueuedConnection);
            });
        }
#else
        statusLabel->setText("Unsupported platform");
        QMessageBox::warning(windowPtr, "Power Patch", "This feature is only supported on Windows.");
        QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
            updateButtonStates();
            statusLabel->setText("Ready");
        });
#endif
    });

    auto *aboutButton = new QPushButton();
    aboutButton->setMinimumSize(30, 30);
    aboutButton->setIcon(window.style()->standardIcon(QStyle::SP_MessageBoxInformation));
    aboutButton->setCursor(Qt::PointingHandCursor);

    applyTheme(isDarkMode(), trayMenu, aboutButton);
    QObject::connect(&app, &QGuiApplication::paletteChanged, &window, [&](const QPalette &) {
        applyTheme(isDarkMode(), trayMenu, aboutButton);
    });
    QObject::connect(aboutButton, &QPushButton::clicked, [&] {
        QMessageBox aboutBox(&window);
        aboutBox.setWindowTitle("About Power Patch");
        const QString aboutHtml =
            "<p><b>Power Patch v1.1</b><br/>"
            "Quick update launcher for Windows.</p>"
            "<p><span style='color:#7a7a7a; font-size:small;'>Developed on Windows 11 25H2</span></p>"
            "<p>Windows Update: opens Settings + triggers scan.<br/>"
            "Microsoft Store: opens Library + clicks Get updates.<br/>"
            "Microsoft 365: runs OfficeC2RClient update.</p>"
            "<p>Author: Shawn SSS</p>";
        aboutBox.setText(aboutHtml);
        aboutBox.setTextFormat(Qt::RichText);
        aboutBox.setIconPixmap(appIconPixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        aboutBox.setStandardButtons(QMessageBox::Ok);
        if (auto *okButton = aboutBox.button(QMessageBox::Ok)) {
            okButton->setCursor(Qt::PointingHandCursor);
        }
        aboutBox.exec();
    });

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel, 1, Qt::AlignVCenter | Qt::AlignLeft);
    headerLayout->addWidget(aboutButton, 0, Qt::AlignVCenter | Qt::AlignRight);

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(subtitleLabel);
    mainLayout->addItem(new QSpacerItem(0, 6, QSizePolicy::Minimum, QSizePolicy::Fixed));
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(closeUpdateWindowsCheck);
    mainLayout->addWidget(trayOnCloseCheck);
    auto *divider = new QFrame();
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(divider);
    QObject::connect(allUpdateButton, &QPushButton::clicked, [&] {
        bool winEnabled = enableWindowsUpdateCheck->isChecked();
        bool storeEnabled = enableStoreUpdateCheck->isChecked();
        bool m365Enabled = enableM365UpdateCheck->isChecked();
        if (!winEnabled && !storeEnabled && !m365Enabled) {
            statusLabel->setText("All updates disabled");
            return;
        }

        allUpdateButton->setEnabled(false);
        winUpdateButton->setEnabled(false);
        storeUpdateButton->setEnabled(false);
        m365UpdateButton->setEnabled(false);

#ifdef _WIN32
        if (winEnabled && areWindowsUpdatesDisabled()) {
            winEnabled = false;
            statusLabel->setText("Windows Update disabled");
                QMessageBox::warning(
                    windowPtr,
                    "Power Patch",
                    "Windows Update is disabled by the service or policy on this device.\n"
                    "Enable Windows Update before trying again.");
        }

        if (storeEnabled) {
            const MicrosoftStoreStatus storeStatus = queryMicrosoftStoreStatus();
            if (storeStatus == MicrosoftStoreStatus::Disabled) {
                storeEnabled = false;
                statusLabel->setText("Microsoft Store updates disabled");
                QMessageBox::warning(
                    windowPtr,
                    "Power Patch",
                    "Microsoft Store updates are disabled by policy on this device.\nEnable the Store before retrying.");
            } else if (storeStatus == MicrosoftStoreStatus::Uninstalled) {
                storeEnabled = false;
                statusLabel->setText("Microsoft Store not installed");
                QMessageBox::warning(
                    windowPtr,
                    "Power Patch",
                    "Microsoft Store appears to be uninstalled on this PC.\nStore updates cannot run.");
            }
        }

        if (m365Enabled) {
            const Microsoft365UpdateStatus m365Status = queryMicrosoft365UpdateStatus();
            if (m365Status == Microsoft365UpdateStatus::Disabled) {
                m365Enabled = false;
                statusLabel->setText("Microsoft 365 updates disabled");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Microsoft 365 updates are disabled in the Click-to-Run configuration.\nYou must enable them before retrying.");
            } else if (m365Status == Microsoft365UpdateStatus::NotInstalled) {
                m365Enabled = false;
                statusLabel->setText("Microsoft 365 Apps not installed");
            QMessageBox::warning(
                windowPtr,
                "Power Patch",
                "Microsoft 365 Apps do not appear to be installed on this machine.\nUpdates cannot run.");
            }
        }

        if (!winEnabled && !storeEnabled && !m365Enabled) {
            QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
                updateButtonStates();
                statusLabel->setText("Ready");
            });
            return;
        }

        const bool closeAfter = closeUpdateWindowsCheck->isChecked();
        (void)QtConcurrent::run([windowPtr, statusLabel, closeAfter, winEnabled, storeEnabled, m365Enabled, updateButtonStates] {
            bool winScanOk = false;
            bool winUiOk = false;
            bool storeOpened = false;
            bool storeClicked = false;
            bool officeOk = false;

            if (winEnabled) {
                QMetaObject::invokeMethod(windowPtr, [statusLabel] {
                    statusLabel->setText("Starting Windows Update...");
                }, Qt::QueuedConnection);
                winUiOk = openWindowsUpdateSettings();
                if (winUiOk) {
                    QThread::msleep(static_cast<unsigned long>(kWaitBeforeWindowsScanMs));
                }
                winScanOk = startWindowsUpdateScan();
                if (closeAfter && winUiOk) {
                    QThread::msleep(static_cast<unsigned long>(kWaitBeforeCloseMs));
                    closeWindowsUpdateWindowAfterDelay(0);
                }
            }

            if (storeEnabled) {
                QMetaObject::invokeMethod(windowPtr, [statusLabel] {
                    statusLabel->setText("Starting Store updates...");
                }, Qt::QueuedConnection);
                storeOpened = openMicrosoftStoreLibrary();
                if (storeOpened)
                    storeClicked = clickMicrosoftStoreGetUpdates(closeAfter);
            }

            if (m365Enabled) {
                QMetaObject::invokeMethod(windowPtr, [statusLabel] {
                    statusLabel->setText("Starting Microsoft 365 update...");
                }, Qt::QueuedConnection);
                officeOk = startMicrosoft365Update();
                if (closeAfter && officeOk) {
                    closeMicrosoft365UpdateAfterCompletion(kWaitBeforeCloseMs);
                }
            }

            QMetaObject::invokeMethod(windowPtr,
                                     [windowPtr, statusLabel, winEnabled, storeEnabled, m365Enabled,
                                      winScanOk, winUiOk, storeOpened, storeClicked, officeOk, updateButtonStates] {
                if (winEnabled) {
                    if (!winScanOk && !winUiOk) {
                        QMessageBox::warning(
                            windowPtr,
                            "Power Patch",
                            "Windows Update did not start. The Settings page might not be available on this system.");
                    } else if (!winScanOk && winUiOk) {
                        QMessageBox::information(
                            windowPtr,
                            "Power Patch",
                            "Windows Update opened, but the scan trigger wasn't available.\n"
                            "If it doesn't automatically start scanning, click \"Check for updates\" in the Settings window.");
                    }
                }

                if (storeEnabled) {
                    if (!storeOpened) {
                        QMessageBox::warning(
                            windowPtr,
                            "Power Patch",
                                    "Couldn't open the Microsoft Store Library page.\n"
                                    "Make sure Microsoft Store is installed and enabled on this PC.");
                    } else if (!storeClicked) {
                        QMessageBox::information(
                            windowPtr,
                            "Power Patch",
                                    "Microsoft Store opened, but the app couldn't automatically click \"Check for updates\" or \"Update\".\n"
                                    "If updates don't start automatically, click \"Check for updates\" in the Store Library.");
                    }
                }

                if (m365Enabled && !officeOk) {
                    QMessageBox::warning(
                        windowPtr,
                        "Power Patch",
                        "Couldn't start Microsoft 365 updates.\n"
                        "This requires a local Microsoft 365 Apps / Office Click-to-Run install.\n"
                        "If you're using a different Office installation type, update it via its own updater or management tooling.");
                }

                QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
                    updateButtonStates();
                    statusLabel->setText("Ready");
                });
            }, Qt::QueuedConnection);
        });
#else
        statusLabel->setText("Unsupported platform");
        QMessageBox::warning(windowPtr, "Power Patch", "This feature is only supported on Windows.");
        QTimer::singleShot(kReenableButtonsDelayMs, windowPtr, [updateButtonStates, statusLabel] {
            updateButtonStates();
            statusLabel->setText("Ready");
        });
#endif
    });

    mainLayout->addWidget(allUpdateButton);

    auto *winUpdateLayout = new QHBoxLayout();
    winUpdateLayout->setSpacing(10);
    winUpdateLayout->addWidget(enableWindowsUpdateCheck);
    winUpdateLayout->addWidget(winUpdateButton, 1);
    mainLayout->addLayout(winUpdateLayout);

    auto *storeUpdateLayout = new QHBoxLayout();
    storeUpdateLayout->setSpacing(10);
    storeUpdateLayout->addWidget(enableStoreUpdateCheck);
    storeUpdateLayout->addWidget(storeUpdateButton, 1);
    mainLayout->addLayout(storeUpdateLayout);

    auto *m365UpdateLayout = new QHBoxLayout();
    m365UpdateLayout->setSpacing(10);
    m365UpdateLayout->addWidget(enableM365UpdateCheck);
    m365UpdateLayout->addWidget(m365UpdateButton, 1);
    mainLayout->addLayout(m365UpdateLayout);

    window.adjustSize();
    QSize targetSize = window.sizeHint();
    if (!targetSize.isValid())
        targetSize = QSize(420, 280);
    targetSize.setWidth(qMax(targetSize.width(), 500));
    QScreen *screen = window.screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect bounds = screen->availableGeometry();
        const int maxWidth = qMax(320, static_cast<int>(bounds.width() * 0.85));
        const int maxHeight = qMax(240, static_cast<int>(bounds.height() * 0.85));
        targetSize.setWidth(qMin(targetSize.width(), maxWidth));
        targetSize.setHeight(qMin(targetSize.height(), maxHeight));
    }
    window.setFixedSize(targetSize);
    if (screen) {
        const QRect bounds = screen->availableGeometry();
        const QPoint centered = bounds.center() - QPoint(targetSize.width() / 2, targetSize.height() / 2);
        window.move(centered);
    }
    window.show();

    return app.exec();
}
