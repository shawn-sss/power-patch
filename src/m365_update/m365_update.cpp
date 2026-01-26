#include "m365_update.h"

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <UIAutomation.h>
  #include <oleauto.h>
  #include <cwctype>
  #include <string>
  #include <vector>
  #include <array>

namespace {
constexpr int kWindowFindStepMs = 250;
constexpr int kProcessWindowFindTimeoutMs = 3000;
constexpr int kOfficeWindowFindTimeoutMs = 10000;
constexpr int kOfficePollSleepMs = 500;
constexpr int kOfficeNoProgressGraceMs = 3000;
constexpr int kOfficeMaxWaitMs = 30 * 60 * 1000;
}

static std::wstring getEnvVar(const wchar_t *name)
{
    if (!name)
        return L"";

    DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0)
        return L"";

    std::wstring buf;
    buf.resize(static_cast<size_t>(needed));
    DWORD written = GetEnvironmentVariableW(name, buf.data(), needed);
    if (written == 0)
        return L"";

    buf.resize(static_cast<size_t>(written));
    return buf;
}

static bool fileExists(const std::wstring &path)
{
    if (path.empty())
        return false;
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
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

static bool runNormalProcess(const std::wstring &exePath, const std::wstring &args)
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
    si.wShowWindow = SW_SHOWNORMAL;

    PROCESS_INFORMATION pi{};

    const BOOL ok = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
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

static std::wstring officeC2RClientPath()
{
    const std::wstring suffix = L"\\Common Files\\Microsoft Shared\\ClickToRun\\OfficeC2RClient.exe";

    const std::wstring programFiles = getEnvVar(L"ProgramFiles");
    const std::wstring programFilesX86 = getEnvVar(L"ProgramFiles(x86)");

    std::vector<std::wstring> roots;
    if (!programFiles.empty())
        roots.push_back(programFiles);
    if (!programFilesX86.empty() && programFilesX86 != programFiles)
        roots.push_back(programFilesX86);

    roots.push_back(L"C:\\Program Files");
    roots.push_back(L"C:\\Program Files (x86)");

    for (const auto &root : roots) {
        std::wstring candidate = root;
        candidate += suffix;
        if (fileExists(candidate))
            return candidate;
    }

    return L"";
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

static HWND findWindowByProcessName(const wchar_t *exeName, int timeoutMs)
{
    if (!exeName || timeoutMs <= 0)
        return nullptr;

    const int stepMs = kWindowFindStepMs;
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

static void closeWindowHandle(HWND hwnd)
{
    if (!hwnd)
        return;
    if (!IsWindow(hwnd))
        return;
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

static bool hasOfficeUpdateProgress(IUIAutomation *automation, IUIAutomationElement *root)
{
    if (!automation || !root)
        return false;

    VARIANT vType;
    VariantInit(&vType);
    vType.vt = VT_I4;
    vType.lVal = UIA_ProgressBarControlTypeId;

    IUIAutomationCondition *cond = nullptr;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, vType, &cond)))
        return false;

    IUIAutomationElement *progress = nullptr;
    const HRESULT hrFind = root->FindFirst(TreeScope_Subtree, cond, &progress);
    cond->Release();
    if (FAILED(hrFind) || !progress)
        return false;

    progress->Release();
    return true;
}

static bool waitForOfficeUpdateCompletion()
{
    HWND hwnd = findWindowByProcessName(L"OfficeC2RClient.exe", kOfficeWindowFindTimeoutMs);
    if (!hwnd)
        return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool didInit = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE)
        ;
    else if (FAILED(hr))
        return false;

    IUIAutomation *automation = nullptr;
    hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation));
    if (FAILED(hr) || !automation) {
        if (didInit) CoUninitialize();
        return false;
    }

    int noProgressMs = 0;
    const DWORD startTick = GetTickCount();
    while (GetTickCount() - startTick < static_cast<DWORD>(kOfficeMaxWaitMs)) {
        if (!IsWindow(hwnd)) {
            automation->Release();
            if (didInit) CoUninitialize();
            return true;
        }

        IUIAutomationElement *windowEl = nullptr;
        hr = automation->ElementFromHandle(hwnd, &windowEl);
        if (FAILED(hr) || !windowEl) {
            Sleep(kOfficePollSleepMs);
            continue;
        }

        const bool hasProgress = hasOfficeUpdateProgress(automation, windowEl);
        windowEl->Release();
        if (hasProgress) {
            noProgressMs = 0;
        } else {
            noProgressMs += kOfficePollSleepMs;
            if (noProgressMs >= kOfficeNoProgressGraceMs) {
                automation->Release();
                if (didInit) CoUninitialize();
                return true;
            }
        }

        Sleep(kOfficePollSleepMs);
    }

    automation->Release();
    if (didInit) CoUninitialize();
    return false;
}

bool startMicrosoft365Update()
{
    const auto exe = officeC2RClientPath();
    if (exe.empty())
        return false;

    if (runNormalProcess(exe, L"/update user"))
        return true;

    if (runHiddenProcess(exe, L"/update user displaylevel=false"))
        return true;

    return false;
}

bool closeMicrosoft365UpdateAfterCompletion(int waitAfterMs)
{
    const bool finished = waitForOfficeUpdateCompletion();
    if (!finished)
        return false;

    if (waitAfterMs > 0)
        Sleep(waitAfterMs);

    closeWindowByProcessAfterDelay(L"OfficeC2RClient.exe", 0);
    return true;
}

void closeWindowByProcessAfterDelay(const wchar_t *exeName, int delayMs)
{
    if (!exeName)
        return;
    if (delayMs > 0)
        Sleep(delayMs);

    HWND hwnd = findWindowByProcessName(exeName, kProcessWindowFindTimeoutMs);
    if (hwnd)
        closeWindowHandle(hwnd);
}
#endif

#ifdef _WIN32
namespace {

bool readRegistryDword(HKEY root, const wchar_t *subKey, const wchar_t *valueName, DWORD *outValue)
{
    if (!subKey || !valueName || !outValue)
        return false;

    const std::array<REGSAM, 3> accessMasks = {
        KEY_READ | KEY_WOW64_64KEY,
        KEY_READ | KEY_WOW64_32KEY,
        KEY_READ,
    };

    for (const REGSAM mask : accessMasks) {
        HKEY key = nullptr;
        const LONG rc = RegOpenKeyExW(root, subKey, 0, mask, &key);
        if (rc != ERROR_SUCCESS)
            continue;

        DWORD data = 0;
        DWORD type = 0;
        DWORD size = sizeof(data);
        const LONG queryRc = RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(&data), &size);
        RegCloseKey(key);
        if (queryRc != ERROR_SUCCESS || type != REG_DWORD)
            continue;

        *outValue = data;
        return true;
    }

    return false;
}

}

Microsoft365UpdateStatus queryMicrosoft365UpdateStatus()
{
    const auto exe = officeC2RClientPath();
    if (exe.empty())
        return Microsoft365UpdateStatus::NotInstalled;

    DWORD updatesEnabled = 0;
    if (readRegistryDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Office\\ClickToRun\\Configuration", L"UpdatesEnabled", &updatesEnabled) &&
        updatesEnabled == 0) {
        return Microsoft365UpdateStatus::Disabled;
    }

    return Microsoft365UpdateStatus::Ready;
}
#else
Microsoft365UpdateStatus queryMicrosoft365UpdateStatus()
{
    return Microsoft365UpdateStatus::Ready;
}
#endif
