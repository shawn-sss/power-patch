#include "dashboard.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QScreen>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <exception>

namespace {
// Keep each source to two lines. Full result text remains available to
// accessibility tools, in its tooltip, and in the copyable activity log.
class ResultLabel : public QLabel {
public:
    explicit ResultLabel(const QString &text) : QLabel(text) {
        setObjectName("result");
        setTextFormat(Qt::PlainText);
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(contentsRect(), Qt::AlignLeft | Qt::AlignVCenter,
            fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
    }
};
const QStringList names = {"Windows Update", "Microsoft Store", "Microsoft 365"};
const QStringList keys = {"enableWindowsUpdate", "enableStoreUpdate", "enableM365Update"};
const QStringList descriptions = {
    "System, security and drivers",
    "Apps from Microsoft Store",
    "Office Click-to-Run apps"
};
QLabel *label(const QString &text, const char *name = nullptr)
{
    auto *result = new QLabel(text);
    result->setWordWrap(true);
    result->setTextFormat(Qt::PlainText);
    result->setMinimumWidth(0);
    result->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    if (name) result->setObjectName(name);
    return result;
}
}

Dashboard::Dashboard(const QString &settingsPath, Launcher launcher, bool enableTray, bool migrateLegacy)
    : settings_(settingsPath, QSettings::IniFormat), launcher_(std::move(launcher))
{
    setWindowTitle("Power Patch");
    setObjectName("dashboard");
    setMinimumSize(300, 240);
    resize(560, 440);
    preferenceTimer_.setSingleShot(true);
    preferenceTimer_.setInterval(200);
    connect(&preferenceTimer_, &QTimer::timeout, this, [this] {
        if (preferenceSave_.isRunning()) {
            preferenceTimer_.start();
            return;
        }
        QVariantMap values;
        for (int i = 0; i < 3; ++i) values.insert(keys[i], cards_[i].selected->isChecked());
        values.insert("closeUpdateWindows", closeWindows_->isChecked());
        values.insert("trayOnClose", trayOnClose_->isChecked());
        preferenceSave_.setFuture(QtConcurrent::run([path = settings_.fileName(), values] {
            QSettings saved(path, QSettings::IniFormat);
            for (auto it = values.cbegin(); it != values.cend(); ++it) saved.setValue(it.key(), it.value());
            saved.sync();
            return saved.status();
        }));
    });
    connect(&preferenceSave_, &QFutureWatcher<QSettings::Status>::finished, this, [this] {
        settingsWarning_->setVisible(preferenceSave_.result() != QSettings::NoError);
    });
    // Import the original key=value file once without overwriting newer preferences.
    if (migrateLegacy && !settings_.value("legacyImported", false).toBool()) {
        QFile legacy(QCoreApplication::applicationDirPath() + "/settings.ini");
        if (legacy.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&legacy);
            const QStringList allowed = keys + QStringList{"closeUpdateWindows", "trayOnClose"};
            while (!stream.atEnd()) {
                const auto line = stream.readLine().trimmed();
                const auto separator = line.indexOf('=');
                const auto key = line.left(separator).trimmed();
                const auto value = line.mid(separator + 1).trimmed();
                if (separator > 0 && allowed.contains(key) && !settings_.contains(key)
                    && (value == "true" || value == "false"))
                    settings_.setValue(key, value == "true");
            }
        }
        settings_.setValue("legacyImported", true);
    }

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *shell = new QWidget;
    shell->setMaximumWidth(760);
    auto *shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(16, 12, 16, 12);
    shellLayout->setSpacing(8);
    outer->addStretch();
    outer->addWidget(shell, 1);
    outer->addStretch();
    auto *scroll = new QScrollArea;
    scroll->setObjectName("sourceScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 2, 0);
    layout->setSpacing(4);
    scroll->setWidget(content);

    auto *header = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(windowIcon().pixmap(30, 30));
    icon->setFixedSize(32, 32);
    header->addWidget(icon);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(label("Power Patch", "title"));
    titles->addWidget(label("Windows, Store & Office", "muted"));
    header->addLayout(titles, 1);
    auto *about = new QPushButton("About");
    about->setAccessibleName("About Power Patch");
    header->addWidget(about);
    connect(about, &QPushButton::clicked, this, [this] {
        QMessageBox::about(this, "About Power Patch",
            "<b>Power Patch " + QCoreApplication::applicationVersion() + "</b><br><br>"
            "Start Windows, Microsoft Store and Microsoft 365 update checks from one place. "
            "Download and installation progress remains in each updater.<br><br>Author: Shawn SSS");
    });
    shellLayout->addLayout(header);
    shellLayout->addWidget(scroll, 1);

    // Primary actions stay outside the scrolling area, even on short displays.
    summary_ = label("Ready", "summary");
    shellLayout->addWidget(summary_);
    progress_ = new QProgressBar;
    progress_->setObjectName("launchProgress");
    progress_->setAccessibleName("Completed update launch attempts");
    progress_->setRange(0, 3);
    progress_->setValue(0);
    progress_->setFormat("%v of %m launch attempts finished");
    progress_->hide();
    shellLayout->addWidget(progress_);
    actions_ = new QBoxLayout(QBoxLayout::LeftToRight);
    runSelected_ = new QPushButton("Run selected updates");
    runSelected_->setObjectName("runSelected");
    runSelected_->setMinimumHeight(32);
    runSelected_->setShortcut(QKeySequence("Ctrl+Return"));
    runSelected_->setToolTip("Start the selected updaters (Ctrl+Enter)");
    actions_->addWidget(runSelected_, 1);
    stop_ = new QPushButton("Stop queued");
    stop_->setObjectName("stopQueued");
    stop_->setToolTip("Skip pending launches. The current operation and updates already started will continue.");
    stop_->hide();
    actions_->addWidget(stop_);
    shellLayout->addLayout(actions_);

    auto *sources = new QHBoxLayout;
    selection_ = label("", "selection");
    sources->addWidget(selection_, 1);
    selectAll_ = new QPushButton("Select all");
    selectAll_->setObjectName("selectAll");
    sources->addWidget(selectAll_);
    layout->addLayout(sources);

    for (int i = 0; i < 3; ++i) {
        auto *frame = new QFrame;
        frame->setObjectName("sourceRow");
        auto *cardLayout = new QVBoxLayout(frame);
        cardLayout->setContentsMargins(0, 6, 0, 8);
        cardLayout->setSpacing(2);
        auto *row = new QHBoxLayout;
        auto &card = cards_[i];
        card.selected = new QCheckBox(names[i]);
        card.selected->setObjectName(keys[i]);
        card.selected->setAccessibleName("Include " + names[i]);
        card.selected->setChecked(settings_.value(keys[i], true).toBool());
        row->addWidget(card.selected, 1);
        card.run = new QPushButton("Run");
        card.run->setObjectName(QString("run%1").arg(i));
        card.run->setAccessibleName("Run " + names[i]);
        row->addWidget(card.run);
        cardLayout->addLayout(row);
        card.selected->setToolTip(descriptions[i]);
        card.detail = new ResultLabel(settings_.value(QString("lastResult/%1").arg(i), descriptions[i]).toString());
        card.detail->setToolTip(card.detail->text());
        cardLayout->addWidget(card.detail);
        layout->addWidget(frame);
        connect(card.selected, &QCheckBox::toggled, this, [this] {
            refreshControls();
            queuePreferenceSave();
        });
        connect(card.run, &QPushButton::clicked, this, [this, i] { start({i}); });
    }

    auto *preferences = new QWidget;
    preferences->setObjectName("preferences");
    auto *prefs = new QVBoxLayout(preferences);
    prefs->setContentsMargins(0, 2, 0, 2);
    prefs->setSpacing(2);
    closeWindows_ = new QCheckBox("Auto-close updater windows");
    closeWindows_->setObjectName("closeWindows");
    closeWindows_->setToolTip("Close Windows/Store after requesting updates; wait for Office completion. Manual-action windows stay open.");
    closeWindows_->setChecked(settings_.value("closeUpdateWindows", false).toBool());
    prefs->addWidget(closeWindows_);
    trayOnClose_ = new QCheckBox("Keep in tray when closed");
    trayOnClose_->setObjectName("trayOnClose");
    trayOnClose_->setChecked(settings_.value("trayOnClose", false).toBool());
    prefs->addWidget(trayOnClose_);
    settingsWarning_ = label("Preferences could not be saved. Check access to the settings folder.", "settingsWarning");
    settingsWarning_->hide();
    shellLayout->insertWidget(2, settingsWarning_);
    connect(closeWindows_, &QCheckBox::toggled, this, &Dashboard::queuePreferenceSave);
    connect(trayOnClose_, &QCheckBox::toggled, this, &Dashboard::queuePreferenceSave);

    preferencesLayout_ = new QBoxLayout(QBoxLayout::LeftToRight);
    preferencesLayout_->setSpacing(4);
    preferencesLayout_->addWidget(preferences, 1);
    auto openPanel = [this](QDialog *panel) {
        if (auto *display = screen()) {
            const QRect bounds = display->availableGeometry().adjusted(8, 8, -8, -40);
            panel->resize(panel->size().boundedTo(bounds.size()));
            const QPoint center = frameGeometry().center() - panel->rect().center();
            panel->move(qBound(bounds.left(), center.x(), qMax(bounds.left(), bounds.right() - panel->width())),
                        qBound(bounds.top(), center.y(), qMax(bounds.top(), bounds.bottom() - panel->height())));
        }
        panel->show();
        panel->raise();
        panel->activateWindow();
    };
    auto *toggleActivity = new QPushButton("Activity...");
    toggleActivity->setObjectName("toggleActivity");
    preferencesLayout_->addWidget(toggleActivity, 0, Qt::AlignLeft | Qt::AlignVCenter);
    layout->addLayout(preferencesLayout_);
    auto *activityDialog = new QDialog(this);
    activityDialog->setObjectName("activityDialog");
    activityDialog->setWindowTitle("Power Patch - Activity");
    activityDialog->setMinimumSize(280, 180);
    activityDialog->resize(540, 320);
    auto *activityLayout = new QVBoxLayout(activityDialog);
    activityLayout->addWidget(label("Activity from this session", "sectionTitle"));
    auto *copy = new QPushButton("Copy activity");
    copy->setObjectName("copyActivity");
    copy->setAccessibleName("Copy activity");
    copy->setEnabled(false);
    activity_ = new QPlainTextEdit;
    activity_->setObjectName("activity");
    activity_->setAccessibleName("Update launch activity log");
    activity_->setReadOnly(true);
    activity_->setMaximumBlockCount(200);
    activity_->setPlaceholderText("No activity this session.\n\nRun an update check to see its progress here. Previous results remain on the main window.");
    activityLayout->addWidget(activity_, 1);
    auto *activityButtons = new QDialogButtonBox(QDialogButtonBox::Close);
    activityButtons->addButton(copy, QDialogButtonBox::ActionRole);
    activityLayout->addWidget(activityButtons);
    connect(activityButtons, &QDialogButtonBox::rejected, activityDialog, &QDialog::close);
    connect(toggleActivity, &QPushButton::clicked, this, [openPanel, activityDialog] { openPanel(activityDialog); });
    connect(copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(activity_->toPlainText());
    });
    connect(activity_, &QPlainTextEdit::textChanged, this, [this, copy] {
        copy->setEnabled(!activity_->toPlainText().isEmpty());
    });
    layout->addStretch();

