//
// Created by kate on 11/22/25.
//

#include <exception>

#include <Application/HelloWorld.hh>

// Globals
Mikoto::Window* g_Window{};
MikotoApp::HelloWorld* g_Application{};

auto InitializeWindow() -> void {
    using namespace Mikoto;
    using namespace MikotoApp;

    // Create a window
    WindowProperties properties{};
    properties.Resizable = true;
    properties.Title = "Hello World";
    properties.Backend = GraphicsAPI::VULKAN_API;
    properties.Width = 1280;
    properties.Height = 720;

    g_Window =  Window::Create( properties );

    if ( g_Window ) {
        g_Window->Init();
    } else {
        MKT_THROW_RUNTIME_ERROR( "Failed to create main application window!" );
    }
}

auto InitializeApplication( const int argc, char** argv ) -> void {
    using namespace Mikoto;
    using namespace MikotoApp;

    g_Application = dynamic_cast<HelloWorld*>( CreateApplication( argc, argv ) );

    g_Application->SetWindow( g_Window );

    g_Application->Init();
}

auto RunApplication(  const int argc, char** argv ) -> Mikoto::Int32 {
    Mikoto::Int32 returnCode{};

    try {
        returnCode = g_Application->Run( argc, argv );
    } catch ( std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "MikotoApp Exception: e.what() {}", e.what() );
    }

    return returnCode;
}

auto RunCleanup() -> void {
    g_Application->Shutdown();

    delete g_Application;
    g_Application = nullptr;

    g_Window->Shutdown();

    delete g_Application;
    g_Application = nullptr;
}

auto main( const int argc, char** argv ) -> int {
    InitializeWindow();
    InitializeApplication( argc, argv );

    const auto code{ RunApplication( argc, argv ) };

    RunCleanup();

    return code;
}