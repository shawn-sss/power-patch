#pragma once

#include "update_service.h"
#include <QWidget>
#include <QFutureWatcher>
#include <QSettings>
#include <QTimer>
#include <array>
#include <functional>

class QCheckBox;
class QLabel;
class QPushButton;
class QProgressBar;
class QPlainTextEdit;
class QSystemTrayIcon;
class QAction;
class QBoxLayout;

class Dashboard : public QWidget {
public:
    using Launcher = std::function<UpdateResult(UpdateKind, bool)>;
    explicit Dashboard(const QString &settingsPath, Launcher launcher = launchUpdate,
                       bool enableTray = true, bool migrateLegacy = true);
    ~Dashboard() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Card {
        QCheckBox *selected;
        QPushButton *run;
        QLabel *detail;
    };
    QSettings settings_;
    Launcher launcher_;
    QFutureWatcher<UpdateResult> watcher_;
    QFutureWatcher<QSettings::Status> preferenceSave_;
    QTimer preferenceTimer_;
    std::array<Card, 3> cards_{};
    QList<int> pending_;
    int current_ = -1;
    int total_ = 0;
    int completed_ = 0;
    int attention_ = 0;
    bool busy_ = false;
    bool stopping_ = false;
    bool quitting_ = false;
    bool closeAfter_ = false;
    QLabel *summary_;
    QLabel *selection_;
    QLabel *settingsWarning_;
    QProgressBar *progress_;
    QPushButton *runSelected_;
    QPushButton *stop_;
    QPushButton *selectAll_;
    QCheckBox *closeWindows_;
    QCheckBox *trayOnClose_;
    QPlainTextEdit *activity_;
    QSystemTrayIcon *tray_ = nullptr;
    QAction *trayRun_ = nullptr;
    QBoxLayout *actions_ = nullptr;
    QBoxLayout *preferencesLayout_ = nullptr;

    void refreshControls();
    void start(QList<int> indices);
    void next();
    void stopPending();
    void finish();
    void record(int index, const QString &message);
    void setBadge(int index, const QString &text, const QString &state);
    void saveSettings();
    void queuePreferenceSave();
    void flushPreferences();
    void applyTheme();
};
