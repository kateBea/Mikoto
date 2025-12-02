//
// Created by kate on 11/22/25.
//

#include <exception>

#include <Platform/Window.hh>

// Globals
Mikoto::Window* g_Window{};

auto InitializeWindow() -> void {
    using namespace Mikoto;

    // Create a window
    WindowProperties properties{};
    properties.Resizable = true;
    properties.Title = "Hello World";
    properties.Backend = GraphicsAPI::DIRECTX_11;
    properties.Width = 1280;
    properties.Height = 720;

    g_Window =  Window::Create( properties );

    if ( g_Window ) {
        g_Window->Init();
    } else {
        MKT_THROW_RUNTIME_ERROR( "Failed to create main application window!" );
    }
}

auto RunCleanup() -> void {

    g_Window->Shutdown();
    delete g_Window;
    g_Window = nullptr;

}

auto main( const int, char** ) -> int {
    InitializeWindow();

    while (!g_Window->ShouldClose()) {
        g_Window->ProcessEvents();
    }

    RunCleanup();

    return 0;
}