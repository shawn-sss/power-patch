#include "dashboard.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QLabel>
#include <QLockFile>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThread>
#include <atomic>
#include <iostream>
#include <stdexcept>

namespace {
int failures = 0;
void check(bool condition, const char *message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
bool waitFor(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 4000) {
        QApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}
QPushButton *button(Dashboard &window, const char *name)
{
    return window.findChild<QPushButton *>(name);
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // The offscreen platform does not discover Windows fonts on its own.
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    app.setFont(QFont("Segoe UI", 10));
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/icons/assets/img/powerpatch_1024.png"));
    QTemporaryDir directory;
    check(directory.isValid(), "temporary settings directory exists");
    if (!directory.isValid()) return 1;
    const QString path = directory.path() + "/settings.ini";

    {
        std::atomic<int> calls{0};
        std::atomic<bool> ranOnGui{false};
        Dashboard window(path, [&](UpdateKind kind, bool) {
            if (QThread::currentThread() == app.thread()) ranOnGui = true;
            ++calls;
            if (kind == UpdateKind::Store) return UpdateResult{UpdateOutcome::Attention, "Manual action needed"};
            if (kind == UpdateKind::Office) return UpdateResult{UpdateOutcome::Unavailable, "Office unavailable"};
            return UpdateResult{UpdateOutcome::Started, "Request sent"};
        }, false, false);
        window.show();
        button(window, "selectAll")->click();
        check(!button(window, "runSelected")->isEnabled(), "empty selection disables batch launch");
        check(button(window, "run0")->isEnabled(), "individual launch is independent of batch selection");
        button(window, "selectAll")->click();
        button(window, "runSelected")->click();
        button(window, "runSelected")->click();
        button(window, "run1")->click();
        check(!window.findChild<QCheckBox *>("closeWindows")->isEnabled(), "launch options locked during run");
        check(waitFor([&] { return button(window, "runSelected")->isEnabled(); }), "batch finishes");
        check(calls == 3, "double clicks do not duplicate launches");
        check(!ranOnGui, "launchers execute off the GUI thread");
        check(window.findChild<QProgressBar *>("launchProgress")->value() == 3, "progress counts completed attempts");
        check(window.findChild<QLabel *>("summary")->text().contains("2 need attention"), "mixed outcomes retained in summary");
        check(window.findChild<QPlainTextEdit *>("activity")->toPlainText().contains("Office unavailable"), "log includes unavailable reason");
        window.findChild<QCheckBox *>("enableStoreUpdate")->setChecked(false);
        window.close();
    }
    {
        QSemaphore gate;
        QFile saved(path);
        check(saved.open(QIODevice::ReadOnly), "preferences saved to disk");
        saved.close();
        std::atomic<int> calls{0};
        Dashboard window(path, [&](UpdateKind, bool) {
            ++calls;
            gate.acquire();
            return UpdateResult{UpdateOutcome::Started, "Request sent"};
        }, false, false);
        check(!window.findChild<QCheckBox *>("enableStoreUpdate")->isChecked(), "selection survives restart");
        check(window.findChildren<QLabel *>("result")[1]->text().contains("Manual action needed"), "previous result survives restart");
        window.show();
        button(window, "selectAll")->click();
        button(window, "runSelected")->click();
        button(window, "stopQueued")->click();
        check(!button(window, "runSelected")->isEnabled(), "stop waits for current operation");
        gate.release();
        check(waitFor([&] { return button(window, "runSelected")->isEnabled(); }), "stopped run finishes");
        check(calls == 1, "stop prevents pending launches");
        check(window.findChild<QLabel *>("summary")->text().contains("1 of 3"), "stopped run does not claim full completion");
        button(window, "run2")->click();
        gate.release();
        check(waitFor([&] { return button(window, "runSelected")->isEnabled(); }), "can retry after stopping");
        check(calls == 2, "individual retry launches once");
        button(window, "runSelected")->click();
        window.close();
        check(window.isVisible(), "close during run keeps window alive until worker finishes");
        gate.release();
        check(waitFor([&] { return !window.isVisible(); }), "deferred close completes");
        check(calls == 3, "close skips remaining launch queue");
    }
    {
        Dashboard window(directory.path() + "/exceptions.ini", [](UpdateKind, bool) -> UpdateResult {
            throw std::runtime_error("simulated failure");
        }, false, false);
        button(window, "runSelected")->click();
        check(waitFor([&] { return button(window, "runSelected")->isEnabled(); }), "exceptions restore controls");
        check(window.findChild<QLabel *>("summary")->text().contains("3 need attention"), "exceptions produce failure results");
    }
    {
        const QString selectionPath = directory.path() + "/selection.ini";
        Dashboard window(selectionPath, [](UpdateKind, bool) {
            return UpdateResult{UpdateOutcome::Started, "Unused"};
        }, false, false);
        QLockFile diskLock(selectionPath + ".lock");
        check(diskLock.tryLock(), "simulate a busy settings file");
        QElapsedTimer clickTime;
        clickTime.start();
        button(window, "selectAll")->click();
        check(clickTime.elapsed() < 1000, "select all does not wait for locked settings file");
        check(!button(window, "runSelected")->isEnabled(), "selection updates immediately before saving");
        QElapsedTimer responsive;
        responsive.start();
        check(waitFor([&] { return responsive.elapsed() >= 350; }), "GUI processes events during background save");
        button(window, "selectAll")->click();
        button(window, "selectAll")->click();
        window.findChild<QCheckBox *>("closeWindows")->setChecked(true);
        diskLock.unlock();
        check(waitFor([&] {
            QFile file(selectionPath);
            return file.open(QIODevice::ReadOnly) && file.readAll().contains("closeUpdateWindows=true");
        }), "latest preferences save in background without closing");
        window.close();
        QSettings saved(selectionPath, QSettings::IniFormat);
        check(!saved.value("enableWindowsUpdate", true).toBool()
              && !saved.value("enableStoreUpdate", true).toBool()
              && !saved.value("enableM365Update", true).toBool(), "rapid selection changes persist final state");
    }
    if (argc > 1) {
        const QString output = QString::fromLocal8Bit(argv[1]);
        QDir().mkpath(output);
        Dashboard window(directory.path() + "/preview.ini", [](UpdateKind, bool) {
            return UpdateResult{UpdateOutcome::Started, "Preview only"};
        }, false, false);
        window.resize(560, 440);
        window.show();
        QApplication::processEvents();
        check(window.grab().save(output + "/dashboard-light.png"), "light preview captured");
        QPalette dark = app.palette();
        dark.setColor(QPalette::Window, QColor("#141b27"));
        app.setPalette(dark);
        QApplication::processEvents();
        check(window.grab().save(output + "/dashboard-dark.png"), "dark preview captured");
        auto *scroll = window.findChild<QScrollArea *>();
        auto settle = [] {
            for (int i = 0; i < 6; ++i) QApplication::processEvents();
        };
        for (const QSize size : {QSize(560, 440), QSize(440, 440), QSize(320, 500),
                                 QSize(800, 600), QSize(1280, 720), QSize(1920, 1080)}) {
            window.resize(size);
            settle();
            check(scroll->horizontalScrollBar()->maximum() == 0, "idle layout fits horizontally");
            check(scroll->verticalScrollBar()->maximum() == 0, "idle controls fit without scrolling");
            check(window.rect().contains(button(window, "runSelected")->mapTo(&window, QPoint(0, 0))), "primary action stays in viewport");
        }
        window.resize(320, 500);
        settle();
        check(window.grab().save(output + "/dashboard-compact.png"), "compact preview captured");
        const QSize originalSize = window.size();
        const int originalScroll = scroll->verticalScrollBar()->maximum();
        auto *preferences = window.findChild<QWidget *>("preferences");
        check(preferences && preferences->isVisible() && !preferences->isWindow(), "preferences are visible directly on the main panel");
        button(window, "toggleActivity")->click();
        auto *activityDialog = window.findChild<QDialog *>("activityDialog");
        check(activityDialog && activityDialog->isVisible(), "Activity opens its own window");
        settle();
        check(window.size() == originalSize && scroll->verticalScrollBar()->maximum() == originalScroll,
              "opening utility windows does not displace update sources");
        check(!window.findChild<QPlainTextEdit *>("activity")->placeholderText().isEmpty(), "empty activity explains what to do");
        check(!button(window, "copyActivity")->isEnabled(), "Copy is disabled for an empty session");
        check(activityDialog->grab().save(output + "/activity-empty.png"), "empty activity preview captured");
        window.findChild<QCheckBox *>("closeWindows")->click();
        check(window.findChild<QCheckBox *>("closeWindows")->isChecked(), "inline preference is interactive");
        check(preferences->grab().save(output + "/preferences.png"), "preferences preview captured");
        button(window, "runSelected")->click();
        check(waitFor([&] { return button(window, "runSelected")->isEnabled(); }), "layout preview run finishes");
        check(button(window, "copyActivity")->isEnabled(), "Copy enables when activity arrives");
        button(window, "copyActivity")->click();
        check(app.clipboard()->text() == window.findChild<QPlainTextEdit *>("activity")->toPlainText(), "Copy copies the full activity log");
        check(activityDialog->grab().save(output + "/activity-populated.png"), "activity results preview captured");
        for (const QSize size : {QSize(300, 240), QSize(320, 500), QSize(560, 440), QSize(1280, 720)}) {
            window.resize(size);
            settle();
            check(scroll->horizontalScrollBar()->maximum() == 0, "expanded results and preferences fit horizontally");
            const auto *run = button(window, "runSelected");
            check(window.rect().contains(QRect(run->mapTo(&window, QPoint(0, 0)), run->size())), "primary action fully visible with expanded content");
        }
        window.resize(560, 440);
        settle();
        check(window.grab().save(output + "/dashboard-expanded.png"), "expanded preview captured");
        activityDialog->close();
        settle();
        check(scroll->verticalScrollBar()->maximum() == 0, "completed results fit the default window without scrolling");
        check(window.grab().save(output + "/dashboard-results.png"), "completed results preview captured");
        button(window, "toggleActivity")->click();
        window.close();
        check(!preferences->isVisible() && !activityDialog->isVisible(), "closing main window also closes utility windows");
        QSemaphore gate;
        Dashboard busy(directory.path() + "/busy.ini", [&](UpdateKind, bool) {
            gate.acquire();
            return UpdateResult{UpdateOutcome::Attention, "Open the updater and choose Check for updates."};
        }, false, false);
        busy.resize(300, 300);
        busy.show();
        button(busy, "runSelected")->click();
        settle();
        auto *busyScroll = busy.findChild<QScrollArea *>();
        check(busyScroll->horizontalScrollBar()->maximum() == 0, "busy narrow layout fits horizontally");
        for (const char *name : {"runSelected", "stopQueued"}) {
            const auto *action = button(busy, name);
            check(busy.rect().contains(QRect(action->mapTo(&busy, QPoint(0, 0)), action->size())), "busy actions fit in narrow viewport");
        }
        check(busy.grab().save(output + "/dashboard-busy-small.png"), "small busy preview captured");
        gate.release(3);
        check(waitFor([&] { return button(busy, "runSelected")->isEnabled(); }), "small busy window completes");
    }
    std::cout << (failures ? "FAILED" : "PASSED") << ": dashboard regression checks (no real updates launched)\n";
    return failures ? 1 : 0;
}