    connect(runSelected_, &QPushButton::clicked, this, [this] {
        QList<int> selected;
        for (int i = 0; i < 3; ++i)
            if (cards_[i].selected->isChecked()) selected.append(i);
        start(selected);
    });
    connect(selectAll_, &QPushButton::clicked, this, [this] {
        const bool all = cards_[0].selected->isChecked() && cards_[1].selected->isChecked() && cards_[2].selected->isChecked();
        for (auto &card : cards_) {
            const QSignalBlocker blocker(card.selected);
            card.selected->setChecked(!all);
        }
        refreshControls();
        queuePreferenceSave();
    });
    connect(stop_, &QPushButton::clicked, this, &Dashboard::stopPending);
    connect(&watcher_, &QFutureWatcher<UpdateResult>::finished, this, [this] {
        const auto result = watcher_.result();
        const int i = current_;
        const bool started = result.outcome == UpdateOutcome::Started;
        if (!started) ++attention_;
        const QString badge = started ? "Requested" : result.outcome == UpdateOutcome::Unavailable ? "Unavailable"
            : result.outcome == UpdateOutcome::Failed ? "Failed" : "Action needed";
        setBadge(i, badge, started ? "success" : "attention");
        const QString detail = badge + ": " + result.message + " (" + QDateTime::currentDateTime().toString("MMM d, h:mm AP") + ")";
        cards_[i].detail->setText(detail);
        cards_[i].detail->setToolTip(detail);
        cards_[i].detail->show();
        settings_.setValue(QString("lastResult/%1").arg(i), detail);
        saveSettings();
        record(i, result.message);
        ++completed_;
        progress_->setValue(completed_);
        current_ = -1;
        next();
    });

