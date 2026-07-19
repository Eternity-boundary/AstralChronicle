# AstralChronicle architecture

AstralChronicle is a WinUI 3 desktop event viewer written in C++/WinRT.

## Source layout

- `src/App` — application lifetime, startup, resources, and composition root.
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
