// Created by EternityBoundary on Jul 20,2026
#include "pch.h"
#include "SessionsViewModel.h"
#include "DesignSystem/Localization/IStringResourceService.h"
#include "SessionsViewModel.g.cpp"
#include <wil/cppwinrt_helpers.h>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <string>
#include <string_view>
namespace
{
    std::wstring Now(){return std::to_wstring(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));}
    std::wstring NewId()
    {
        static std::atomic_uint64_t sequence{};
        auto const timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return L"session-" + std::to_wstring(timestamp) + L"-" + std::to_wstring(++sequence);
    }
    std::wstring EscapeExportValue(std::wstring_view value)
    {
        std::wstring result;
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
    std::wstring UnescapeExportValue(std::wstring_view value)
    {
        std::wstring result;
        bool escaped{};
        for (auto const character : value)
        {
            if (!escaped && character == L'\\') { escaped = true; continue; }
            if (escaped)
            {
                result += character == L't' ? L'\t' : character == L'r' ? L'\r' : character == L'n' ? L'\n' : character;
                escaped = false;
            }
            else result += character;
        }
        if (escaped) result += L'\\';
        return result;
    }
    bool ParseBool(std::wstring_view value){return value == L"1" || value == L"true";}
}
namespace winrt::AstralChronicle::implementation
{
    SessionsViewModel::SessionsViewModel():m_sessions(winrt::single_threaded_observable_vector<winrt::hstring>()){}
    void SessionsViewModel::Initialize(::AstralChronicle::services::ISessionRepository& repo, ::AstralChronicle::design::IStringResourceService const& strings, Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher){m_repo=&repo;m_strings=&strings;m_dispatcher=dispatcher;m_heading=strings.GetString(L"Sessions.Heading");m_summary=strings.GetString(L"Sessions.Summary");NewSession();Refresh();}
    void SessionsViewModel::Refresh(){if(!m_repo||!m_dispatcher)return;auto v=++m_version;m_loading=true;SetStatus(m_strings->GetString(L"Sessions.Loading.Text"),m_strings->GetString(L"Sessions.LoadingDetails.Text"),Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);LoadAsync(v);}
    winrt::fire_and_forget SessionsViewModel::LoadAsync(std::uint64_t v){auto life=get_strong();co_await winrt::resume_background();auto all=m_repo->Load();co_await wil::resume_foreground(m_dispatcher);if(v!=m_version)co_return;Apply(std::move(all));}
    void SessionsViewModel::Apply(std::vector<::AstralChronicle::models::DiagnosticSession> all){auto selectedId=m_pendingSelectedId;if(selectedId.empty()&&m_hasSelection&&m_selectedIndex>=0&&static_cast<std::size_t>(m_selectedIndex)<m_models.size())selectedId=m_models[static_cast<std::size_t>(m_selectedIndex)].Id;std::sort(all.begin(),all.end(),[](auto const&a,auto const&b){return a.IsPinned!=b.IsPinned?a.IsPinned>b.IsPinned:a.Name<b.Name;});m_models=std::move(all);m_selectedIndex=-1;m_hasSelection=false;auto vals=winrt::single_threaded_observable_vector<winrt::hstring>();for(auto const&s:m_models){vals.Append(winrt::hstring{s.Name});if(!selectedId.empty()&&s.Id==selectedId){m_selectedIndex=static_cast<std::int32_t>(vals.Size()-1);m_hasSelection=true;Fill(s);}}m_pendingSelectedId.clear();m_sessions=vals;m_loading=false;m_summary=m_strings->GetString(L"Sessions.SummaryCount.Text");m_hasStatus=false;Raise(L"Sessions");Raise(L"Summary");Raise(L"SelectedIndex");Raise(L"HasSelection");RaiseEditor();Raise(L"IsLoading");Raise(L"HasStatusMessage");}
    void SessionsViewModel::SelectedIndex(std::int32_t index){m_selectedIndex=index;m_hasSelection=index>=0&&static_cast<std::size_t>(index)<m_models.size();if(m_hasSelection)Fill(m_models[static_cast<std::size_t>(index)]);Raise(L"SelectedIndex");Raise(L"HasSelection");RaiseEditor();}
    void SessionsViewModel::Fill(::AstralChronicle::models::DiagnosticSession const&s){m_selectedName=s.Name;m_editorDescription=s.Description;m_editorChannels=s.Channels;m_editorFilter=s.Filter;m_editorTags=s.Tags;m_editorNotes=s.Notes;m_editorCorrelationGroup=s.CorrelationGroup;m_editorRemoteTarget=s.RemoteTarget;m_editorStartAt=s.StartAt;m_editorEndAt=s.EndAt;m_editorBookmarkedEventIds=s.BookmarkedEventIds;m_editorPinned=s.IsPinned;m_editorArchived=s.IsArchived;m_activeSummary=s.IsActive?m_strings->GetString(L"Sessions.Active.Text"):m_strings->GetString(L"Sessions.Inactive.Text");}
    void SessionsViewModel::NewSession(){m_pendingSelectedId.clear();m_selectedIndex=-1;m_hasSelection=false;m_selectedName.clear();m_editorDescription.clear();m_editorChannels=L"System";m_editorFilter=L"*";m_editorTags.clear();m_editorNotes.clear();m_editorCorrelationGroup.clear();m_editorRemoteTarget.clear();m_editorStartAt.clear();m_editorEndAt.clear();m_editorBookmarkedEventIds.clear();m_editorPinned=false;m_editorArchived=false;m_activeSummary=m_strings?m_strings->GetString(L"Sessions.New.Text"):winrt::hstring{};RaiseEditor();}
    ::AstralChronicle::models::DiagnosticSession SessionsViewModel::Build()const{::AstralChronicle::models::DiagnosticSession s;if(m_hasSelection)s=m_models[static_cast<std::size_t>(m_selectedIndex)];s.Id=s.Id.empty()?NewId():s.Id;s.Name=m_selectedName;s.Description=m_editorDescription;s.Channels=m_editorChannels;s.Filter=m_editorFilter;s.Tags=m_editorTags;s.Notes=m_editorNotes;s.CorrelationGroup=m_editorCorrelationGroup;s.RemoteTarget=m_editorRemoteTarget;s.StartAt=m_editorStartAt;s.EndAt=m_editorEndAt;s.BookmarkedEventIds=m_editorBookmarkedEventIds;s.IsPinned=m_editorPinned;s.IsArchived=m_editorArchived;s.CreatedAt=s.CreatedAt.empty()?Now():s.CreatedAt;return s;}
    void SessionsViewModel::Save(){if(!m_repo||!CanSave()){SetStatus(m_strings->GetString(L"Sessions.NameRequired.Text"),{},Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);return;}auto s=Build();if(m_repo->Upsert(s)){m_pendingSelectedId=s.Id;SetStatus(m_strings->GetString(L"Sessions.Saved.Text"),{},Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);Refresh();}}
    void SessionsViewModel::Delete(){if(!m_repo||!m_hasSelection)return;if(m_repo->Remove(m_models[static_cast<std::size_t>(m_selectedIndex)].Id)){NewSession();SetStatus(m_strings->GetString(L"Sessions.Deleted.Text"),{},Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);Refresh();}}
    void SessionsViewModel::Duplicate(){if(!m_hasSelection)return;auto s=Build();s.Id=NewId()+L"-copy";s.Name+=L" (copy)";s.IsActive=false;if(m_repo&&m_repo->Upsert(s)){m_pendingSelectedId=s.Id;SetStatus(m_strings->GetString(L"Sessions.Duplicated.Text"),{},Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);Refresh();}}
    void SessionsViewModel::Archive(){if(!m_hasSelection)return;m_editorArchived=!m_editorArchived;m_editorPinned=false;Save();}
    void SessionsViewModel::TogglePin(){if(!m_hasSelection)return;m_editorPinned=!m_editorPinned;Save();}
    void SessionsViewModel::Resume(){if(!m_hasSelection||!m_repo)return;auto s=Build();s.IsActive=true;s.LastResumedAt=Now();(void)m_repo->Upsert(s);SetStatus(m_strings->GetString(L"Sessions.Resumed.Text"),{},Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);Refresh();}
    winrt::hstring SessionsViewModel::ExportText()const{std::wstring t;for(auto const&s:m_models){t+=L"["+EscapeExportValue(s.Name)+L"]\r\nChannels="+EscapeExportValue(s.Channels)+L"\r\nFilter="+EscapeExportValue(s.Filter)+L"\r\nDescription="+EscapeExportValue(s.Description)+L"\r\nTags="+EscapeExportValue(s.Tags)+L"\r\nNotes="+EscapeExportValue(s.Notes)+L"\r\nCorrelationGroup="+EscapeExportValue(s.CorrelationGroup)+L"\r\nRemoteTarget="+EscapeExportValue(s.RemoteTarget)+L"\r\nStartAt="+EscapeExportValue(s.StartAt)+L"\r\nEndAt="+EscapeExportValue(s.EndAt)+L"\r\nBookmarkedEventIds="+EscapeExportValue(s.BookmarkedEventIds)+L"\r\nExportSettings="+EscapeExportValue(s.ExportSettings)+L"\r\nCreatedAt="+EscapeExportValue(s.CreatedAt)+L"\r\nLastResumedAt="+EscapeExportValue(s.LastResumedAt)+L"\r\nPinned="+(s.IsPinned?L"1":L"0")+L"\r\nActive="+(s.IsActive?L"1":L"0")+L"\r\nArchived="+(s.IsArchived?L"1":L"0")+L"\r\n\r\n";}return winrt::hstring{t};}
    void SessionsViewModel::ImportText(winrt::hstring const& text){if(!m_repo)return;std::wstring input{text.c_str()};::AstralChronicle::models::DiagnosticSession s;bool active{};std::size_t start{};auto commit=[&]{if(!active||s.Name.empty())return;s.Id=NewId()+L"-import";if(s.CreatedAt.empty())s.CreatedAt=Now();(void)m_repo->Upsert(s);s={};s.Filter=L"*";s.Channels=L"System";active=false;};while(start<=input.size()){auto end=input.find(L'\n',start);auto line=input.substr(start,end==std::wstring::npos?std::wstring::npos:end-start);if(!line.empty()&&line.back()==L'\r')line.pop_back();if(line.size()>2&&line.front()==L'['&&line.back()==L']'){commit();s.Name=UnescapeExportValue(line.substr(1,line.size()-2));active=true;}else if(line.rfind(L"Channels=",0)==0)s.Channels=UnescapeExportValue(line.substr(9));else if(line.rfind(L"Filter=",0)==0)s.Filter=UnescapeExportValue(line.substr(7));else if(line.rfind(L"Description=",0)==0)s.Description=UnescapeExportValue(line.substr(12));else if(line.rfind(L"Tags=",0)==0)s.Tags=UnescapeExportValue(line.substr(5));else if(line.rfind(L"Notes=",0)==0)s.Notes=UnescapeExportValue(line.substr(6));else if(line.rfind(L"CorrelationGroup=",0)==0)s.CorrelationGroup=UnescapeExportValue(line.substr(17));else if(line.rfind(L"RemoteTarget=",0)==0)s.RemoteTarget=UnescapeExportValue(line.substr(13));else if(line.rfind(L"StartAt=",0)==0)s.StartAt=UnescapeExportValue(line.substr(8));else if(line.rfind(L"EndAt=",0)==0)s.EndAt=UnescapeExportValue(line.substr(6));else if(line.rfind(L"BookmarkedEventIds=",0)==0)s.BookmarkedEventIds=UnescapeExportValue(line.substr(19));else if(line.rfind(L"ExportSettings=",0)==0)s.ExportSettings=UnescapeExportValue(line.substr(15));else if(line.rfind(L"CreatedAt=",0)==0)s.CreatedAt=UnescapeExportValue(line.substr(10));else if(line.rfind(L"LastResumedAt=",0)==0)s.LastResumedAt=UnescapeExportValue(line.substr(14));else if(line.rfind(L"Pinned=",0)==0)s.IsPinned=ParseBool(line.substr(7));else if(line.rfind(L"Active=",0)==0)s.IsActive=ParseBool(line.substr(7));else if(line.rfind(L"Archived=",0)==0)s.IsArchived=ParseBool(line.substr(9));if(end==std::wstring::npos)break;start=end+1;}commit();SetStatus(m_strings->GetString(L"Sessions.Imported.Text"),{},Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);Refresh();}
    winrt::hstring SessionsViewModel::Heading()const{return m_heading;}winrt::hstring SessionsViewModel::Summary()const{return m_summary;}winrt::hstring SessionsViewModel::StatusText()const{return m_statusText;}winrt::hstring SessionsViewModel::StatusDetails()const{return m_statusDetails;}winrt::hstring SessionsViewModel::SelectedName()const{return m_selectedName;}void SessionsViewModel::SelectedName(winrt::hstring const&v){m_selectedName=v;Raise(L"SelectedName");Raise(L"CanSave");}winrt::hstring SessionsViewModel::EditorDescription()const{return m_editorDescription;}void SessionsViewModel::EditorDescription(winrt::hstring const&v){m_editorDescription=v;Raise(L"EditorDescription");}winrt::hstring SessionsViewModel::EditorChannels()const{return m_editorChannels;}void SessionsViewModel::EditorChannels(winrt::hstring const&v){m_editorChannels=v;Raise(L"EditorChannels");}winrt::hstring SessionsViewModel::EditorFilter()const{return m_editorFilter;}void SessionsViewModel::EditorFilter(winrt::hstring const&v){m_editorFilter=v;Raise(L"EditorFilter");}winrt::hstring SessionsViewModel::EditorTags()const{return m_editorTags;}void SessionsViewModel::EditorTags(winrt::hstring const&v){m_editorTags=v;Raise(L"EditorTags");}winrt::hstring SessionsViewModel::EditorNotes()const{return m_editorNotes;}void SessionsViewModel::EditorNotes(winrt::hstring const&v){m_editorNotes=v;Raise(L"EditorNotes");}winrt::hstring SessionsViewModel::EditorCorrelationGroup()const{return m_editorCorrelationGroup;}void SessionsViewModel::EditorCorrelationGroup(winrt::hstring const&v){m_editorCorrelationGroup=v;Raise(L"EditorCorrelationGroup");}winrt::hstring SessionsViewModel::EditorRemoteTarget()const{return m_editorRemoteTarget;}void SessionsViewModel::EditorRemoteTarget(winrt::hstring const&v){m_editorRemoteTarget=v;Raise(L"EditorRemoteTarget");}winrt::hstring SessionsViewModel::EditorStartAt()const{return m_editorStartAt;}void SessionsViewModel::EditorStartAt(winrt::hstring const&v){m_editorStartAt=v;Raise(L"EditorStartAt");}winrt::hstring SessionsViewModel::EditorEndAt()const{return m_editorEndAt;}void SessionsViewModel::EditorEndAt(winrt::hstring const&v){m_editorEndAt=v;Raise(L"EditorEndAt");}winrt::hstring SessionsViewModel::EditorBookmarkedEventIds()const{return m_editorBookmarkedEventIds;}void SessionsViewModel::EditorBookmarkedEventIds(winrt::hstring const&v){m_editorBookmarkedEventIds=v;Raise(L"EditorBookmarkedEventIds");}winrt::hstring SessionsViewModel::ActiveSummary()const{return m_activeSummary;}bool SessionsViewModel::EditorPinned()const noexcept{return m_editorPinned;}void SessionsViewModel::EditorPinned(bool v){m_editorPinned=v;Raise(L"EditorPinned");}bool SessionsViewModel::EditorArchived()const noexcept{return m_editorArchived;}bool SessionsViewModel::HasSelection()const noexcept{return m_hasSelection;}bool SessionsViewModel::CanSave()const noexcept{return !m_selectedName.empty();}std::int32_t SessionsViewModel::SelectedIndex()const noexcept{return m_selectedIndex;}Windows::Foundation::Collections::IObservableVector<winrt::hstring> SessionsViewModel::Sessions()const{return m_sessions;}Microsoft::UI::Xaml::Controls::InfoBarSeverity SessionsViewModel::StatusSeverity()const noexcept{return m_severity;}bool SessionsViewModel::HasStatusMessage()const noexcept{return m_hasStatus;}bool SessionsViewModel::IsLoading()const noexcept{return m_loading;}
    void SessionsViewModel::SetStatus(winrt::hstring a,winrt::hstring b,Microsoft::UI::Xaml::Controls::InfoBarSeverity c){m_statusText=std::move(a);m_statusDetails=std::move(b);m_severity=c;m_hasStatus=true;Raise(L"StatusText");Raise(L"StatusDetails");Raise(L"StatusSeverity");Raise(L"HasStatusMessage");}void SessionsViewModel::RaiseEditor(){for(auto const&p:{L"SelectedName",L"EditorDescription",L"EditorChannels",L"EditorFilter",L"EditorTags",L"EditorNotes",L"EditorCorrelationGroup",L"EditorRemoteTarget",L"EditorStartAt",L"EditorEndAt",L"EditorBookmarkedEventIds",L"EditorPinned",L"EditorArchived",L"ActiveSummary",L"CanSave"})Raise(p);}void SessionsViewModel::Raise(winrt::hstring const&n){m_changed(*this,Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{n});}winrt::event_token SessionsViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&h){return m_changed.add(h);}void SessionsViewModel::PropertyChanged(winrt::event_token const&t)noexcept{m_changed.remove(t);}
}
