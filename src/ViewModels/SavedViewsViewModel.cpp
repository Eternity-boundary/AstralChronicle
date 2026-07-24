// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SavedViewsViewModel.h"

#include "DesignSystem/Localization/IStringResourceService.h"

#include "SavedViewsViewModel.g.cpp"

#include <wil/cppwinrt_helpers.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    [[nodiscard]] std::wstring NowText()
    {
        return std::to_wstring(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    [[nodiscard]] winrt::hstring FormatResource(
        winrt::hstring format,
        winrt::hstring const& value)
    {
        auto text = std::wstring{ format.c_str() };
        auto const token = std::wstring{ L"{0}" };
        auto const position = text.find(token);
        if (position != std::wstring::npos)
        {
            text.replace(position, token.size(), value.c_str());
        }
        return winrt::hstring{ text };
    }

    [[nodiscard]] winrt::hstring TypeValue(::AstralChronicle::models::SavedViewType const type)
    {
        using Type = ::AstralChronicle::models::SavedViewType;
        switch (type)
        {
        case Type::Pinned: return L"Pinned";
        case Type::WindowsCustomView: return L"WindowsCustomView";
        case Type::Bookmark: return L"Bookmark";
        case Type::FavoriteLog: return L"FavoriteLog";
        case Type::RecentSearch: return L"RecentSearch";
        case Type::Workspace: return L"Workspace";
        case Type::User:
        default: return L"User";
        }
    }

    [[nodiscard]] ::AstralChronicle::models::SavedViewType ParseTypeValue(std::wstring_view value)
    {
        if (value == L"Pinned") return ::AstralChronicle::models::SavedViewType::Pinned;
        if (value == L"WindowsCustomView") return ::AstralChronicle::models::SavedViewType::WindowsCustomView;
        if (value == L"Bookmark") return ::AstralChronicle::models::SavedViewType::Bookmark;
        if (value == L"FavoriteLog") return ::AstralChronicle::models::SavedViewType::FavoriteLog;
        if (value == L"RecentSearch") return ::AstralChronicle::models::SavedViewType::RecentSearch;
        if (value == L"Workspace") return ::AstralChronicle::models::SavedViewType::Workspace;
        return ::AstralChronicle::models::SavedViewType::User;
    }

    [[nodiscard]] std::wstring EscapeExportValue(std::wstring_view value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (auto const character : value)
        {
            switch (character)
            {
            case L'\\': result += L"\\\\"; break;
            case L'\t': result += L"\\t"; break;
            case L'\r': result += L"\\r"; break;
            case L'\n': result += L"\\n"; break;
            default: result += character; break;
            }
        }
        return result;
    }

    [[nodiscard]] std::wstring UnescapeExportValue(std::wstring_view value)
    {
        std::wstring result;
        bool escaped{};
        for (auto const character : value)
        {
            if (!escaped && character == L'\\')
            {
                escaped = true;
                continue;
            }
            if (escaped)
            {
                switch (character)
                {
                case L't': result += L'\t'; break;
                case L'r': result += L'\r'; break;
                case L'n': result += L'\n'; break;
                default: result += character; break;
                }
                escaped = false;
                continue;
            }
            result += character;
        }
        if (escaped) result += L'\\';
        return result;
    }

    [[nodiscard]] bool ParseBool(std::wstring_view value)
    {
        return value == L"1" || value == L"true";
    }

    [[nodiscard]] std::wstring NewId()
    {
        static std::atomic_uint64_t sequence{};
        auto const timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return L"view-" + std::to_wstring(timestamp) + L"-" + std::to_wstring(++sequence);
    }
}

namespace winrt::AstralChronicle::implementation
{
    SavedViewsViewModel::SavedViewsViewModel()
        : m_views(winrt::single_threaded_observable_vector<winrt::AstralChronicle::SavedViewItemViewModel>())
    {
    }

    void SavedViewsViewModel::Initialize(
        std::shared_ptr<::AstralChronicle::services::ISavedViewRepository> repository,
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
        Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        m_repository = std::move(repository);
        m_strings = std::move(strings);
        if (!m_repository || !m_strings)
        {
            throw std::invalid_argument("Saved views require repository and string services.");
        }
        m_dispatcher = dispatcher;
        m_heading = m_strings->GetString(L"SavedViews.Heading");
        m_summary = m_strings->GetString(L"SavedViews.Summary");
        m_customViewWarning = m_strings->GetString(L"SavedViews.CustomViewWarning.Text");
        NewView();
        Refresh();
    }

    void SavedViewsViewModel::Refresh()
    {
        if (!m_repository || !m_strings || !m_dispatcher)
        {
            return;
        }
        auto const requestVersion = ++m_requestVersion;
        m_isLoading = true;
        SetStatus(
            m_strings->GetString(L"SavedViews.Loading.Text"),
            m_strings->GetString(L"SavedViews.LoadingDetails.Text"),
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
        LoadAsync(requestVersion);
    }

    winrt::fire_and_forget SavedViewsViewModel::LoadAsync(std::uint64_t const requestVersion)
    {
        auto lifetime = get_strong();
        try
        {
            auto const repository = m_repository;
            auto const dispatcher = m_dispatcher;
            if (!repository || !dispatcher)
            {
                co_return;
            }

            bool failed{};
            std::vector<::AstralChronicle::models::SavedView> views;
            co_await winrt::resume_background();
            try
            {
                views = repository->Load();
            }
            catch (...)
            {
                failed = true;
            }

            co_await wil::resume_foreground(dispatcher);
            if (requestVersion != m_requestVersion)
            {
                co_return;
            }
            if (failed)
            {
                m_isLoading = false;
                SetStatus(
                    m_strings->GetString(L"SavedViews.SaveFailed.Text"),
                    {},
                    Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
                co_return;
            }
            ApplyLoaded(std::move(views));
        }
        catch (...)
        {
            // Dispatcher shutdown is a normal cancellation path.
        }
    }

    void SavedViewsViewModel::ApplyLoaded(std::vector<::AstralChronicle::models::SavedView> views)
    {
        auto selectedId = m_pendingSelectedId;
        if (selectedId.empty() && m_selectedView)
        {
            selectedId = std::wstring{ m_selectedView.Id().c_str() };
        }
        std::sort(views.begin(), views.end(), [](auto const& left, auto const& right)
        {
            if (left.IsPinned != right.IsPinned) return left.IsPinned > right.IsPinned;
            return left.UpdatedAt > right.UpdatedAt;
        });
        m_models = std::move(views);
        auto values = winrt::single_threaded_observable_vector<winrt::AstralChronicle::SavedViewItemViewModel>();
        winrt::AstralChronicle::SavedViewItemViewModel selected{ nullptr };
        for (auto const& view : m_models)
        {
            auto item = winrt::make<winrt::AstralChronicle::implementation::SavedViewItemViewModel>();
            winrt::get_self<winrt::AstralChronicle::implementation::SavedViewItemViewModel>(item)->Initialize(view, *m_strings);
            values.Append(item);
            if (!selectedId.empty() && view.Id == selectedId) selected = item;
        }
        m_views = values;
        m_selectedView = selected;
        m_hasSelection = selected != nullptr;
        if (selected)
        {
            auto const model = std::find_if(m_models.begin(), m_models.end(), [&selectedId](auto const& view)
            {
                return view.Id == selectedId;
            });
            if (model != m_models.end()) FillEditor(*model);
        }
        else if (!selectedId.empty())
        {
            // The selected item was deleted or disappeared during refresh.
            // Reset the editor instead of turning stale data into a new view.
            NewView();
        }
        m_pendingSelectedId.clear();
        m_isLoading = false;
        m_summary = FormatResource(m_strings->GetString(L"SavedViews.SummaryCount.Text"), winrt::to_hstring(m_models.size()));
        SetStatus(
            m_strings->GetString(L"SavedViews.Ready.Text"),
            {},
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
        m_hasStatusMessage = false;
        RaisePropertyChanged(L"Views");
        RaisePropertyChanged(L"Summary");
        RaisePropertyChanged(L"SelectedView");
        RaisePropertyChanged(L"HasSelection");
        RaisePropertyChanged(L"SelectedViewName");
        RaiseEditorProperties();
        RaiseStatusProperties();
    }

    void SavedViewsViewModel::SelectedView(
        winrt::AstralChronicle::SavedViewItemViewModel const& value)
    {
        if (m_selectedView == value)
        {
            return;
        }
        m_selectedView = value;
        m_hasSelection = value != nullptr;
        if (value)
        {
            auto const id = std::wstring{ value.Id().c_str() };
            auto const model = std::find_if(m_models.begin(), m_models.end(), [&id](auto const& view)
            {
                return view.Id == id;
            });
            if (model != m_models.end())
            {
                FillEditor(*model);
            }
            else
            {
                NewView();
                return;
            }
        }
        else
        {
            m_selectedViewName.clear();
        }
        RaisePropertyChanged(L"SelectedView");
        RaisePropertyChanged(L"HasSelection");
        RaisePropertyChanged(L"SelectedViewName");
        RaiseEditorProperties();
    }

    void SavedViewsViewModel::FillEditor(::AstralChronicle::models::SavedView const& view)
    {
        m_selectedViewName = view.Name;
        m_editorName = view.Name;
        m_editorDescription = view.Description;
        m_editorQuery = view.Query;
        m_editorChannel = view.Channel;
        m_editorType = TypeValue(view.Type);
        m_editorSort = view.Sort;
        m_editorColumns = view.Columns;
        m_editorDetails = view.Details;
        m_editorTimeline = view.Timeline;
        m_editorTags = view.Tags;
        m_editorCustomViewXml = view.CustomViewXml;
    }

    void SavedViewsViewModel::NewView()
    {
        m_pendingSelectedId.clear();
        m_selectedView = nullptr;
        m_hasSelection = false;
        m_selectedViewName.clear();
        m_editorName.clear();
        m_editorDescription.clear();
        m_editorQuery = L"*";
        m_editorChannel = L"System";
        m_editorType = L"User";
        m_editorSort = L"TimeCreated";
        m_editorColumns = L"Level,TimeCreated,Provider,EventId,Task,Channel,User,Computer,Description";
        m_editorDetails = false;
        m_editorTimeline = false;
        m_editorTags.clear();
        m_editorCustomViewXml.clear();
        RaisePropertyChanged(L"SelectedView");
        RaisePropertyChanged(L"HasSelection");
        RaisePropertyChanged(L"SelectedViewName");
        RaiseEditorProperties();
    }

    ::AstralChronicle::models::SavedViewType SavedViewsViewModel::ParseType() const
    {
        if (m_editorType == L"Pinned") return ::AstralChronicle::models::SavedViewType::Pinned;
        if (m_editorType == L"WindowsCustomView") return ::AstralChronicle::models::SavedViewType::WindowsCustomView;
        if (m_editorType == L"Bookmark") return ::AstralChronicle::models::SavedViewType::Bookmark;
        if (m_editorType == L"FavoriteLog") return ::AstralChronicle::models::SavedViewType::FavoriteLog;
        if (m_editorType == L"RecentSearch") return ::AstralChronicle::models::SavedViewType::RecentSearch;
        if (m_editorType == L"Workspace") return ::AstralChronicle::models::SavedViewType::Workspace;
        return ::AstralChronicle::models::SavedViewType::User;
    }

    ::AstralChronicle::models::SavedView SavedViewsViewModel::BuildEditorView() const
    {
        ::AstralChronicle::models::SavedView view;
        view.Id = m_selectedView ? std::wstring{ m_selectedView.Id().c_str() } : NewId();
        view.Name = m_editorName;
        view.Description = m_editorDescription;
        view.Query = m_editorQuery.empty() ? L"*" : std::wstring{ m_editorQuery.c_str() };
        view.Channel = m_editorChannel.empty() ? L"System" : std::wstring{ m_editorChannel.c_str() };
        view.Type = ParseType();
        view.Sort = m_editorSort;
        view.Columns = m_editorColumns;
        view.Details = m_editorDetails;
        view.Timeline = m_editorTimeline;
        view.Tags = m_editorTags;
        view.CustomViewXml = m_editorCustomViewXml;
        auto const existing = std::find_if(m_models.begin(), m_models.end(), [&view](auto const& candidate)
        {
            return candidate.Id == view.Id;
        });
        view.CreatedAt = existing == m_models.end() ? NowText() : existing->CreatedAt;
        view.UpdatedAt = NowText();
        view.IsPinned = existing != m_models.end() && existing->IsPinned;
        return view;
    }

    void SavedViewsViewModel::Save()
    {
        if (!m_repository || !m_strings || !CanSave())
        {
            if (m_strings)
            {
                SetStatus(m_strings->GetString(L"SavedViews.NameRequired.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
            }
            return;
        }
        auto view = BuildEditorView();
        if (!m_repository->Upsert(view))
        {
            SetStatus(m_strings->GetString(L"SavedViews.SaveFailed.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
            return;
        }
        m_pendingSelectedId = view.Id;
        m_selectedViewName = view.Name;
        SetStatus(m_strings->GetString(L"SavedViews.Saved.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
        Refresh();
    }

    void SavedViewsViewModel::Delete()
    {
        if (!m_repository || !m_selectedView) return;
        auto const id = std::wstring{ m_selectedView.Id().c_str() };
        if (m_repository->Remove(id))
        {
            NewView();
            SetStatus(m_strings->GetString(L"SavedViews.Deleted.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
            Refresh();
        }
    }

    void SavedViewsViewModel::Duplicate()
    {
        if (!m_repository || !m_selectedView) return;
        auto const id = std::wstring{ m_selectedView.Id().c_str() };
        auto const existing = std::find_if(m_models.begin(), m_models.end(), [&id](auto const& view)
        {
            return view.Id == id;
        });
        if (existing == m_models.end()) return;
        auto copy = *existing;
        copy.Id = NewId() + L"-copy";
        copy.Name += L" (Copy)";
        copy.Type = ::AstralChronicle::models::SavedViewType::User;
        copy.IsPinned = false;
        copy.CreatedAt = NowText();
        copy.UpdatedAt = copy.CreatedAt;
        if (m_repository->Upsert(copy))
        {
            m_pendingSelectedId = copy.Id;
            SetStatus(m_strings->GetString(L"SavedViews.Duplicated.Text"), {}, Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
            Refresh();
        }
    }

    void SavedViewsViewModel::TogglePin()
    {
        if (!m_repository || !m_selectedView) return;
        auto const id = std::wstring{ m_selectedView.Id().c_str() };
        auto existing = std::find_if(m_models.begin(), m_models.end(), [&id](auto const& view)
        {
            return view.Id == id;
        });
        if (existing == m_models.end()) return;
        auto updated = *existing;
        updated.IsPinned = !updated.IsPinned;
        updated.UpdatedAt = NowText();
        if (m_repository->Upsert(updated))
        {
            Refresh();
        }
        else
        {
            SetStatus(
                m_strings->GetString(L"SavedViews.SaveFailed.Text"),
                {},
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
        }
    }

    winrt::hstring SavedViewsViewModel::CopyQuery() const { return m_editorQuery; }

    winrt::hstring SavedViewsViewModel::ExportText() const
    {
        std::wstring text;
        for (auto const& view : m_models)
        {
            text += L"[" + EscapeExportValue(view.Name) + L"]\r\n";
            text += L"Type=" + EscapeExportValue(TypeValue(view.Type).c_str()) + L"\r\n";
            text += L"Channel=" + EscapeExportValue(view.Channel) + L"\r\n";
            text += L"Query=" + EscapeExportValue(view.Query) + L"\r\n";
            text += L"Description=" + EscapeExportValue(view.Description) + L"\r\n";
            text += L"Sort=" + EscapeExportValue(view.Sort) + L"\r\n";
            text += L"Columns=" + EscapeExportValue(view.Columns) + L"\r\n";
            text += L"Details=" + std::wstring{ view.Details ? L"1" : L"0" } + L"\r\n";
            text += L"Timeline=" + std::wstring{ view.Timeline ? L"1" : L"0" } + L"\r\n";
            text += L"Tags=" + EscapeExportValue(view.Tags) + L"\r\n";
            text += L"Pinned=" + std::wstring{ view.IsPinned ? L"1" : L"0" } + L"\r\n";
            text += L"CustomViewXml=" + EscapeExportValue(view.CustomViewXml) + L"\r\n\r\n";
        }
        return winrt::hstring{ text };
    }

    void SavedViewsViewModel::ImportText(winrt::hstring const& text)
    {
        if (!m_repository) return;
        std::wstring input{ text.c_str() };
        std::size_t start{};
        ::AstralChronicle::models::SavedView current;
        bool hasCurrent{};
        std::size_t importedCount{};
        std::size_t failedCount{};
        auto commit = [&]
        {
            if (!hasCurrent || current.Name.empty()) return;
            current.Id = NewId() + L"-import";
            current.CreatedAt = NowText();
            current.UpdatedAt = current.CreatedAt;
            if (m_repository->Upsert(current))
            {
                ++importedCount;
            }
            else
            {
                ++failedCount;
            }
            current = {};
            current.Query = L"*";
            current.Channel = L"System";
            hasCurrent = false;
        };
        while (start <= input.size())
        {
            auto const end = input.find(L'\n', start);
            auto line = input.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            if (line.size() > 2 && line.front() == L'[' && line.back() == L']')
            {
                commit();
                current.Name = UnescapeExportValue(line.substr(1, line.size() - 2));
                hasCurrent = true;
            }
            else if (line.rfind(L"Type=", 0) == 0) current.Type = ParseTypeValue(UnescapeExportValue(line.substr(5)));
            else if (line.rfind(L"Channel=", 0) == 0) current.Channel = UnescapeExportValue(line.substr(8));
            else if (line.rfind(L"Query=", 0) == 0) current.Query = UnescapeExportValue(line.substr(6));
            else if (line.rfind(L"Description=", 0) == 0) current.Description = UnescapeExportValue(line.substr(12));
            else if (line.rfind(L"Sort=", 0) == 0) current.Sort = UnescapeExportValue(line.substr(5));
            else if (line.rfind(L"Columns=", 0) == 0) current.Columns = UnescapeExportValue(line.substr(8));
            else if (line.rfind(L"Details=", 0) == 0) current.Details = ParseBool(line.substr(8));
            else if (line.rfind(L"Timeline=", 0) == 0) current.Timeline = ParseBool(line.substr(9));
            else if (line.rfind(L"Tags=", 0) == 0) current.Tags = UnescapeExportValue(line.substr(5));
            else if (line.rfind(L"Pinned=", 0) == 0) current.IsPinned = ParseBool(line.substr(7));
            else if (line.rfind(L"CustomViewXml=", 0) == 0) current.CustomViewXml = UnescapeExportValue(line.substr(14));
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
        commit();
        if (failedCount != 0 || importedCount == 0)
        {
            SetStatus(
                m_strings->GetString(L"SavedViews.SaveFailed.Text"),
                {},
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error);
        }
        else
        {
            SetStatus(
                m_strings->GetString(L"SavedViews.Imported.Text"),
                {},
                Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
            Refresh();
        }
    }

    void SavedViewsViewModel::SetStatus(
        winrt::hstring title,
        winrt::hstring details,
        Microsoft::UI::Xaml::Controls::InfoBarSeverity const severity)
    {
        m_statusText = std::move(title);
        m_statusDetails = std::move(details);
        m_statusSeverity = severity;
        m_hasStatusMessage = true;
        RaiseStatusProperties();
    }

    winrt::hstring SavedViewsViewModel::Heading() const { return m_heading; }
    winrt::hstring SavedViewsViewModel::Summary() const { return m_summary; }
    winrt::hstring SavedViewsViewModel::StatusText() const { return m_statusText; }
    winrt::hstring SavedViewsViewModel::StatusDetails() const { return m_statusDetails; }
    Microsoft::UI::Xaml::Controls::InfoBarSeverity SavedViewsViewModel::StatusSeverity() const noexcept { return m_statusSeverity; }
    bool SavedViewsViewModel::HasStatusMessage() const noexcept { return m_hasStatusMessage; }
    bool SavedViewsViewModel::IsLoading() const noexcept { return m_isLoading; }
    Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::SavedViewItemViewModel> SavedViewsViewModel::Views() const { return m_views; }
    winrt::AstralChronicle::SavedViewItemViewModel SavedViewsViewModel::SelectedView() const { return m_selectedView; }
    bool SavedViewsViewModel::HasSelection() const noexcept { return m_hasSelection; }
    bool SavedViewsViewModel::CanSave() const noexcept { return !m_editorName.empty(); }
    winrt::hstring SavedViewsViewModel::SelectedViewName() const { return m_selectedViewName; }
    winrt::hstring SavedViewsViewModel::EditorName() const { return m_editorName; }
    void SavedViewsViewModel::EditorName(winrt::hstring const& value) { m_editorName = value; RaisePropertyChanged(L"EditorName"); RaisePropertyChanged(L"CanSave"); }
    winrt::hstring SavedViewsViewModel::EditorDescription() const { return m_editorDescription; }
    void SavedViewsViewModel::EditorDescription(winrt::hstring const& value) { m_editorDescription = value; RaisePropertyChanged(L"EditorDescription"); }
    winrt::hstring SavedViewsViewModel::EditorQuery() const { return m_editorQuery; }
    void SavedViewsViewModel::EditorQuery(winrt::hstring const& value) { m_editorQuery = value; RaisePropertyChanged(L"EditorQuery"); }
    winrt::hstring SavedViewsViewModel::EditorChannel() const { return m_editorChannel; }
    void SavedViewsViewModel::EditorChannel(winrt::hstring const& value) { m_editorChannel = value; RaisePropertyChanged(L"EditorChannel"); }
    winrt::hstring SavedViewsViewModel::EditorType() const { return m_editorType; }
    void SavedViewsViewModel::EditorType(winrt::hstring const& value) { m_editorType = value; RaisePropertyChanged(L"EditorType"); }
    winrt::hstring SavedViewsViewModel::EditorSort() const { return m_editorSort; }
    void SavedViewsViewModel::EditorSort(winrt::hstring const& value) { m_editorSort = value; RaisePropertyChanged(L"EditorSort"); }
    winrt::hstring SavedViewsViewModel::EditorColumns() const { return m_editorColumns; }
    void SavedViewsViewModel::EditorColumns(winrt::hstring const& value) { m_editorColumns = value; RaisePropertyChanged(L"EditorColumns"); }
    bool SavedViewsViewModel::EditorDetails() const noexcept { return m_editorDetails; }
    void SavedViewsViewModel::EditorDetails(bool const value) { m_editorDetails = value; RaisePropertyChanged(L"EditorDetails"); }
    bool SavedViewsViewModel::EditorTimeline() const noexcept { return m_editorTimeline; }
    void SavedViewsViewModel::EditorTimeline(bool const value) { m_editorTimeline = value; RaisePropertyChanged(L"EditorTimeline"); }
    winrt::hstring SavedViewsViewModel::EditorTags() const { return m_editorTags; }
    void SavedViewsViewModel::EditorTags(winrt::hstring const& value) { m_editorTags = value; RaisePropertyChanged(L"EditorTags"); }
    winrt::hstring SavedViewsViewModel::EditorCustomViewXml() const { return m_editorCustomViewXml; }
    void SavedViewsViewModel::EditorCustomViewXml(winrt::hstring const& value) { m_editorCustomViewXml = value; RaisePropertyChanged(L"EditorCustomViewXml"); }
    winrt::hstring SavedViewsViewModel::CustomViewWarning() const { return m_customViewWarning; }

    void SavedViewsViewModel::RaiseEditorProperties()
    {
        RaisePropertyChanged(L"EditorName");
        RaisePropertyChanged(L"EditorDescription");
        RaisePropertyChanged(L"EditorQuery");
        RaisePropertyChanged(L"EditorChannel");
        RaisePropertyChanged(L"EditorType");
        RaisePropertyChanged(L"EditorSort");
        RaisePropertyChanged(L"EditorColumns");
        RaisePropertyChanged(L"EditorDetails");
        RaisePropertyChanged(L"EditorTimeline");
        RaisePropertyChanged(L"EditorTags");
        RaisePropertyChanged(L"EditorCustomViewXml");
        RaisePropertyChanged(L"CanSave");
    }

    void SavedViewsViewModel::RaiseStatusProperties()
    {
        RaisePropertyChanged(L"StatusText");
        RaisePropertyChanged(L"StatusDetails");
        RaisePropertyChanged(L"StatusSeverity");
        RaisePropertyChanged(L"HasStatusMessage");
        RaisePropertyChanged(L"IsLoading");
    }

    winrt::event_token SavedViewsViewModel::PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void SavedViewsViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void SavedViewsViewModel::RaisePropertyChanged(winrt::hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
    }
}
