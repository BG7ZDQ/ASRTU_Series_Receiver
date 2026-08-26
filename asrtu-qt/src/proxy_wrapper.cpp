#include <windows.h>

#include <string>

int wmain()
{
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
