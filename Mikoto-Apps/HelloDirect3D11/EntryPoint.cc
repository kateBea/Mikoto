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

#include <Core/Core.hh>
#include <Core/EventService.hh>

#include <Logging/Logger.hh>

#include <Platform/Window.hh>
#include <Platform/WindowsService.hh>

#include <Renderer/Core/RenderService.hh>

Mikoto::Window* g_Window{ nullptr };

auto InitializeWindow() -> void {
    using namespace Mikoto;

    Root::Register<WindowsService>( WindowsServiceCreateInfo{} );

    WindowProperties properties{
        .Title{ "Hello Application" },
        .Width{ 1280 },
        .Height{ 720 },
        .Backend{ GraphicsAPI::DIRECTX_11 },
        .Resizable{ true }
    };

    g_Window = WindowsService::Get()->Create( properties );

    Root::Register<InputService>( InputServiceCreateInfo{ .MainWindow{ g_Window } } );
}

auto InitializeApplication() -> void {
    using namespace Mikoto;

    if (!g_Window) {
        return;
    }

    try {
        Root::RegisterDeferred<EventService>(EventServiceCreateInfo{});
        Root::RegisterDeferred<RenderService>( RenderServiceCreateInfo{
            .TargetWindow{ g_Window },
            .RendererAPI{ g_Window->GetApi() },
            .EnableImGui{ false }
        } );
        Root::Init( RootConfig{ .EnableCoreServices{ true } } );

    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Initializing application - Exception: e.what(): {}", e.what() );
    }
}

auto RunCleanup() -> void {
    using namespace Mikoto;

    Root::Shutdown();
}

auto RunApplication() -> void {
    using namespace Mikoto;

    try {

        while (!g_Window->ShouldClose()) {
            TimeService::Get()->UpdateTimeStep();

            if ( !g_Window->IsMinimized() ) {
                const double timeStep{ TimeService::Get()->GetTimeStep( TimeUnit::SECONDS ) };

                Root::UpdateState( static_cast<float>( timeStep ) );
            }
        }

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