    if (enableTray && QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_ = new QSystemTrayIcon(windowIcon(), this);
        tray_->setToolTip("Power Patch - Ready");
        auto *menu = new QMenu(this);
        auto showWindow = [this] { showNormal(); raise(); activateWindow(); };
        connect(menu->addAction("Open Power Patch"), &QAction::triggered, this, showWindow);
        trayRun_ = menu->addAction("Run selected updates");
        connect(trayRun_, &QAction::triggered, runSelected_, &QPushButton::click);
        menu->addSeparator();
        connect(menu->addAction("Exit"), &QAction::triggered, this, [this] {
            quitting_ = true;
            close();
        });
        tray_->setContextMenu(menu);
        connect(tray_, &QSystemTrayIcon::activated, this, [showWindow](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) showWindow();
        });
        tray_->show();
    } else {
        trayOnClose_->setEnabled(false);
        trayOnClose_->setToolTip("The system tray is unavailable in this session.");
    }

    // Previous layouts saved oversized windows. Start compact once after this redesign.
    if (settings_.value("compactLayout", false).toBool() && settings_.contains("geometry"))
        restoreGeometry(settings_.value("geometry").toByteArray());
    settings_.setValue("compactLayout", true);
    if (auto *screen = this->screen()) {
        const QRect bounds = screen->availableGeometry();
        const QSize available = bounds.size() - QSize(16, 48);
        setMinimumSize(qMin(300, available.width()), qMin(240, available.height()));
        resize(size().boundedTo(available));
        move(qBound(bounds.left(), x(), qMax(bounds.left(), bounds.right() - width())),
             qBound(bounds.top(), y(), qMax(bounds.top(), bounds.bottom() - height() - 32)));
    }
    applyTheme();
    connect(qApp, &QGuiApplication::paletteChanged, this, [this] { applyTheme(); });
    refreshControls();
    saveSettings();
}

