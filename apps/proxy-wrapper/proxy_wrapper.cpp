#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

#include <string>

namespace {
constexpr wchar_t kProxyAppId[] = L"BG7ZDQ.ASRTU.Series.Proxy";

void applyTaskbarIdentity()
{
    SetCurrentProcessExplicitAppUserModelID(kProxyAppId);
    SetConsoleTitleW(L"ASRTU Telemetry Upload Proxy");

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninitializeCom = SUCCEEDED(comResult);

    // A console window is owned by Console Host on current Windows.  Setting
    // only the icon resource on this executable therefore does not give that
    // window its own taskbar identity.  Apply both the icon and AppUserModelID
    // directly to the console HWND once it has been created.
    HWND console = nullptr;
    for (int attempt = 0; attempt < 50 && console == nullptr; ++attempt) {
        console = GetConsoleWindow();
        if (console == nullptr)
            Sleep(20);
    }
    if (console == nullptr) {
        if (uninitializeCom)
            CoUninitialize();
        return;
    }

    HINSTANCE module = GetModuleHandleW(nullptr);
    HICON largeIcon = static_cast<HICON>(LoadImageW(
        module, L"IDI_APPLICATION_ICON", IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    HICON smallIcon = static_cast<HICON>(LoadImageW(
        module, L"IDI_APPLICATION_ICON", IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if (largeIcon)
        SendMessageW(console, WM_SETICON, ICON_BIG,
                     reinterpret_cast<LPARAM>(largeIcon));
    if (smallIcon)
        SendMessageW(console, WM_SETICON, ICON_SMALL,
                     reinterpret_cast<LPARAM>(smallIcon));

    IPropertyStore* properties = nullptr;
    if (SUCCEEDED(SHGetPropertyStoreForWindow(
            console, IID_PPV_ARGS(&properties)))) {
        PROPVARIANT value{};
        if (SUCCEEDED(InitPropVariantFromString(kProxyAppId, &value))) {
            properties->SetValue(PKEY_AppUserModel_ID, value);
            properties->Commit();
            PropVariantClear(&value);
        }
        properties->Release();
    }
    if (uninitializeCom)
        CoUninitialize();
}
}

int wmain()
{
    applyTaskbarIdentity();

    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return 127;

    std::wstring directory(modulePath, length);
    const auto separator = directory.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
        return 127;
    directory.resize(separator);

    const std::wstring executable = directory + L"\\proxy_mmt_gui.exe";
    std::wstring commandLine = L"\"" + executable + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr,
                        TRUE, 0, nullptr, directory.c_str(), &startup, &process)) {
        return int(GetLastError());
    }

    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    return int(exitCode);
}
