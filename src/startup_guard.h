#pragma once
#include <QString>

// Serializes startup until the replacement window is ready. Only older
// instances of this executable in the current Windows session are stopped.
class StartupGuard {
public:
    StartupGuard() = default;
    ~StartupGuard();
    StartupGuard(const StartupGuard &) = delete;
    StartupGuard &operator=(const StartupGuard &) = delete;
    bool replacePreviousInstances(QString &error);
    void release();
private:
    void *mutex_ = nullptr;
};
