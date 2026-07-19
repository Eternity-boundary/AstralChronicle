[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateRange(1, [int]::MaxValue)]
    [int]$AppPid
)

$ErrorActionPreference = 'Stop'
$winApp = (Get-Command winapp -ErrorAction Stop).Source
$passed = 0
$failed = 0
$results = [System.Collections.Generic.List[object]]::new()

function Test-Ui {
    param(
        [Parameter(Mandatory)]
        [string]$Name,
        [Parameter(Mandatory)]
        [scriptblock]$Action
    )

    try {
        & $Action
        $script:passed++
        $script:results.Add([pscustomobject]@{ Name = $Name; Status = 'PASS'; Detail = '' })
    }
    catch {
        $script:failed++
        $script:results.Add([pscustomobject]@{ Name = $Name; Status = 'FAIL'; Detail = $_.Exception.Message })
    }
}

function Invoke-UiCommand {
    param([Parameter(Mandatory)][string[]]$Arguments)

    $output = & $winApp @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw ($output -join [Environment]::NewLine)
    }
}

Test-Ui 'Dashboard loads' {
    Invoke-UiCommand @('ui', 'wait-for', 'DashboardHeading', '-a', $AppPid, '-t', '3000')
}

Test-Ui 'Shell greeting is displayed' {
    Invoke-UiCommand @('ui', 'wait-for', 'ShellGreetingText', '-a', $AppPid, '-t', '3000')
}

Test-Ui 'Shell system status is displayed' {
    Invoke-UiCommand @('ui', 'wait-for', 'ShellSystemStatusText', '-a', $AppPid, '-t', '3000')
}

foreach ($card in @(
        'DashboardErrorCardTitle',
        'DashboardWarnCardTitle',
        'DashboardTodayCardTitle',
        'DashboardRecentCriticalEventsTitle',
        'DashboardCrashTimelineTitle')) {
    Test-Ui "Dashboard shows $card" {
        Invoke-UiCommand @('ui', 'wait-for', $card, '-a', $AppPid, '-t', '3000')
    }
}

Test-Ui 'Dashboard shows one service status card' {
    Invoke-UiCommand @('ui', 'wait-for', 'DashboardServiceStatusCardTitle', '-a', $AppPid, '-t', '3000')
    Invoke-UiCommand @('ui', 'wait-for', 'DashboardServiceStatusText', '-a', $AppPid, '-t', '3000')
}

foreach ($page in @(
        'EventLogsNavigationItem',
        'TimelineNavigationItem',
        'ProvidersNavigationItem',
        'SessionsNavigationItem',
        'SavedViewsNavigationItem',
        'LiveNavigationItem',
        'RemoteNavigationItem')) {
    Test-Ui "Navigate using $page" {
        Invoke-UiCommand @('ui', 'invoke', $page, '-a', $AppPid)
        Invoke-UiCommand @('ui', 'wait-for', 'FeaturePlaceholderHeading', '-a', $AppPid, '-t', '3000')
    }
}

Test-Ui 'Navigate to Settings' {
    Invoke-UiCommand @('ui', 'invoke', 'SettingsNavigationItem', '-a', $AppPid)
    Invoke-UiCommand @('ui', 'wait-for', 'ThemeModeRadioButtons', '-a', $AppPid, '-t', '3000')
    Invoke-UiCommand @('ui', 'wait-for', 'LanguageRadioButtons', '-a', $AppPid, '-t', '3000')
    Invoke-UiCommand @('ui', 'wait-for', 'LanguageEnglishOption', '-a', $AppPid, '-t', '3000')
    Invoke-UiCommand @('ui', 'wait-for', 'LanguageTraditionalChineseOption', '-a', $AppPid, '-t', '3000')
    Invoke-UiCommand @('ui', 'wait-for', 'AboutCardTitle', '-a', $AppPid, '-t', '3000')
}

Test-Ui 'Return to Dashboard' {
    Invoke-UiCommand @('ui', 'invoke', 'DashboardNavigationItem', '-a', $AppPid)
    Invoke-UiCommand @('ui', 'wait-for', 'DashboardHeading', '-a', $AppPid, '-t', '3000')
}

$results | Format-Table -AutoSize | Out-String | Write-Host
Write-Host "Passed: $passed | Failed: $failed"
if ($failed -gt 0) {
    exit 1
}
