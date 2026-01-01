#include "windows_update.h"

#include "constants.h"

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <shellapi.h>
  #include <cstdint>
  #include <cwctype>
  #include <iterator>
  #include <string>

static std::wstring systemExePath(const wchar_t *exeName)
{
    wchar_t sysDir[MAX_PATH]{};
    const UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return L"";

    std::wstring path(sysDir);
    path += L"\\";
    path += exeName;
    return path;
}

static bool runHiddenProcess(const std::wstring &exePath, const std::wstring &args)
{
    if (exePath.empty())
        return false;

    std::wstring cmd = L"\"";
    cmd += exePath;
    cmd += L"\"";
    if (!args.empty()) {
        cmd += L" ";
        cmd += args;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};

    const BOOL ok = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!ok)
        return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static bool wcontains_insensitive(std::wstring haystack, std::wstring needle)
{
    auto tolower_inplace = [](std::wstring &s) {
        for (auto &ch : s) {
            ch = static_cast<wchar_t>(towlower(ch));
        }
    };
    tolower_inplace(haystack);
    tolower_inplace(needle);
    return haystack.find(needle) != std::wstring::npos;
}

static bool processImageNameContains(HWND hwnd, const wchar_t *needle)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
        return false;

    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return false;

    wchar_t pathBuf[MAX_PATH * 4]{};
    DWORD sz = static_cast<DWORD>(std::size(pathBuf));
    const BOOL ok = QueryFullProcessImageNameW(h, 0, pathBuf, &sz);
    CloseHandle(h);
    if (!ok)
        return false;

    return wcontains_insensitive(std::wstring(pathBuf), std::wstring(needle));
}

struct FindWindowByProcessData
{
    const wchar_t *exeName = nullptr;
    HWND found = nullptr;
};

struct FindWindowByTitleData
{
    const wchar_t *titleNeedle = nullptr;
    HWND found = nullptr;
};

static BOOL CALLBACK enumWindowsFindProcess(HWND hwnd, LPARAM lparam)
{
    auto *data = reinterpret_cast<FindWindowByProcessData *>(lparam);
    if (!data || !data->exeName)
        return TRUE;
    if (!IsWindowVisible(hwnd))
        return TRUE;

    if (processImageNameContains(hwnd, data->exeName)) {
        data->found = hwnd;
        return FALSE;
    }

    return TRUE;
}

static BOOL CALLBACK enumWindowsFindTitle(HWND hwnd, LPARAM lparam)
{
    auto *data = reinterpret_cast<FindWindowByTitleData *>(lparam);
    if (!data || !data->titleNeedle)
        return TRUE;
    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
    if (title[0] == L'\0')
        return TRUE;

    if (wcontains_insensitive(std::wstring(title), std::wstring(data->titleNeedle))) {
        data->found = hwnd;
        return FALSE;
    }

    return TRUE;
}

static HWND findWindowByProcessName(const wchar_t *exeName, int timeoutMs)
{
    if (!exeName || timeoutMs <= 0)
        return nullptr;

    const int stepMs = app_constants::kWindowFindStepMs;
    for (int waited = 0; waited <= timeoutMs; waited += stepMs) {
        FindWindowByProcessData data;
        data.exeName = exeName;
        EnumWindows(enumWindowsFindProcess, reinterpret_cast<LPARAM>(&data));
        if (data.found)
            return data.found;
        Sleep(stepMs);
    }
    return nullptr;
}

static HWND findWindowByTitleContains(const wchar_t *titleNeedle, int timeoutMs)
{
    if (!titleNeedle || timeoutMs <= 0)
        return nullptr;

    const int stepMs = app_constants::kWindowFindStepMs;
    for (int waited = 0; waited <= timeoutMs; waited += stepMs) {
        FindWindowByTitleData data;
        data.titleNeedle = titleNeedle;
        EnumWindows(enumWindowsFindTitle, reinterpret_cast<LPARAM>(&data));
        if (data.found)
            return data.found;
        Sleep(stepMs);
    }
    return nullptr;
}

static void closeWindowHandle(HWND hwnd)
{
    if (!hwnd)
        return;
    if (!IsWindow(hwnd))
        return;
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

bool openWindowsUpdateSettings()
{
    auto tryShell = [](const wchar_t *uri) -> bool {
        const auto rc = reinterpret_cast<std::intptr_t>(
            ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL));
        return rc > 32;
    };

    if (tryShell(L"ms-settings:windowsupdate-action"))
        return true;

    if (tryShell(L"ms-settings:windowsupdate"))
        return true;

    return false;
}

bool startWindowsUpdateScan()
{
    const auto uso = systemExePath(L"UsoClient.exe");
    if (!uso.empty()) {
        if (runHiddenProcess(uso, L"StartInteractiveScan"))
            return true;
        if (runHiddenProcess(uso, L"StartScan"))
            return true;
    }

    const auto wuauclt = systemExePath(L"wuauclt.exe");
    if (!wuauclt.empty()) {
        if (runHiddenProcess(wuauclt, L"/detectnow"))
            return true;
    }

    return false;
}

void closeWindowsUpdateWindowAfterDelay(int delayMs)
{
    if (delayMs > 0)
        Sleep(delayMs);

    HWND hwnd = findWindowByProcessName(L"SystemSettings.exe", app_constants::kWindowsUpdateFindTimeoutMs);
    if (!hwnd)
        hwnd = findWindowByProcessName(L"ApplicationFrameHost.exe", app_constants::kWindowsUpdateFindTimeoutMs);
    if (!hwnd)
        hwnd = findWindowByTitleContains(L"Windows Update", app_constants::kWindowsUpdateFindTimeoutMs);

    if (hwnd)
        closeWindowHandle(hwnd);
}
#endif
