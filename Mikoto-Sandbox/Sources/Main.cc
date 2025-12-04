/**
 * EntryPoint.cc
 * Created by kaTe on 12/11/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <SandboxApp.hh>

Mikoto::Window* g_Window{ nullptr };
Mikoto::SandboxApp* g_Application{ nullptr };

auto InitializeWindow() -> void {
    using namespace Mikoto;

    WindowProperties properties{};
    properties.Resizable = true;
    properties.Title = "Sandbox Application [Vulkan]";
    properties.Backend = GraphicsAPI::VULKAN_API;
    properties.Width = 1280;
    properties.Height = 720;

    g_Window = Window::Create( properties );

    g_Window->Init();
}

auto main(int, char**) -> int {
    using namespace Mikoto;

    InitializeWindow();

    g_Application = new SandboxApp{};

    g_Application->SetWindow( g_Window );
    g_Application->Init();

    g_Application->Run();

    g_Application->Shutdown();

    return 0;
}