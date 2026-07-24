# AstralChronicle

![Language](https://img.shields.io/badge/language-C++%2FWinRT-red) [![MSBuild](https://github.com/Eternity-boundary/AstralChronicle/actions/workflows/msbuild.yml/badge.svg)](https://github.com/Eternity-boundary/AstralChronicle/actions/workflows/msbuild.yml)


AstralChronicle 是以 WinUI 3 與 C++/WinRT 開發的 Windows 事件智慧檢視器。它保留 Windows 事件記錄的原始資料與權限模型，同時提供較易讀、可搜尋且支援佈景主題的桌面體驗。

> 專案仍在積極開發中；事件來源、系統權限與 Windows 版本都會影響可見資料。

## 預覽截圖
<img width="2862" height="1491" alt="image" src="https://github.com/user-attachments/assets/a7e79ac1-4c44-4090-a620-d0fa0e6c8a5c" />
<img width="2839" height="1490" alt="image" src="https://github.com/user-attachments/assets/4973660c-5c60-4c1d-ad75-e808fce71291" />




## 目前功能

- 儀表板：顯示今日事件數、近期重大事件與當機時間軸摘要
- 事件記錄：瀏覽 Windows 記錄、延後載入「應用程式及服務記錄」、搜尋、篩選、排序、書籤、複製與匯出
- 自訂檢視：展開時從 `%ProgramData%\Microsoft\Event Viewer\Views` 背景讀取既有資料夾與檢視；「系統管理事件」使用跨頻道 QueryList 查詢
- 提供者：搜尋事件提供者並查看中繼資料、路徑與事件定義
- 時間軸、即時監控、遠端查詢、工作階段與已儲存檢視
- 正體中文與英文資源

## 開發環境與建置

需要 Windows 10 1809（17763）或更新版本、Visual Studio 2022 的「Desktop development with C++」工作負載，以及相容的 Windows SDK。

1. 以 Visual Studio 開啟 [AstralChronicle.slnx](AstralChronicle.slnx)。
2. 選擇 `Debug` + `x64`，建置並使用 Visual Studio 的偵錯啟動。

也可從 Developer PowerShell 執行：

```powershell
MSBuild.exe AstralChronicle.slnx /m /p:Configuration=Debug /p:Platform=x64
```

這是 MSIX 桌面應用程式，請透過 Visual Studio 的部署／偵錯流程或 Windows AppsFolder 啟動，勿直接執行輸出的 `.exe`。

某些頻道（特別是 Security）需要系統管理員權限。若頁面顯示權限不足，可使用介面提供的「以管理員身分重新啟動」操作。


## 文件

- [架構與服務邊界](docs/architecture.md)
- [DesignSystem 與佈景主題規範](docs/design-system.md)
- [測試說明](tests/README.md)

---
## AI產生程式碼使用範圍
- Code Review
- 注解
- 部分程式碼

本專案主要由一位貢獻者獨立開發。

為減少錯誤並提高程式碼質量，開發過程中可能會使用人工智慧輔助程式碼審查。

所有變更在合併前已經經過手動驗證和測試。

並且爲了維護程式碼的可讀性，和加快程式開發，使用AI產生了部分注解和程式碼，包含由AI產生程式碼的翻譯單元會在檔案頂部註明

## 授權

本專案採用 GNU General Public License v3.0（GPL-3.0）授權。
