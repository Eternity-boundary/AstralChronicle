// Created by EternityBoundary on Jul 19,2026
#include "pch.h"
#include "App.xaml.h"
#include "../Views/Shell/MainWindow.xaml.h"
#include "AppHost.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::AstralChronicle::implementation
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    /// <summary>
    /// Invoked when the application is launched.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        Resources().MergedDictionaries().Append(Microsoft::UI::Xaml::Controls::XamlControlsResources{});

        auto const designSystem = ResourceDictionary{};
        designSystem.Source(Windows::Foundation::Uri{ L"ms-appx:///src/DesignSystem/DesignSystem.xaml" });
        Resources().MergedDictionaries().Append(designSystem);

        m_host = std::make_unique<::AstralChronicle::app::AppHost>();
        auto mainWindow = make<MainWindow>();
        get_self<MainWindow>(mainWindow)->Initialize(*m_host);
        window = mainWindow;
        window.Activate();
    }
}
