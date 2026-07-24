// Created by EternityBoundary on Jul 20,2026
#pragma once

#include "SavedViewsViewModel.g.h"
#include "SavedViewItemViewModel.h"
#include "Services/ISavedViewRepository.h"

#include <winrt/Microsoft.UI.Dispatching.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace AstralChronicle::design
{
    struct IStringResourceService;
}

namespace winrt::AstralChronicle::implementation
{
    struct SavedViewsViewModel : SavedViewsViewModelT<SavedViewsViewModel>
    {
        SavedViewsViewModel();

        void Initialize(
            std::shared_ptr<::AstralChronicle::services::ISavedViewRepository> repository,
            std::shared_ptr<::AstralChronicle::design::IStringResourceService> strings,
            Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        [[nodiscard]] winrt::hstring Heading() const;
        [[nodiscard]] winrt::hstring Summary() const;
        [[nodiscard]] winrt::hstring StatusText() const;
        [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] Microsoft::UI::Xaml::Controls::InfoBarSeverity StatusSeverity() const noexcept;
        [[nodiscard]] bool HasStatusMessage() const noexcept;
        [[nodiscard]] bool IsLoading() const noexcept;
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::SavedViewItemViewModel> Views() const;
        [[nodiscard]] winrt::AstralChronicle::SavedViewItemViewModel SelectedView() const;
        void SelectedView(winrt::AstralChronicle::SavedViewItemViewModel const& value);
        [[nodiscard]] bool HasSelection() const noexcept;
        [[nodiscard]] bool CanSave() const noexcept;
        [[nodiscard]] winrt::hstring SelectedViewName() const;
        [[nodiscard]] winrt::hstring EditorName() const;
        void EditorName(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring EditorDescription() const;
        void EditorDescription(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring EditorQuery() const;
        void EditorQuery(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring EditorChannel() const;
        void EditorChannel(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring EditorType() const;
        void EditorType(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring EditorSort() const;
        void EditorSort(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring EditorColumns() const;
        void EditorColumns(winrt::hstring const& value);
        [[nodiscard]] bool EditorDetails() const noexcept;
        void EditorDetails(bool value);
        [[nodiscard]] bool EditorTimeline() const noexcept;
        void EditorTimeline(bool value);
        [[nodiscard]] winrt::hstring EditorTags() const;
        void EditorTags(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring EditorCustomViewXml() const;
        void EditorCustomViewXml(winrt::hstring const& value);
        [[nodiscard]] winrt::hstring CustomViewWarning() const;
        void Refresh();
        void NewView();
        void Save();
        void Delete();
        void Duplicate();
        void TogglePin();
        [[nodiscard]] winrt::hstring CopyQuery() const;
        [[nodiscard]] winrt::hstring ExportText() const;
        void ImportText(winrt::hstring const& text);

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::fire_and_forget LoadAsync(std::uint64_t requestVersion);
        void ApplyLoaded(std::vector<::AstralChronicle::models::SavedView> views);
        void FillEditor(::AstralChronicle::models::SavedView const& view);
        [[nodiscard]] ::AstralChronicle::models::SavedView BuildEditorView() const;
        [[nodiscard]] ::AstralChronicle::models::SavedViewType ParseType() const;
        void SetStatus(winrt::hstring title, winrt::hstring details, Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);
        void RaiseEditorProperties();
        void RaisePropertyChanged(winrt::hstring const& propertyName);
        void RaiseStatusProperties();

        std::shared_ptr<::AstralChronicle::services::ISavedViewRepository> m_repository;
        std::shared_ptr<::AstralChronicle::design::IStringResourceService> m_strings;
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        std::uint64_t m_requestVersion{};
        winrt::hstring m_heading;
        winrt::hstring m_summary;
        winrt::hstring m_statusText;
        winrt::hstring m_statusDetails;
        winrt::hstring m_selectedViewName;
        winrt::hstring m_editorName;
        winrt::hstring m_editorDescription;
        winrt::hstring m_editorQuery{ L"*" };
        winrt::hstring m_editorChannel{ L"System" };
        winrt::hstring m_editorType{ L"User" };
        winrt::hstring m_editorSort{ L"TimeCreated" };
        winrt::hstring m_editorColumns{ L"Level,TimeCreated,Provider,EventId,Task,Channel,User,Computer,Description" };
        winrt::hstring m_editorTags;
        winrt::hstring m_editorCustomViewXml;
        winrt::hstring m_customViewWarning;
        std::vector<::AstralChronicle::models::SavedView> m_models;
        std::wstring m_pendingSelectedId;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::AstralChronicle::SavedViewItemViewModel> m_views{ nullptr };
        winrt::AstralChronicle::SavedViewItemViewModel m_selectedView{ nullptr };
        bool m_editorDetails{};
        bool m_editorTimeline{};
        bool m_hasSelection{};
        bool m_isLoading{};
        bool m_hasStatusMessage{ true };
        Microsoft::UI::Xaml::Controls::InfoBarSeverity m_statusSeverity{
            Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::AstralChronicle::factory_implementation
{
    struct SavedViewsViewModel : SavedViewsViewModelT<SavedViewsViewModel, implementation::SavedViewsViewModel>
    {
    };
}
