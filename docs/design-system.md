# AstralChronicle DesignSystem

`src/DesignSystem` is the only shared UI foundation for AstralChronicle. Pages and
features consume its semantic resources and built-in WinUI control styles; they do
not define page-local colors, brushes, templates, or duplicate card styles.

## Resource layers

- `DesignSystem.xaml` composes the resource dictionaries in a stable order.
- `Resources/Colors.xaml` maps semantic roles to WinUI Fluent brushes for `Light`,
  `Dark`, and `HighContrast`. The High Contrast branch redirects only to
  `SystemColor*Brush` resources so Windows controls remain legible under user-selected
  contrast themes.
- `Resources/Typography.xaml` owns spacing and content-width tokens in device-
  independent units.
- `Resources/Controls.xaml` contains lightweight styles based on the platform text,
  surface, `Border`, `Grid`, `ScrollViewer`, and `NavigationView` defaults.
- `Theme/ThemeService` owns System/Light/Dark/StarrySky/BlackSoulsLeaf selection,
  persistence, and change subscriptions. The two artwork modes load the corresponding
  `Assets/*Main.png` and `Assets/*Side.png` brushes and apply dark contrast overlays;
  High Contrast hides those images and uses system colors. `System` applies
  `ElementTheme::Default`, allowing Windows to select High Contrast resources
  automatically.
- `Localization/StringResourceService` reads the app's `.resw` resources. Static
  XAML strings use `x:Uid`; dynamic ViewModel strings use resource keys so pages do
  not contain English-only display text. Stable automation identifiers are assigned
  directly in XAML, while visible labels remain localized through `x:Uid`.

## Contribution contract

When adding a component, first prefer a built-in WinUI control. Add a semantic token
or a reusable style here, add all three theme branches when a custom resource is
needed, and consume it from pages with `ThemeResource` at the usage site. Keep
keyboard behavior, focus visuals, automation names, and localization in the shared
component contract. Avoid custom templates unless a platform control cannot satisfy
the interaction and the component has explicit keyboard, UI Automation, High
Contrast, and responsive behavior coverage.

All layout dimensions are XAML DIPs, not physical pixels; no page performs custom
pixel scaling or draws hard-coded color literals. Built-in `NavigationView`,
`ScrollViewer`, `InfoBar`, and text controls provide the keyboard, focus, scaling,
and screen-reader behavior used by the current shell.

## Data-dense pages

Event rows and provider metadata favour standard `Grid`, `ListView`, `TextBlock`,
and `NavigationViewItem` controls with semantic styles. Label-and-value cards use
separate grid columns, keep long paths wrapped in the value column, and never place
two text elements in the same grid cell. Use `x:Bind` for page view-model state;
only operating-system-discovered navigation items are created dynamically.

When a navigation branch can require a slow system query, show a localized loading
item and populate it asynchronously. The temporary state must use the existing
Fluent control styling rather than introducing page-specific brushes or templates.
