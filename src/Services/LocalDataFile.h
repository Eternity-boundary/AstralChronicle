// Created by OpenAI Codex on Jul 23, 2026
#pragma once

#include <winrt/Windows.Storage.h>

#include <Windows.h>

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace AstralChronicle::services::details
{
    inline std::atomic_uint64_t LocalDataTemporaryFileCounter{};

    class LocalDataTransactionLock final
    {
    public:
        explicit LocalDataTransactionLock(std::wstring_view const storageKey) noexcept
        {
            std::uint64_t hash = 14695981039346656037ULL;
            for (auto const character : storageKey)
            {
                hash ^= static_cast<std::uint16_t>(character);
                hash *= 1099511628211ULL;
            }

            std::array<wchar_t, 96> name{};
            if (_snwprintf_s(
                    name.data(),
                    name.size(),
                    _TRUNCATE,
                    L"Local\\AstralChronicle.Data.%016llX",
                    static_cast<unsigned long long>(hash)) < 0)
            {
                return;
            }

            m_handle = ::CreateMutexW(nullptr, FALSE, name.data());
            if (!m_handle)
            {
                return;
            }

            auto const waitResult = ::WaitForSingleObject(m_handle, INFINITE);
            if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED)
            {
                m_acquired = true;
                return;
            }

            ::CloseHandle(m_handle);
            m_handle = nullptr;
        }

        ~LocalDataTransactionLock()
        {
            if (m_acquired)
            {
                (void)::ReleaseMutex(m_handle);
            }
            if (m_handle)
            {
                ::CloseHandle(m_handle);
            }
        }

        LocalDataTransactionLock(LocalDataTransactionLock const&) = delete;
        LocalDataTransactionLock& operator=(LocalDataTransactionLock const&) = delete;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return m_acquired;
        }

    private:
        HANDLE m_handle{};
        bool m_acquired{};
    };

    enum class LocalTextReadStatus
    {
        Missing,
        Succeeded,
        Failed,
    };

    struct LocalTextReadResult final
    {
        LocalTextReadStatus Status{ LocalTextReadStatus::Failed };
        std::wstring Text;
    };

    [[nodiscard]] inline std::filesystem::path LocalDataPath(
        std::wstring_view const fileName)
    {
        auto const folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
        return std::filesystem::path{ folder.Path().c_str() } / std::wstring{ fileName };
    }

    [[nodiscard]] inline LocalTextReadResult ReadLocalUtf8Text(
        std::wstring_view const fileName,
        std::size_t const maximumBytes) noexcept
    {
        try
        {
            auto const path = LocalDataPath(fileName);
            std::error_code error;
            auto const exists = std::filesystem::exists(path, error);
            if (error)
            {
                return {};
            }
            if (!exists)
            {
                return { LocalTextReadStatus::Missing, {} };
            }
            if (!std::filesystem::is_regular_file(path, error) || error)
            {
                return {};
            }

            auto const byteCount = std::filesystem::file_size(path, error);
            if (error || byteCount > maximumBytes)
            {
                return {};
            }

            std::ifstream input{ path, std::ios::binary };
            if (!input)
            {
                return {};
            }

            std::string bytes(static_cast<std::size_t>(byteCount), '\0');
            if (!bytes.empty())
            {
                input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                if (!input)
                {
                    return {};
                }
            }

            if (bytes.size() >= 3 &&
                static_cast<unsigned char>(bytes[0]) == 0xEF &&
                static_cast<unsigned char>(bytes[1]) == 0xBB &&
                static_cast<unsigned char>(bytes[2]) == 0xBF)
            {
                bytes.erase(0, 3);
            }
            if (bytes.size() >= 2 &&
                ((static_cast<unsigned char>(bytes[0]) == 0xFF &&
                    static_cast<unsigned char>(bytes[1]) == 0xFE) ||
                    (static_cast<unsigned char>(bytes[0]) == 0xFE &&
                        static_cast<unsigned char>(bytes[1]) == 0xFF)))
            {
                return {};
            }

            auto const text = winrt::to_hstring(bytes);
            if (std::wstring_view{ text.c_str(), text.size() }.find(L'\0') !=
                std::wstring_view::npos)
            {
                return {};
            }
            return {
                LocalTextReadStatus::Succeeded,
                std::wstring{ text.c_str(), text.size() },
            };
        }
        catch (...)
        {
            return {};
        }
    }

    [[nodiscard]] inline bool WriteLocalUtf8TextAtomically(
        std::wstring_view const fileName,
        std::wstring_view const text,
        std::size_t const maximumBytes) noexcept
    {
        std::filesystem::path temporaryPath;
        try
        {
            auto const targetPath = LocalDataPath(fileName);
            temporaryPath = targetPath;
            temporaryPath +=
                L".tmp." +
                std::to_wstring(::GetCurrentProcessId()) +
                L"." +
                std::to_wstring(::GetCurrentThreadId()) +
                L"." +
                std::to_wstring(
                    LocalDataTemporaryFileCounter.fetch_add(1, std::memory_order_relaxed));

            auto const utf8 = winrt::to_string(winrt::hstring{ text });
            if (utf8.size() > maximumBytes)
            {
                return false;
            }

            {
                std::ofstream output{
                    temporaryPath,
                    std::ios::binary | std::ios::trunc,
                };
                if (!output)
                {
                    (void)::DeleteFileW(temporaryPath.c_str());
                    return false;
                }
                output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
                output.flush();
                if (!output)
                {
                    output.close();
                    (void)::DeleteFileW(temporaryPath.c_str());
                    return false;
                }
            }

            if (!::MoveFileExW(
                    temporaryPath.c_str(),
                    targetPath.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                (void)::DeleteFileW(temporaryPath.c_str());
                return false;
            }
            return true;
        }
        catch (...)
        {
            if (!temporaryPath.empty())
            {
                (void)::DeleteFileW(temporaryPath.c_str());
            }
            return false;
        }
    }

    [[nodiscard]] inline bool DeleteLocalDataFile(
        std::wstring_view const fileName) noexcept
    {
        try
        {
            auto const path = LocalDataPath(fileName);
            if (::DeleteFileW(path.c_str()))
            {
                return true;
            }
            auto const error = ::GetLastError();
            return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        }
        catch (...)
        {
            return false;
        }
    }
}
