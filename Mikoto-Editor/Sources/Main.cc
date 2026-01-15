/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

#include <exception>

// Project Headers
#include <Application/EditorApp.hh>
#include <Application/EditorConfigLoader.hh>
#include <Core/Profiler.hh>
#include <Layers/EditorLayer.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>

Mikoto::Window* g_Window{ nullptr };
Mikoto::EditorApp* g_Application{ nullptr };

constexpr std::string_view g_ConfidPath{ "app-config.toml" };
const Mikoto::BaseConfiguration g_Config{ g_ConfidPath };

auto InitializeWindow() -> void {
    using namespace Mikoto;

    if (!g_Config.IsLoaded()) {
        std::printf( "Could not load file at %s·", g_ConfidPath.data() );
        return;
    }

    WindowProperties properties{
        .Title{ g_Config.Get<std::string>( "application.title" ) },
        .Width{ static_cast<Int32>( g_Config.Get<Int64>( "application.width" ) ) },
        .Height{ static_cast<Int32>( g_Config.Get<Int64>( "application.height" )) },
        .Backend{ InferAPI( g_Config.Get<std::string>( "renderer.api" ) ) },
        .Resizable{ g_Config.Get<bool>( "application.resizable" ) }
    };

    g_Window = Window::Create( properties );

    g_Window->Init();
}

auto InitializeApplication() -> void {
    using namespace Mikoto;

    if (!g_Window) {
        return;
    }

    g_Application = new EditorApp{};

    try {

        g_Application->SetWindow( g_Window );
        g_Application->Init();

        // Register the editor
        EditorLayerCreateInfo spec{
            .Name{ "Editor Layer" },
            .TargetWindow{ g_Window },
            .ModelsRootDirectory{ g_Config.Get<std::string>( "assets.path" ) }
        };

        g_Application->PushLayer<EditorLayer>( spec );

    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Initializing application - Exception: e.what(): {}", e.what() );
    }
}

auto RunCleanup() -> void {
    if (g_Application) {
        g_Application->Shutdown();
    }

    delete g_Application;

    if (g_Window) {
        g_Window->Shutdown();
    }

    delete g_Window;
}

auto RunApplication() -> void {
    if (!g_Application || !g_Window) {
        return;
    }

    try {

        g_Application->Run();

    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Running application - Exception: e.what(): {}", e.what() );
    }
}


auto Usage(const int argc)-> bool {
    if ( argc != 1 ) {
        std::printf( "MikotoEditor takes no arguments." );
        return false;
    }

    return true;
}

auto main( const int argc, char** ) -> int {
    MKT_BEGIN_PROFILER_NAMED();

    if (!Usage( argc )) {
        return 1;
    }

    InitializeWindow();

    InitializeApplication();

    RunApplication();

    RunCleanup();

    return 0;
}