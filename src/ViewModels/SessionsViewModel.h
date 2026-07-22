// Created by EternityBoundary on Jul 20,2026
#pragma once
#include "SessionsViewModel.g.h"
#include "Models/DiagnosticSession.h"
#include "Services/ISessionRepository.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <cstdint>
namespace AstralChronicle::design { struct IStringResourceService; }
namespace winrt::AstralChronicle::implementation
{
    struct SessionsViewModel : SessionsViewModelT<SessionsViewModel>
    {
        SessionsViewModel();
        void Initialize(::AstralChronicle::services::ISessionRepository& repo, ::AstralChronicle::design::IStringResourceService const& strings, Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);
        [[nodiscard]] winrt::hstring Heading() const; [[nodiscard]] winrt::hstring Summary() const; [[nodiscard]] winrt::hstring StatusText() const; [[nodiscard]] winrt::hstring StatusDetails() const;
        [[nodiscard]] winrt::hstring SelectedName() const; void SelectedName(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorDescription() const; void EditorDescription(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorChannels() const; void EditorChannels(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorFilter() const; void EditorFilter(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorTags() const; void EditorTags(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorNotes() const; void EditorNotes(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorCorrelationGroup() const; void EditorCorrelationGroup(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorRemoteTarget() const; void EditorRemoteTarget(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorStartAt() const; void EditorStartAt(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorEndAt() const; void EditorEndAt(winrt::hstring const&); [[nodiscard]] winrt::hstring EditorBookmarkedEventIds() const; void EditorBookmarkedEventIds(winrt::hstring const&);
        [[nodiscard]] winrt::hstring ActiveSummary() const; [[nodiscard]] bool EditorPinned() const noexcept; void EditorPinned(bool); [[nodiscard]] bool EditorArchived() const noexcept; [[nodiscard]] bool HasSelection() const noexcept; [[nodiscard]] bool CanSave() const noexcept; [[nodiscard]] std::int32_t SelectedIndex() const noexcept; void SelectedIndex(std::int32_t);
        [[nodiscard]] Windows::Foundation::Collections::IObservableVector<winrt::hstring> Sessions() const; [[nodiscard]] Microsoft::UI::Xaml::Controls::InfoBarSeverity StatusSeverity() const noexcept; [[nodiscard]] bool HasStatusMessage() const noexcept; [[nodiscard]] bool IsLoading() const noexcept;
        void Refresh(); void NewSession(); void Save(); void Delete(); void Duplicate(); void Archive(); void TogglePin(); void Resume(); [[nodiscard]] winrt::hstring ExportText() const; void ImportText(winrt::hstring const&);
        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&); void PropertyChanged(winrt::event_token const&) noexcept;
    private:
        winrt::fire_and_forget LoadAsync(std::uint64_t); void Apply(std::vector<::AstralChronicle::models::DiagnosticSession>); void Fill(::AstralChronicle::models::DiagnosticSession const&); [[nodiscard]] ::AstralChronicle::models::DiagnosticSession Build() const; void SetStatus(winrt::hstring, winrt::hstring, Microsoft::UI::Xaml::Controls::InfoBarSeverity); void Raise(winrt::hstring const&); void RaiseEditor();
        ::AstralChronicle::services::ISessionRepository* m_repo{}; ::AstralChronicle::design::IStringResourceService const* m_strings{}; Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{nullptr}; std::uint64_t m_version{}; std::vector<::AstralChronicle::models::DiagnosticSession> m_models; std::wstring m_pendingSelectedId; winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_sessions{nullptr}; std::int32_t m_selectedIndex{-1};
        winrt::hstring m_heading,m_summary,m_statusText,m_statusDetails,m_selectedName,m_editorDescription,m_editorChannels{L"System"},m_editorFilter{L"*"},m_editorTags,m_editorNotes,m_editorCorrelationGroup,m_editorRemoteTarget,m_editorStartAt,m_editorEndAt,m_editorBookmarkedEventIds,m_activeSummary; bool m_editorPinned{},m_editorArchived{},m_hasSelection{},m_loading{},m_hasStatus{true}; Microsoft::UI::Xaml::Controls::InfoBarSeverity m_severity{Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational}; winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_changed;
    };
}
namespace winrt::AstralChronicle::factory_implementation { struct SessionsViewModel : SessionsViewModelT<SessionsViewModel, implementation::SessionsViewModel> { }; }
