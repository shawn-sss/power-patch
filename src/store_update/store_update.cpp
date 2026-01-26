#include "store_update.h"

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <shellapi.h>
  #include <UIAutomation.h>
  #include <oleauto.h>
  #include <cstdint>
  #include <cwctype>
  #include <string>
  #include <vector>
  #include <array>

namespace {
constexpr int kWindowFindStepMs = 250;
constexpr int kStoreFindTimeoutMs = 3000;
constexpr int kStoreButtonPollCount = 30;
constexpr int kStoreButtonPollSleepMs = 400;
constexpr int kStoreInitialWaitMs = 5000;
constexpr int kStorePostCheckWaitMs = 5000;
constexpr int kStoreCloseWaitMs = 5000;
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

struct FindWindowData
{
    HWND found = nullptr;
};

static BOOL CALLBACK enumWindowsFindStore(HWND hwnd, LPARAM lparam)
{
    auto *data = reinterpret_cast<FindWindowData *>(lparam);
    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
    const std::wstring wtitle(title);

    if (processImageNameContains(hwnd, L"WinStore.App.exe")) {
        data->found = hwnd;
        return FALSE;
    }

    if (wcontains_insensitive(wtitle, L"microsoft store") || wcontains_insensitive(wtitle, L"windows store")) {
        data->found = hwnd;
        return FALSE;
    }

    return TRUE;
}

static HWND findMicrosoftStoreWindow(int timeoutMs)
{
    const int stepMs = kWindowFindStepMs;
    for (int waited = 0; waited <= timeoutMs; waited += stepMs) {
        FindWindowData data;
        EnumWindows(enumWindowsFindStore, reinterpret_cast<LPARAM>(&data));
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

static bool tryInvokeButtonByName(IUIAutomation *automation, IUIAutomationElement *root, const wchar_t *name)
{
    if (!automation || !root || !name)
        return false;

    IUIAutomationCondition *condBtn = nullptr;
    IUIAutomationCondition *condName = nullptr;
    IUIAutomationCondition *andCond = nullptr;

    VARIANT vType;
    VariantInit(&vType);
    vType.vt = VT_I4;
    vType.lVal = UIA_ButtonControlTypeId;

    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, vType, &condBtn)))
        return false;

    VARIANT vName;
    VariantInit(&vName);
    vName.vt = VT_BSTR;
    vName.bstrVal = SysAllocString(name);
    if (!vName.bstrVal) {
        condBtn->Release();
        return false;
    }

    const HRESULT hrName = automation->CreatePropertyCondition(UIA_NamePropertyId, vName, &condName);
    VariantClear(&vName);
    if (FAILED(hrName)) {
        condBtn->Release();
        return false;
    }

    if (FAILED(automation->CreateAndCondition(condBtn, condName, &andCond))) {
        condName->Release();
        condBtn->Release();
        return false;
    }

    IUIAutomationElement *btn = nullptr;
    const HRESULT hrFind = root->FindFirst(TreeScope_Subtree, andCond, &btn);

    andCond->Release();
    condName->Release();
    condBtn->Release();

    if (FAILED(hrFind) || !btn)
        return false;

    IUIAutomationInvokePattern *invoke = nullptr;
    const HRESULT hrPat = btn->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
    btn->Release();
    if (FAILED(hrPat) || !invoke)
        return false;

    const HRESULT hrInvoke = invoke->Invoke();
    invoke->Release();
    return SUCCEEDED(hrInvoke);
}

static std::wstring toLowerCopy(const std::wstring &input)
{
    std::wstring out = input;
    for (auto &ch : out) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return out;
}

static bool tryInvokeCheckForUpdatesButton(IUIAutomation *automation, IUIAutomationElement *root)
{
    const std::vector<const wchar_t *> candidates = {
        L"Check for updates",
        L"Check for update",
        L"Get updates",
        L"Get Updates",
    };

    for (auto *label : candidates) {
        if (tryInvokeButtonByName(automation, root, label))
            return true;
    }
    return false;
}

static bool isStoreUpdateAllButtonName(const std::wstring &name)
{
    auto lower = toLowerCopy(name);
    return lower.find(L"update all") != std::wstring::npos;
}

static bool isStoreUpdateSingleButtonName(const std::wstring &name)
{
    auto lower = toLowerCopy(name);
    return lower == L"update";
}

