# AstralChronicle architecture

AstralChronicle is a WinUI 3 desktop event viewer written in C++/WinRT.

## Source layout

- `src/App` — application lifetime, startup, resources, and composition root.
- `src/DesignSystem` — shared Fluent theme tokens, control styles, spacing, and the injected theme service.
- `src/Core` — shared primitives and cross-cutting infrastructure with no UI dependency.
- `src/Features` — feature-owned event-viewing workflows; create a subdirectory per feature.
- `src/Models` — immutable or value-like domain and presentation data.
- `src/Services` — Windows Event Log access, persistence, and platform integrations.
- `src/ViewModels` — presentation state and commands; no XAML UI ownership.
- `src/Views` — XAML views, controls, and window shell.

The initial window belongs to `src/Views/Shell`. Keep new pages in `src/Views/Pages`, reusable controls in `src/Views/Controls`, and organise feature-specific code beneath `src/Features/<FeatureName>`.

## Project conventions

- Keep the Windows App SDK and MSIX manifest at the repository root.
- Add every authored source file explicitly to `AstralChronicle.vcxproj` and its matching entry to `AstralChronicle.vcxproj.filters`.
- Use `x64` or `ARM64` for Windows desktop builds; do not introduce AnyCPU.

## Application composition

`AppHost` is the composition root. It registers all application services by interface and owns the service provider for the process lifetime. UI code receives the host exactly once during window startup; page creation is centralized through `INavigationService` factories.

Pages are intentionally created only after their route is selected. The current `NavigationService` keeps the policy and route catalog in one place; feature modules will register their own factories through a plugin-facing registration contract.

## Windows Event Log boundary

`IEventLogCatalogService` enumerates registered channels, while `IEventQueryService` retrieves record summaries and details through `EvtQuery`/`EvtNext`/`EvtRender`. Querying is cancellation-aware and occurs away from the UI thread. Channel queries use `EvtQueryChannelPath`; structured custom-view queries use an empty path with a Windows `<QueryList>`, which allows one view to combine multiple channels.

`ICustomViewCatalogService` reads the existing Event Viewer view tree from `%ProgramData%\Microsoft\Event Viewer\Views`. It keeps folder hierarchy and the original QueryList separate from the shell so the `NavigationView` can materialize custom folders only when the user expands the branch. `IEventProviderService`, `IEventLiveService`, `IRemoteEventService`, `ISavedViewRepository`, and `ISessionRepository` follow the same interface-first boundary for their corresponding pages.

Windows implementations own Win32 handles and platform-specific file access. View models depend only on interfaces, so paging, EVTX file support, remote sessions, subscriptions, and test doubles can evolve without coupling UI code to platform handles.

## MVVM in native C++/WinRT

AstralChronicle therefore uses the Windows App SDK-native equivalent: runtime-class view models implement `Microsoft.UI.Xaml.Data.INotifyPropertyChanged`, and views use compile-time `x:Bind` with `Mode=OneWay`. This retains observable state and compile-time binding validation while preserving a pure C++23/WinRT process.

## DesignSystem and theme service

The DesignSystem is the only home for shared visual decisions. Pages consume semantic
resources such as `AstralPrimaryTextBrush`, `AstralCardBackgroundBrush`, and
`AstralCardBorderStyle`; page-local colors and duplicate card styles are deliberately
not allowed. `Colors.xaml` has explicit Light, Dark, and HighContrast dictionaries and
uses Windows system/WinUI resources so text remains readable when the user changes
theme, accent, contrast, or display scale.

The theme service is an interface-driven adaptation of the existing EtherealScepter
`ThemeService` pattern. It keeps DI ownership, persisted theme selection, and change
subscriptions. In addition to System/Light/Dark, the settings page exposes the
StarrySky and BlackSoulsLeaf artwork themes. Those modes load the matching main and
side images from `Assets` and add dark contrast overlays while using the Dark Fluent
control theme. High Contrast hides the artwork and lets system colors own the visual
contract. `System` maps to `ElementTheme::Default`, allowing Windows High Contrast to
select the dedicated resource dictionary automatically.

`IStringResourceService` and the `Strings/<locale>/Resources.resw` files keep visible
shell and ViewModel text localizable. Static XAML uses `x:Uid`, while dynamic labels
use resource templates before they reach the view. This keeps the DesignSystem
language-neutral and prevents new pages from adding English-only literals.

## Native precompiled header

`src/pch.h` is included first by application implementation files and contains only
stable Windows, C++/WinRT, WinUI, WIL, and commonly used C++ standard-library
headers. It defines `WIN32_LEAN_AND_MEAN` and `NOMINMAX` before `windows.h`, then
undefines `GetCurrentTime` to avoid the Win32 macro collision with WinUI's
`Storyboard::GetCurrentTime`.

Do not add application headers, generated headers, feature-only APIs, or frequently
changing code to the PCH. Every public header must continue to include its own
dependencies so it remains valid when compiled independently.