Dashboard::~Dashboard()
{
    flushPreferences();
    // Workers own only value data; they never dereference this window.
    watcher_.waitForFinished();
}

void Dashboard::queuePreferenceSave()
{
    preferenceTimer_.start();
}

void Dashboard::flushPreferences()
{
    const bool pending = preferenceTimer_.isActive();
    preferenceTimer_.stop();
    // Only shutdown waits for disk access. Preserve the latest click even if
    // the window closes before the debounce timer or an older save finishes.
    preferenceSave_.waitForFinished();
    if (pending) {
        for (int i = 0; i < 3; ++i) settings_.setValue(keys[i], cards_[i].selected->isChecked());
        settings_.setValue("closeUpdateWindows", closeWindows_->isChecked());
        settings_.setValue("trayOnClose", trayOnClose_->isChecked());
        saveSettings();
    }
}

void Dashboard::saveSettings()
{
    settings_.sync();
    settingsWarning_->setVisible(settings_.status() != QSettings::NoError);
}

void Dashboard::refreshControls()
{
    int selected = 0;
    for (auto &card : cards_) {
        selected += card.selected->isChecked() ? 1 : 0;
        card.selected->setEnabled(!busy_);
        card.run->setEnabled(!busy_);
    }
    selection_->setText(QString("%1 of 3 selected").arg(selected));
    selectAll_->setText(selected == 3 ? "Deselect all" : "Select all");
    selectAll_->setEnabled(!busy_);
    runSelected_->setEnabled(!busy_ && selected > 0);
    runSelected_->setText(busy_ ? "Starting checks..." : "Run selected updates");
    closeWindows_->setEnabled(!busy_);
    stop_->setVisible(busy_);
    stop_->setEnabled(busy_ && !pending_.isEmpty() && !stopping_);
    if (trayRun_) trayRun_->setEnabled(runSelected_->isEnabled());
    if (!busy_ && completed_ == 0 && !stopping_)
        summary_->setText(selected ? "Ready - progress opens in each updater" : "Select a source, or use its Run button");
}

