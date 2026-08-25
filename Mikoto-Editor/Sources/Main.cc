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

#include <new>
#include <cstdlib>
#include <exception>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Application/Configuration.hh>
#include <Application/EditorApp.hh>


#include <Layers/EditorLayer.hh>
#include <Layers/EditorDebugLayer.hh>
#include <Layers/EditorDebugLayer.hh>
#include <Layers/EditorRayTraceLayer.hh>
#include <Layers/EditorHelloCubeLayer.hh>
#include <Layers/EditorHelloTriangleLayer.hh>

#include <Platform/Window.hh>
#include <Platform/WindowsService.hh>

using namespace mikoto::core;
using namespace mikoto::editor;
using namespace mikoto::platform;

EditorApp* gApplication{ nullptr };

Window* gWindow{ nullptr };
WindowsService* gWindowsService{ nullptr };

constexpr eastl::string_view kConfigPath{ "app-config.toml" };
const BaseConfiguration gConfiguration{ kConfigPath };

auto InitWindow() -> bool {
    if (!gConfiguration.IsLoaded()) {
        MKT_CORE_LOGGER_ERROR( "Could not load file at %s·", kConfigPath.data() );
        return false;
    }

    // Initialize the window service
    gWindowsService = new (std::nothrow) WindowsService{ WindowsServiceCreateInfo{} };
    gWindowsService->Initialize();

    const WindowProperties properties{
        .mTitle = gConfiguration.Get<eastl::string>( "application.title" ),
        .mWidth = as<i32>( gConfiguration.Get<i64>( "application.width" ) ),
        .mHeight = as<i32>( gConfiguration.Get<i64>( "application.height" )),
        .mBackend = InferAPI( gConfiguration.Get<eastl::string>( "renderer.api" ) ),
        .mResizable = gConfiguration.Get<bool>( "application.resizable" ) };
    gWindow = gWindowsService->Create( properties );

    return true;
}

auto InitEditor() -> bool {
    MKT_BEGIN_PROFILER_NAMED();

    if (!gWindow) {
        return false;
    }

    gApplication = new (std::nothrow) EditorApp{ gWindow };

    try {
        gApplication->Init();

#if true
        //gApplication->PushLayer<EditorDebugLayer>( gWindow );
        //gApplication->PushLayer<EditorRayTraceLayer>( gWindow );
        gApplication->PushLayer<EditorHelloTriangleLayer>( gWindow );
        //gApplication->PushLayer<EditorHelloCubeLayer>( gWindow );
#endif
        //gApplication->PushLayer<EditorLayer>( gWindow );
    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Init App exception - e.what(): {}", e.what() );
        return false;
    }

    return true;
}

auto Cleanup() -> void {
    using namespace mikoto;

    MKT_BEGIN_PROFILER_NAMED();

    // Cleanup app
    if (gApplication) {
        gApplication->Shutdown();
        delete gApplication;
    }

    // Cleanup window service
    if (gWindowsService) {
        gWindowsService->Shutdown();
        delete gWindowsService;
    }
}

auto Run() -> void {
    MKT_BEGIN_PROFILER_NAMED();

    if (!gApplication || !gWindow) {
        return;
    }

    try {
        gApplication->Run();

    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Run App Exception: e.what(): {}", e.what() );
    }
}

// TODO: Add control to for entry point according to platform
// WinMain for windows graphics apps...
auto main( const int argc, char** ) -> int {
    MKT_BEGIN_PROFILER_NAMED();

    if ( argc != 1 ) {
        MKT_CORE_LOGGER_ERROR( "Application expects no arguments." );
        return EXIT_FAILURE;
    }

    if (!InitWindow()) {
        return EXIT_FAILURE;
    }

    if (!InitEditor()) {
        Cleanup();
        return EXIT_FAILURE;
    }

    Run();
    Cleanup();

    return EXIT_SUCCESS;
}