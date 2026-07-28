<p align="center">
  <img src="Assets/StoreLogo.scale-200.png" width="112" alt="AstralChronicle logo" />
</p>

<h1 align="center">AstralChronicle</h1>

<p align="center">
  <strong>更易讀、更好搜尋的 Windows 事件智慧檢視器</strong>
</p>

<p align="center">
  <a href="https://github.com/Eternity-boundary/AstralChronicle/actions/workflows/msbuild.yml">
    <img src="https://github.com/Eternity-boundary/AstralChronicle/actions/workflows/msbuild.yml/badge.svg" alt="MSBuild 狀態" />
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/License-GPL--3.0-blue.svg" alt="GPL-3.0 授權" />
  </a>
  <img src="https://img.shields.io/badge/Platform-Windows%2010%201809%2B-0078D4?logo=windows&logoColor=white" alt="Windows 10 1809 或更新版本" />
  <img src="https://img.shields.io/badge/UI-WinUI%203-0078D4?logo=windows&logoColor=white" alt="WinUI 3" />
  <img src="https://img.shields.io/badge/Language-C%2B%2B%2FWinRT-00599C?logo=cplusplus&logoColor=white" alt="C++/WinRT" />
  <img src="https://img.shields.io/badge/Package-MSIX-5E5E5E?logo=windows&logoColor=white" alt="MSIX 桌面應用程式" />
  <img src="https://img.shields.io/badge/Languages-4-6E40C9" alt="四種介面語言" />
</p>

<p align="center">
  <a href="#目前功能">功能</a> ·
  <a href="#快速開始">快速開始</a> ·
  <a href="#文件">文件</a> ·
  <a href="#授權">授權</a>
</p>

---

AstralChronicle 是以 WinUI 3 與 C++/WinRT 開發的 Windows 事件智慧檢視器。它保留 Windows 事件記錄的原始資料與權限模型，同時提供更易讀、可搜尋且支援佈景主題的桌面體驗。

> [!IMPORTANT]
> 專案仍在積極開發中。事件來源、系統權限與 Windows 版本都會影響可見資料；部分頻道（尤其是 Security）需要系統管理員權限。

## 預覽截圖

<p align="center">
  <img width="480" alt="AstralChronicle 儀表板" src="https://github.com/user-attachments/assets/a7e79ac1-4c44-4090-a620-d0fa0e6c8a5c" />
  <img width="480" alt="AstralChronicle 事件記錄" src="https://github.com/user-attachments/assets/4973660c-5c60-4c1d-ad75-e808fce71291" />
</p>

## 目前功能

| 領域 | 功能 |
| --- | --- |
| 儀表板 | 顯示今日事件數、近期重大事件與當機時間軸摘要。 |
| 事件記錄 | 瀏覽 Windows 記錄、延後載入「應用程式及服務記錄」、搜尋、篩選、排序、書籤、複製與匯出。 |
| 自訂檢視 | 展開時從 `%ProgramData%\Microsoft\Event Viewer\Views` 背景讀取既有資料夾與檢視；「系統管理事件」使用跨頻道 QueryList 查詢。 |
| 提供者 | 搜尋事件提供者，並查看中繼資料、路徑與事件定義。 |
| 進階工具 | 時間軸、即時監控、遠端查詢、工作階段與已儲存檢視。 |
| 介面語言 | English、繁體中文、简体中文與日本語。 |

## 快速開始

### 需求

- Windows 10 1809（17763）或更新版本
- Visual Studio 2022，並安裝「Desktop development with C++」工作負載
- 相容的 Windows SDK

### 建置與啟動

1. 以 Visual Studio 開啟 [AstralChronicle.slnx](AstralChronicle.slnx)。
2. 選擇 `Debug` + `x64`，建置並使用 Visual Studio 的偵錯啟動。

也可以在 Developer PowerShell 執行：

```powershell
MSBuild.exe AstralChronicle.slnx /m /p:Configuration=Debug /p:Platform=x64
```

這是 MSIX 桌面應用程式，請透過 Visual Studio 的部署／偵錯流程或 Windows AppsFolder 啟動，勿直接執行輸出的 `.exe`。若需要存取受保護的頻道，可在介面中使用「以管理員身分重新啟動」。

## 從原始碼建置

```powershell
$ErrorActionPreference = 'Stop'

$repositoryUrl = 'https://github.com/Eternity-boundary/AstralChronicle.git'
$repositoryPath = Join-Path (Get-Location) 'AstralChronicle'
$configuration = 'Debug' # or Release
$platform = 'x64'

if (Test-Path -LiteralPath $repositoryPath) {
    throw "directory already exists：$repositoryPath"
}

& git clone $repositoryUrl $repositoryPath
if ($LASTEXITCODE -ne 0) {
    throw "git clone failed, code：$LASTEXITCODE"
}

Set-Location -LiteralPath $repositoryPath

$nuget = (Get-Command 'nuget.exe' -ErrorAction Stop).Source
$msbuild = (Get-Command 'MSBuild.exe' -ErrorAction Stop).Source

& $nuget restore '.\packages.config' '-PackagesDirectory' '.\packages'
if ($LASTEXITCODE -ne 0) {
    throw "NuGet restore failed, code：$LASTEXITCODE"
}

$buildArgs = @(
    '.\AstralChronicle.slnx'
    '/m'
    '/p:AppxPackageSigningEnabled=false'
    "/p:Configuration=$configuration"
    "/p:Platform=$platform"
)

& $msbuild @buildArgs
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed, code：$LASTEXITCODE"
}

Write-Host "build successful：$configuration | $platform"
```

## 文件

- [架構與服務邊界](docs/architecture.md)
- [DesignSystem 與佈景主題規範](docs/design-system.md)
- [測試說明](tests/README.md)

## AI 輔助程式碼使用範圍

本專案主要由一位貢獻者獨立開發。為降低錯誤並提升程式碼品質，開發過程可能使用人工智慧輔助：

- Code review
- 註解
- 部分程式碼與翻譯單元

所有變更皆在合併前經過手動驗證與測試。包含 AI 產生程式碼的翻譯單元會在檔案頂端註明。

## 授權

本專案採用 [GNU General Public License v3.0](LICENSE)（GPL-3.0）授權。