void Dashboard::setBadge(int index, const QString &text, const QString &state)
{
    Q_UNUSED(state);
    cards_[index].detail->setText(text);
    cards_[index].detail->setToolTip(text);
}

void Dashboard::record(int index, const QString &message)
{
    activity_->appendPlainText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        + "  " + names[index] + ": " + message);
}

void Dashboard::start(QList<int> indices)
{
    if (busy_ || indices.isEmpty()) return;
    busy_ = true;
    stopping_ = false;
    completed_ = attention_ = 0;
    total_ = indices.size();
    pending_ = std::move(indices);
    closeAfter_ = closeWindows_->isChecked();
    progress_->setRange(0, total_);
    progress_->setValue(0);
    progress_->show();
    for (int i : pending_) setBadge(i, "Queued", "idle");
    next();
}

void Dashboard::next()
{
    if (pending_.isEmpty()) { finish(); return; }
    current_ = pending_.takeFirst();
    const int index = current_;
    setBadge(index, "Starting", "running");
    summary_->setText(QString("Starting %1 (%2 of %3)").arg(names[index]).arg(completed_ + 1).arg(total_));
    if (tray_) tray_->setToolTip("Power Patch - Starting " + names[index]);
    record(index, "Starting update check...");
    refreshControls();
    // Snapshot all inputs. QFutureWatcher delivers results on the GUI thread.
    watcher_.setFuture(QtConcurrent::run([launcher = launcher_, index, closeAfter = closeAfter_] {
        try {
            return launcher(static_cast<UpdateKind>(index), closeAfter);
        } catch (const std::exception &) {
            return UpdateResult{UpdateOutcome::Failed, "An unexpected error interrupted this launch. Try again."};
        } catch (...) {
            return UpdateResult{UpdateOutcome::Failed, "The updater could not complete its launch request. Try again."};
        }
    }));
}

void Dashboard::stopPending()
{
    if (!busy_) return;
    stopping_ = true;
    for (int i : pending_) {
        setBadge(i, "Skipped", "idle");
        record(i, "Skipped before launch.");
    }
    pending_.clear();
    summary_->setText(quitting_ ? "Closing after the current launch finishes..."
                              : "Queued launches stopped. Waiting for the current operation...");
    refreshControls();
}

void Dashboard::finish()
{
    busy_ = false;
    progress_->hide();
    summary_->setText(stopping_
        ? QString("Stopped queue - %1 of %2 launch attempts finished").arg(completed_).arg(total_)
        : attention_ ? QString("Launch attempts finished - %1 need attention").arg(attention_)
                     : "Update requests sent - follow progress in each updater");
    if (tray_) tray_->setToolTip("Power Patch - " + summary_->text());
    refreshControls();
    if (quitting_) QTimer::singleShot(0, this, [this] { close(); });
}

