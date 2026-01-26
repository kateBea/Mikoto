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


#include <exception>

#include <Core/Root.hh>
#include <Core/Profiler.hh>

#include <Platform/Window.hh>
#include <Platform/WindowsService.hh>

// Globals
Mikoto::Window* g_Window{};

auto InitializeWindow() -> void {
    MKT_BEGIN_PROFILER_NAMED();

    using namespace Mikoto;

    // Initialize the window service so we can use Windows
    if (!Root::RegisterService<WindowsService>( WindowsServiceCreateInfo{} )) {
        return;
    }

    WindowProperties properties{
        .Title{ "Hello World" },
        .Width{ 1280 },
        .Height{ 720 },
        .Backend{ GraphicsAPI::DIRECTX_11 },
    };

    g_Window = WindowsService::Get()->Create( properties );
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