static bool tryInvokeStoreUpdateActions(IUIAutomation *automation, IUIAutomationElement *root)
{
    if (!automation || !root)
        return false;

    VARIANT vType;
    VariantInit(&vType);
    vType.vt = VT_I4;
    vType.lVal = UIA_ButtonControlTypeId;

    IUIAutomationCondition *condBtn = nullptr;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, vType, &condBtn)))
        return false;

    IUIAutomationElementArray *buttons = nullptr;
    const HRESULT hr = root->FindAll(TreeScope_Subtree, condBtn, &buttons);
    condBtn->Release();
    if (FAILED(hr) || !buttons)
        return false;

    bool clicked = false;
    int length = 0;
    buttons->get_Length(&length);
    for (int i = 0; i < length; ++i) {
        IUIAutomationElement *btn = nullptr;
        if (FAILED(buttons->GetElement(i, &btn)) || !btn)
            continue;

        BSTR nameBstr = nullptr;
        if (SUCCEEDED(btn->get_CurrentName(&nameBstr)) && nameBstr) {
            std::wstring name(nameBstr, SysStringLen(nameBstr));
            SysFreeString(nameBstr);

            if (isStoreUpdateAllButtonName(name)) {
                IUIAutomationInvokePattern *invoke = nullptr;
                const HRESULT hrPat = btn->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
                btn->Release();
                if (SUCCEEDED(hrPat) && invoke) {
                    const HRESULT hrInvoke = invoke->Invoke();
                    invoke->Release();
                    if (SUCCEEDED(hrInvoke)) {
                        buttons->Release();
                        return true;
                    }
                }
                continue;
            }
        }

        btn->Release();
    }

    for (int i = 0; i < length; ++i) {
        IUIAutomationElement *btn = nullptr;
        if (FAILED(buttons->GetElement(i, &btn)) || !btn)
            continue;

        BSTR nameBstr = nullptr;
        if (SUCCEEDED(btn->get_CurrentName(&nameBstr)) && nameBstr) {
            std::wstring name(nameBstr, SysStringLen(nameBstr));
            SysFreeString(nameBstr);

            if (isStoreUpdateSingleButtonName(name)) {
                IUIAutomationInvokePattern *invoke = nullptr;
                const HRESULT hrPat = btn->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
                btn->Release();
                if (SUCCEEDED(hrPat) && invoke) {
                    const HRESULT hrInvoke = invoke->Invoke();
                    invoke->Release();
                    if (SUCCEEDED(hrInvoke)) {
                        clicked = true;
                        continue;
                    }
                }
                continue;
            }
        }

        btn->Release();
    }

    buttons->Release();
    return clicked;
}

bool openMicrosoftStoreLibrary()
{
    auto tryShell = [](const wchar_t *uri) -> bool {
        const auto rc = reinterpret_cast<std::intptr_t>(
            ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL));
        return rc > 32;
    };

    if (tryShell(L"ms-windows-store://downloadsandupdates"))
        return true;

    if (tryShell(L"ms-windows-store://home"))
        return true;

    return false;
}

bool clickMicrosoftStoreGetUpdates(bool closeAfter)
{
    HWND storeHwnd = findMicrosoftStoreWindow(kStoreFindTimeoutMs);
    if (!storeHwnd)
        return false;

    Sleep(kStoreInitialWaitMs);

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

    IUIAutomationElement *windowEl = nullptr;
    hr = automation->ElementFromHandle(storeHwnd, &windowEl);
    if (FAILED(hr) || !windowEl) {
        automation->Release();
        if (didInit) CoUninitialize();
        return false;
    }

    bool clickedCheck = false;
    for (int i = 0; i < kStoreButtonPollCount && !clickedCheck; ++i) {
        clickedCheck = tryInvokeCheckForUpdatesButton(automation, windowEl);
        if (!clickedCheck)
            Sleep(kStoreButtonPollSleepMs);
    }

    Sleep(kStorePostCheckWaitMs);

    bool clickedUpdates = false;
    for (int i = 0; i < kStoreButtonPollCount && !clickedUpdates; ++i) {
        clickedUpdates = tryInvokeStoreUpdateActions(automation, windowEl);
        if (!clickedUpdates)
            Sleep(kStoreButtonPollSleepMs);
    }

    if (closeAfter) {
        Sleep(kStoreCloseWaitMs);
        closeWindowHandle(storeHwnd);
    }

    windowEl->Release();
    automation->Release();
    if (didInit) CoUninitialize();
    return clickedCheck || clickedUpdates;
}
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

bool isMicrosoftStorePolicyDisabled()
{
    const std::array<const wchar_t *, 3> subKeys = {
        L"SOFTWARE\\Policies\\Microsoft\\WindowsStore",
        L"SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer",
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
    };
    const std::array<const wchar_t *, 3> valueNames = {
        L"RemoveWindowsStore",
        L"DisableWindowsStore",
        L"NoWindowsStore",
    };

    for (auto root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        for (const auto *subKey : subKeys) {
            for (const auto *valueName : valueNames) {
                DWORD value = 0;
                if (readRegistryDword(root, subKey, valueName, &value) && value != 0)
                    return true;
            }
        }
    }
    return false;
}

}

MicrosoftStoreStatus queryMicrosoftStoreStatus()
{
    if (isMicrosoftStorePolicyDisabled())
        return MicrosoftStoreStatus::Disabled;

    HKEY protocolKey = nullptr;
    const LONG rc = RegOpenKeyExW(HKEY_CLASSES_ROOT, L"ms-windows-store", 0, KEY_READ | KEY_WOW64_64KEY, &protocolKey);
    if (protocolKey)
        RegCloseKey(protocolKey);

    if (rc != ERROR_SUCCESS)
        return MicrosoftStoreStatus::Uninstalled;

    return MicrosoftStoreStatus::Available;
}
#else
MicrosoftStoreStatus queryMicrosoftStoreStatus()
{
    return MicrosoftStoreStatus::Available;
}
#endif
#endif