void Dashboard::closeEvent(QCloseEvent *event)
{
    flushPreferences();
    settings_.setValue("geometry", saveGeometry());
    saveSettings();
    if (!quitting_ && tray_ && QSystemTrayIcon::isSystemTrayAvailable() && trayOnClose_->isChecked()) {
        for (auto *panel : findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) panel->hide();
        hide();
        event->ignore();
        return;
    }
    if (busy_) {
        quitting_ = true;
        stopPending();
        showNormal();
        event->ignore();
        return;
    }
    if (tray_) tray_->hide();
    for (auto *panel : findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) panel->close();
    event->accept();
}

void Dashboard::applyTheme()
{
    const bool dark = qApp->palette().color(QPalette::Window).lightness() < 128;
    const QString bg = dark ? "#141b27" : "#f3f6fb";
    const QString surface = dark ? "#1d2736" : "#ffffff";
    const QString ink = dark ? "#e9eef8" : "#17243b";
    const QString muted = dark ? "#a9b8ce" : "#556780";
    const QString border = dark ? "#35465d" : "#d5dfed";
    setStyleSheet(QString(
        "QWidget { color: %1; }"
        "QWidget#dashboard, QScrollArea, QScrollArea > QWidget > QWidget { background: %2; }"
        "QDialog { background: %2; }"
        "QLabel { background: transparent; }"
        "QLabel#title { font-size: 16pt; font-weight: 600; }"
        "QLabel#summary { color: %4; }"
        "QLabel#eyebrow { font-size: 9pt; font-weight: 700; color: %4; }"
        "QLabel#muted, QLabel#result, QLabel#selection { color: %4; }"
        "QLabel#sectionTitle { font-weight: 600; }"
        "QFrame#sourceRow { background: transparent; border: none; border-bottom: 1px solid %5; }"
        "QPushButton { background: %3; border: 1px solid %5; border-radius: 4px; padding: 4px 9px; }"
        "QPushButton#selectAll { background: transparent; border-color: transparent; color: %4; }"
        "QPushButton:hover { border-color: #588ce5; }"
        "QPushButton:pressed { background: %2; }"
        "QPushButton:focus, QCheckBox:focus { outline: none; border: 2px solid #588ce5; }"
        "QPushButton:disabled { color: %4; background: %2; }"
        "QPushButton#runSelected { color: white; background: #285fd0; border-color: #285fd0; font-weight: 600; }"
        "QPushButton#runSelected:hover { background: #2053bd; }"
        "QPushButton#runSelected:disabled { background: %5; border-color: %5; color: %4; }"
        "QCheckBox { spacing: 7px; padding: 2px; background: transparent; }"
        "QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid %5; border-radius: 3px; background: %2; }"
        "QCheckBox::indicator:checked { background: #285fd0; border-color: #285fd0; image: url(:/icons/assets/img/check_dark.png); }"
        "QCheckBox:disabled { color: %4; }"
        "QLabel#badge { background: transparent; color: %4; }"
        "QLabel#settingsWarning { background: #61461d; color: #fff0cb; }"
        "QProgressBar { border: 1px solid %5; border-radius: 5px; background: %2; text-align: center; min-height: 20px; }"
        "QProgressBar::chunk { background: %5; border-radius: 4px; }"
        "QPlainTextEdit { background: %3; border: 1px solid %5; border-radius: 6px; padding: 8px; }"
    ).arg(ink, bg, surface, muted, border));
}

void Dashboard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (preferencesLayout_) {
        const int requiredWidth = closeWindows_->sizeHint().width()
            + findChild<QPushButton *>("toggleActivity")->sizeHint().width() + 48;
        preferencesLayout_->setDirection(width() < requiredWidth ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }
    if (actions_) {
        const int requiredWidth = runSelected_->sizeHint().width() + stop_->sizeHint().width() + 48;
        actions_->setDirection(width() < requiredWidth ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }
}
