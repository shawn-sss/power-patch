#include "startup_guard.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <memory>
#include <string>

namespace {
struct CloseHandleDeleter {
    void operator()(void *handle) const { if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
using Handle = std::unique_ptr<void, CloseHandleDeleter>;

std::wstring imagePath(HANDLE process)
{
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &length)) return {};
    path.resize(length);
    return path;
}
bool fail(QString &error, const QString &operation)
{
    error = operation + QString(" (Windows error %1).").arg(GetLastError());
    return false;
}
}
#endif

StartupGuard::~StartupGuard() { release(); }

void StartupGuard::release()
{
#ifdef _WIN32
    if (mutex_) {
        ReleaseMutex(mutex_);
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
#endif
}

bool StartupGuard::replacePreviousInstances(QString &error)
{
#ifdef _WIN32
    Handle lock(CreateMutexW(nullptr, FALSE, L"Local\\PowerPatch.Startup"));
    if (!lock) return fail(error, "Could not create the startup lock");
    const DWORD acquired = WaitForSingleObject(lock.get(), 15000);
    if (acquired != WAIT_OBJECT_0 && acquired != WAIT_ABANDONED) {
        error = "Another Power Patch launch is still starting. Try launching again shortly.";
        return false;
    }
    mutex_ = lock.release();

    const DWORD currentPid = GetCurrentProcessId();
    DWORD session = 0;
    if (!ProcessIdToSessionId(currentPid, &session)) return fail(error, "Could not identify the current session");
    const auto ownPath = imagePath(GetCurrentProcess());
    if (ownPath.empty()) return fail(error, "Could not identify this executable");
    const auto ownName = ownPath.substr(ownPath.find_last_of(L"\\/") + 1);
    FILETIME ownCreated{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &ownCreated, &exited, &kernel, &user))
        return fail(error, "Could not identify this launch's start time");

    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snapshot.get() == INVALID_HANDLE_VALUE) return fail(error, "Could not inspect running processes");
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.get(), &entry)) return fail(error, "Could not read the process list");
    do {
        if (entry.th32ProcessID == currentPid || _wcsicmp(entry.szExeFile, ownName.c_str()) != 0) continue;
        DWORD otherSession = 0;
        if (!ProcessIdToSessionId(entry.th32ProcessID, &otherSession) || otherSession != session) continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, entry.th32ProcessID));
        if (!process) {
            if (GetLastError() == ERROR_INVALID_PARAMETER) continue; // Already exited.
            return fail(error, "Could not inspect an existing instance. Close it manually and try again");
        }
        if (WaitForSingleObject(process.get(), 0) == WAIT_OBJECT_0) continue;
        const auto otherPath = imagePath(process.get());
        if (otherPath.empty()) return fail(error, "Could not verify an existing instance's executable");
        if (_wcsicmp(ownPath.c_str(), otherPath.c_str()) != 0) continue;
        FILETIME created{};
        if (!GetProcessTimes(process.get(), &created, &exited, &kernel, &user))
            return fail(error, "Could not inspect an existing instance's start time");
        // A newer launch may already be waiting for our lock. Let it replace us
        // after our startup finishes, rather than terminating the newer request.
        if (CompareFileTime(&created, &ownCreated) > 0) continue;
        Handle target(OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID));
        if (!target) {
            if (WaitForSingleObject(process.get(), 0) == WAIT_OBJECT_0) continue;
            return fail(error, "Could not close an existing instance. Close it manually and try again");
        }
        if (!TerminateProcess(target.get(), 0) && WaitForSingleObject(target.get(), 0) != WAIT_OBJECT_0)
            return fail(error, "Could not close an existing instance");
        if (WaitForSingleObject(target.get(), 5000) != WAIT_OBJECT_0) {
            error = "An existing Power Patch process did not exit. Close it manually and try again.";
            return false;
        }
    } while (Process32NextW(snapshot.get(), &entry));
    if (GetLastError() != ERROR_NO_MORE_FILES) return fail(error, "Could not finish inspecting running processes");
#else
    Q_UNUSED(error);
#endif
    return true;
}
