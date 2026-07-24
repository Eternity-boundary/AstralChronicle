[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateRange(1, [int]::MaxValue)]
    [int]$AppPid
)

$ErrorActionPreference = 'Continue'
$winApp = 'C:\Users\EternityBoundary\AppData\Local\Programs\WinAppCLI\winapp.exe'
$passed = 0
$failed = 0
$results = [System.Collections.Generic.List[object]]::new()

function Invoke-UiCommand {
    param([Parameter(Mandatory)][string[]]$NativeArgs)

    $output = & $winApp @NativeArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw ($output -join [Environment]::NewLine)
    }
}

function Test-Ui {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Action
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

$pages = @(
    @{ NavigationId = 'DashboardNavigationItem'; ReadyId = 'DashboardHeading' },
    @{ NavigationId = 'EventLogsNavigationItem'; ReadyId = 'EventLogsHeading' },
    @{ NavigationId = 'TimelineNavigationItem'; ReadyId = 'TimelineHeading' },
    @{ NavigationId = 'ProvidersNavigationItem'; ReadyId = 'ProvidersHeading' },
    @{ NavigationId = 'SessionsNavigationItem'; ReadyId = 'SessionsHeading' },
    @{ NavigationId = 'SavedViewsNavigationItem'; ReadyId = 'SavedViewsHeading' },
    @{ NavigationId = 'LiveNavigationItem'; ReadyId = 'LiveHeading' },
    @{ NavigationId = 'RemoteNavigationItem'; ReadyId = 'RemoteHeading' },
    @{ NavigationId = 'SettingsNavigationItem'; ReadyId = 'ThemeModeRadioButtons' }
)

foreach ($page in $pages) {
    Test-Ui "Navigate to $($page.NavigationId)" {
        Invoke-UiCommand @('ui', 'invoke', $page.NavigationId, '-a', $AppPid)
        Invoke-UiCommand @('ui', 'wait-for', $page.ReadyId, '-a', $AppPid, '-t', '5000')
    }
}

$results | Format-Table -AutoSize | Out-String | Write-Host
Write-Host "Passed: $passed | Failed: $failed"
if ($failed -gt 0) {
    exit 1
}
