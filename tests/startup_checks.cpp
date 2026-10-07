#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <array>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    const QString probe = QCoreApplication::applicationDirPath() + "/startup_probe.exe";
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QString otherProbe = directory.path() + "/startup_probe.exe";
    if (!QFile::copy(probe, otherProbe)) return 1;
    auto start = [&](QProcess &process, const QString &path, const QStringList &args) {
        process.start(path, args);
        const bool ready = process.waitForStarted(3000) && process.waitForReadyRead(5000)
            && process.readAllStandardOutput().contains("READY");
        check(ready, "probe starts successfully");
        if (!ready) std::cerr << process.readAllStandardError().constData();
    };
    auto stop = [](QProcess &process) {
        if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(3000); }
    };
    QProcess unrelated;
    start(unrelated, otherProbe, {"--hold"});
    std::array<QProcess, 3> existing;
    for (auto &process : existing) start(process, probe, {"--hold"});
    QProcess replacement;
    start(replacement, probe, {});
    for (auto &process : existing)
        check(process.state() == QProcess::NotRunning || process.waitForFinished(3000), "all older matching instances exit");
    check(replacement.state() == QProcess::Running, "replacement stays running");
    check(!unrelated.waitForFinished(50), "same filename in another directory is left running");
    QProcess next;
    start(next, probe, {});
    check(replacement.state() == QProcess::NotRunning || replacement.waitForFinished(3000), "a second launch replaces the previous launch");
    check(next.state() == QProcess::Running, "newest launch stays running");
    std::array<QProcess, 3> racing;
    for (auto &process : racing) {
        process.start(probe, QStringList{});
        check(process.waitForStarted(3000), "concurrent launch starts");
    }
    QElapsedTimer timer;
    timer.start();
    int running = 0;
    do {
        QCoreApplication::processEvents();
        running = next.state() == QProcess::Running ? 1 : 0;
        for (auto &process : racing) running += process.state() == QProcess::Running ? 1 : 0;
        if (running == 1) break;
        QThread::msleep(10);
    } while (timer.elapsed() < 5000);
    check(running == 1, "concurrent launches leave exactly one instance");
    check(!unrelated.waitForFinished(50), "unrelated copy survives all replacements");
    for (auto &process : racing) stop(process);
    for (auto &process : existing) stop(process);
    stop(next);
    stop(replacement);
    stop(unrelated);
    std::cout << (failures ? "FAILED" : "PASSED") << ": startup replacement checks\n";
    return failures ? 1 : 0;
}
