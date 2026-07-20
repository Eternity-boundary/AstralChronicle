// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "WindowsSessionRepository.h"
#include <winrt/Windows.Storage.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace
{
    namespace models = AstralChronicle::models;
    constexpr wchar_t Key[] = L"DiagnosticSessions.Items";
    std::mutex Mutex;
    std::wstring Escape(std::wstring_view value) { std::wstring out; for (auto c : value) { if (c == L'\\') out += L"\\\\"; else if (c == L'\t') out += L"\\t"; else if (c == L'\n') out += L"\\n"; else out += c; } return out; }
    std::wstring Unescape(std::wstring_view value) { std::wstring out; bool slash{}; for (auto c : value) { if (!slash && c == L'\\') { slash = true; continue; } if (slash) { out += c == L't' ? L'\t' : c == L'n' ? L'\n' : c; slash = false; } else out += c; } if (slash) out += L'\\'; return out; }
    std::vector<std::wstring> Split(std::wstring_view line) { std::vector<std::wstring> out; std::size_t start{}; bool slash{}; for (std::size_t i{}; i <= line.size(); ++i) { if (i < line.size() && !slash && line[i] == L'\\') { slash = true; continue; } if (i < line.size() && slash) { slash = false; continue; } if (i == line.size() || line[i] == L'\t') { out.emplace_back(Unescape(line.substr(start, i - start))); start = i + 1; } } return out; }
    bool Bool(std::wstring_view v) { return v == L"1"; }
    std::wstring Text(bool v) { return v ? L"1" : L"0"; }
    std::vector<models::DiagnosticSession> LoadUnlocked()
    {
        try
        {
            auto values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
            auto key = winrt::hstring{ Key }; if (!values.HasKey(key)) return {};
            auto text = winrt::unbox_value<winrt::hstring>(values.Lookup(key)); std::wstring input{ text.c_str() }; std::vector<models::DiagnosticSession> result; std::size_t start{};
            for (std::size_t i{}; i <= input.size(); ++i) { if (i != input.size() && input[i] != L'\n') continue; auto f = Split(std::wstring_view{ input }.substr(start, i - start)); start = i + 1; if (f.size() < 13 || f[0].empty()) continue; models::DiagnosticSession s; s.Id=std::move(f[0]); s.Name=std::move(f[1]); s.Description=std::move(f[2]); s.Channels=std::move(f[3]); s.Filter=std::move(f[4]); s.Tags=std::move(f[5]); s.Notes=std::move(f[6]); s.CorrelationGroup=std::move(f[7]); s.RemoteTarget=std::move(f[8]); s.CreatedAt=std::move(f[9]); s.LastResumedAt=std::move(f[10]); s.IsPinned=Bool(f[11]); s.IsActive=Bool(f[12]); if (f.size() > 13) s.StartAt=std::move(f[13]); if (f.size() > 14) s.EndAt=std::move(f[14]); if (f.size() > 15) s.BookmarkedEventIds=std::move(f[15]); if (f.size() > 16) s.ExportSettings=std::move(f[16]); if (f.size() > 17) s.IsArchived=Bool(f[17]); result.emplace_back(std::move(s)); }
            return result;
        }
        catch (...) { return {}; }
    }
    bool Persist(std::vector<models::DiagnosticSession> const& sessions)
    {
        try { std::wstring text; for (auto const& s : sessions) { std::vector<std::wstring> f{s.Id,s.Name,s.Description,s.Channels,s.Filter,s.Tags,s.Notes,s.CorrelationGroup,s.RemoteTarget,s.CreatedAt,s.LastResumedAt,Text(s.IsPinned),Text(s.IsActive),s.StartAt,s.EndAt,s.BookmarkedEventIds,s.ExportSettings,Text(s.IsArchived)}; for (std::size_t i{}; i<f.size(); ++i) { if (i) text += L'\t'; text += Escape(f[i]); } text += L'\n'; } auto values=winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values(); values.Insert(winrt::hstring{Key}, winrt::box_value(winrt::hstring{text})); return true; } catch (...) { return false; }
    }
}
namespace AstralChronicle::services
{
    std::vector<models::DiagnosticSession> WindowsSessionRepository::Load() const { std::scoped_lock lock{Mutex}; return LoadUnlocked(); }
    bool WindowsSessionRepository::Upsert(models::DiagnosticSession const& session) { if (session.Id.empty()) return false; std::scoped_lock lock{Mutex}; auto all=LoadUnlocked(); auto it=std::find_if(all.begin(),all.end(),[&](auto const& x){return x.Id==session.Id;}); if(it==all.end()) all.push_back(session); else *it=session; return Persist(all); }
    bool WindowsSessionRepository::Remove(std::wstring_view id) { std::scoped_lock lock{Mutex}; auto all=LoadUnlocked(); auto old=all.size(); all.erase(std::remove_if(all.begin(),all.end(),[&](auto const& x){return x.Id==id;}),all.end()); return old != all.size() && Persist(all); }
}
