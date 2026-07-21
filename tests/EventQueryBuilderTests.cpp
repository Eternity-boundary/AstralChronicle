// Created by EternityBoundary on Jul 20,2026
#include "../src/Services/EventQueryBuilder.h"

#include <cassert>

int main()
{
    AstralChronicle::models::EventFilter filter;
    filter.Provider = L"Microsoft-Windows-Kernel-Power";
    filter.EventId = L"41";
    filter.Level = L"2";
    filter.Opcode = L"1";
    filter.Keyword = L"0x20";
    filter.ProcessId = L"100";
    filter.ThreadId = L"200";
    filter.AfterHours = 6;
    filter.AfterToday = true;

    auto const query = AstralChronicle::services::BuildEventQuery(filter);
    assert(query ==
        L"*[System[Provider[@Name='Microsoft-Windows-Kernel-Power'] and EventID=41 and Level=2 and Opcode=1 and band(Keywords, 32) and Execution[@ProcessID=100] and Execution[@ThreadID=200] and TimeCreated[timediff(@SystemTime) <= 86400000]]]" );

    filter.AfterToday = false;
    assert(AstralChronicle::services::BuildEventQuery(filter).find(L"21600000") != std::wstring::npos);

    filter.RawXPath = L"*[System[Level=1 or Level=2]]";
    assert(AstralChronicle::services::BuildEventQuery(filter) == filter.RawXPath);

    auto const combinedQuery = AstralChronicle::services::CombineEventQueries(
        L"*[System[Level=2]]",
        L"*[System[EventID=41]]");
    assert(combinedQuery && *combinedQuery ==
        L"*[(System[Level=2]) and (System[EventID=41])]");

    auto const structuredQuery = AstralChronicle::services::ApplyFilterToStructuredQuery(
        L"<QueryList><Query Id=\"0\"><Select Path=\"Application\">*[System[Level=2]]</Select><Select Path=\"System\">*</Select></Query></QueryList>",
        L"*[System[EventID=41]]");
    assert(structuredQuery && *structuredQuery ==
        L"<QueryList><Query Id=\"0\"><Select Path=\"Application\">*[(System[Level=2]) and (System[EventID=41])]</Select><Select Path=\"System\">*[System[EventID=41]]</Select></Query></QueryList>");

    filter = {};
    assert(AstralChronicle::services::BuildEventQuery(filter) == L"*");
    return 0;
}
