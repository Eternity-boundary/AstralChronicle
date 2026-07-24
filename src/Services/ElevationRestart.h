// Created by EternityBoundary on Jul 24,2026
#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

#pragma comment(lib, "Shell32.lib")

namespace AstralChronicle::services
{
    [[nodiscard]] inline bool RestartElevatedApplication()
    {
        std::vector<wchar_t> executablePath(260);
        for (;;)
        {
            auto const length = GetModuleFileNameW(
                nullptr,
                executablePath.data(),
                static_cast<DWORD>(executablePath.size()));
            if (length == 0)
            {
                return false;
            }

            if (length < executablePath.size() - 1 || executablePath.size() >= 32768)
            {
                executablePath.resize(length);
                break;
            }

            executablePath.resize(executablePath.size() * 2);
        }

        auto const currentExecutablePath = std::wstring{
            executablePath.data(), executablePath.size() };
        auto const separator = currentExecutablePath.find_last_of(L"\\/");
        if (separator == std::wstring::npos)
        {
            return false;
        }

        auto elevationShimPath = currentExecutablePath.substr(0, separator + 1);
        elevationShimPath += L"AstralChronicleElevationShim.exe";

        SHELLEXECUTEINFOW executeInfo{};
        executeInfo.cbSize = sizeof(executeInfo);
        executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        executeInfo.lpVerb = L"open";
        executeInfo.lpFile = elevationShimPath.c_str();
        executeInfo.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&executeInfo))
        {
            return false;
        }

        if (executeInfo.hProcess)
        {
            CloseHandle(executeInfo.hProcess);
        }
        return true;
    }
}
