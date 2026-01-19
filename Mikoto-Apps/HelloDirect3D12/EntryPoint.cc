//    Copyright 2026 ケイト
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

#include <Logging/Logger.hh>

#include <Platform/Window.hh>

Mikoto::Window* g_Window{ nullptr };

auto InitializeWindow() -> void {
    using namespace Mikoto;

    WindowProperties properties{
        .Title{ "Hello Application - Direct3D12" },
        .Width{ 1280 },
        .Height{ 720 },
        .Backend{ GraphicsAPI::DIRECTX_12 },
        .Resizable{ true }
    };

    g_Window = Window::Create( properties );

    g_Window->Init();
}

auto InitializeApplication() -> void {
    using namespace Mikoto;

    if (!g_Window) {
        return;
    }

    try {

    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Initializing application - Exception: e.what(): {}", e.what() );
    }
}

auto RunCleanup() -> void {
    if (g_Window) {
        g_Window->Shutdown();
    }

    delete g_Window;
}

auto RunApplication() -> void {
    try {

    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Running application - Exception: e.what(): {}", e.what() );
    }
}


auto Usage(const int argc)-> bool {
    if ( argc != 1 ) {
        std::printf( "Application takes no arguments." );
        return false;
    }

    return true;
}

auto main( const int argc, char** ) -> int {
    if (!Usage( argc )) {
        return 1;
    }

    InitializeWindow();
    InitializeApplication();
    RunApplication();
    RunCleanup();

    return 0;
}