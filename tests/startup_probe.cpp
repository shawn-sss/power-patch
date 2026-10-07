#include "startup_guard.h"
#include <QCoreApplication>
#include <QThread>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    StartupGuard guard;
    if (!app.arguments().contains("--hold")) {
        QString error;
        if (!guard.replacePreviousInstances(error)) {
            std::cerr << error.toStdString() << std::endl;
            return 1;
        }
    }
    guard.release();
    std::cout << "READY" << std::endl;
    QThread::sleep(60);
    return 0;
}
