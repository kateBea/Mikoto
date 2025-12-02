//
// Created by kate on 11/22/25.
//

#include <exception>

#include <HelloWorld.hh>

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

    g_Application = new HelloWorld();

    g_Application->SetWindow( g_Window );

    g_Application->Init();
}

auto RunApplication() -> void {

    try {
        g_Application->Run();
    } catch ( std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "MikotoApp Exception: e.what() {}", e.what() );
    }

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

    RunApplication();

    RunCleanup();

    return 0;
}