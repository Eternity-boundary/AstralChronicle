// Created by EternityBoundary on Jul 21,2026
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <appmodel.h>
#include <shellapi.h>

#include <string>

#pragma comment(lib, "Shell32.lib")

namespace
{
    [[nodiscard]] std::wstring CurrentExecutablePath()
    {
        std::wstring path(260, L'\0');
        for (;;)
        {
            auto const length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (length == 0)
            {
                return {};
            }

            if (length < path.size() - 1 || path.size() >= 32768)
            {
                path.resize(length);
                return path;
            }

            path.resize(path.size() * 2);
        }
    }

    [[nodiscard]] std::wstring CurrentApplicationUserModelShellPath()
    {
        wchar_t buffer[APPLICATION_USER_MODEL_ID_MAX_LENGTH]{};
        UINT32 length = ARRAYSIZE(buffer);
        if (GetCurrentApplicationUserModelId(&length, buffer) != ERROR_SUCCESS || length <= 1)
        {
            return {};
        }

        std::wstring shellPath{ L"shell:AppsFolder\\" };
        shellPath.append(buffer, length - 1);
        return shellPath;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    auto target = CurrentApplicationUserModelShellPath();
    if (target.empty())
    {
        auto const executablePath = CurrentExecutablePath();
        auto const separator = executablePath.find_last_of(L"\\/");
        if (separator == std::wstring::npos)
        {
            return ERROR_FILE_NOT_FOUND;
        }

        target = executablePath.substr(0, separator + 1);
        target += L"AstralChronicle.exe";
    }

    SHELLEXECUTEINFOW executeInfo{};
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_DEFAULT;
    executeInfo.lpVerb = L"runas";
    executeInfo.lpFile = target.c_str();
    executeInfo.nShow = SW_SHOWNORMAL;

    // Keep this shim alive until ShellExecuteEx has completed the elevation
    // hand-off. The UI process must not exit while that hand-off is pending.
    return ShellExecuteExW(&executeInfo) ? ERROR_SUCCESS : GetLastError();
}
