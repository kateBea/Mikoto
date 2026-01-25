//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <exception>
#include <string_view>

#include <Core/Root.hh>
#include <Core/Profiler.hh>
#include <Logging/Logger.hh>

#include <Library/Utility/Types.hh>

#include <Layers/EditorLayer.hh>
#include <Application/EditorApp.hh>
#include <Application/EditorConfigLoader.hh>

#include <Platform/Window.hh>
#include <Platform/WindowsService.hh>

Mikoto::Window* g_Window{ nullptr };
Mikoto::EditorApp* g_Application{ nullptr };

constexpr std::string_view g_ConfidPath{ "app-config.toml" };
const Mikoto::BaseConfiguration g_Config{ g_ConfidPath };

auto InitializeWindow() -> void {
    MKT_BEGIN_PROFILER_NAMED();

    using namespace Mikoto;

    // Initialize the window service so we can use windows
    if (!Root::RegisterService<WindowsService>( WindowsServiceCreateInfo{} )) {
        return;
    }

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

    g_Window = WindowsService::Get()->Create( properties );
}

auto InitializeApplication() -> void {
    using namespace Mikoto;

    MKT_BEGIN_PROFILER_NAMED();

    if (!g_Window) {
        return;
    }

    g_Application = new EditorApp{};

    try {

        g_Application->SetWindow( g_Window );
        g_Application->Init();

        g_Application->PushLayer<EditorLayer>(g_Window );

    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Initializing application - Exception: e.what(): {}", e.what() );
    }
}

auto RunCleanup() -> void {
    MKT_BEGIN_PROFILER_NAMED();

    if (g_Application) {
        g_Application->Shutdown();
    }

    delete g_Application;
}

auto RunApplication() -> void {
    MKT_BEGIN_PROFILER_NAMED();

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
    MKT_BEGIN_PROFILER_NAMED();

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