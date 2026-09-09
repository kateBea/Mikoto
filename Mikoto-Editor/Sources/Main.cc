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
#include <exception>
#include <limits>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/CLAParser.hh>
#include <Core/CVar.hh>

#include <Logging/Logger.hh>

#include <Application/Configuration.hh>
#include <Application/EditorApp.hh>


#include <Layers/EditorLayer.hh>
#include <Layers/EditorDebugLayer.hh>
#include <Layers/EditorDebugLayer.hh>
#include <Layers/EditorRayTraceLayer.hh>
#include <Layers/EditorHelloCubeLayer.hh>
#include <Layers/EditorHelloTriangleLayer.hh>
#include <Layers/NetworkDebugLayer.hh>

#include <Platform/Window.hh>
#include <Platform/WindowsService.hh>

#if MIKOTO_PLATFORM_WINDOWS
    #include <windows.h>
#endif

using namespace mikoto::core;
using namespace mikoto::editor;
using namespace mikoto::platform;

EditorApp* gApplication{ nullptr };

Window* gWindow{ nullptr };
WindowsService* gWindowsService{ nullptr };

constexpr eastl::string_view kConfigPath{ "app-config.toml" };
const BaseConfiguration gConfiguration{ kConfigPath };

namespace {
    auto GetWindowDimension( const i32 value, const i32 fallback ) -> i32 {
        return value > 0 ? value : fallback;
    }
}

auto InitWindow() -> bool {
    if (!gConfiguration.IsLoaded()) {
        MKT_CORE_LOGGER_ERROR( "Could not load file at %s·", kConfigPath.data() );
        return false;
    }

    // Register configuration defaults only after command-line parsing. Deferred
    // --set overrides are then applied by CVarRegistry during registration.
    auto& cVars{ CVarRegistry::Get() };

    const i64 configuredWidth{ gConfiguration.Get<i64>( "application.width" ) };
    const i64 configuredHeight{ gConfiguration.Get<i64>( "application.height" ) };
    const i32 defaultWidth{ configuredWidth > 0 && configuredWidth <= std::numeric_limits<i32>::max() ? as<i32>( configuredWidth ) : 1280 };
    const i32 defaultHeight{ configuredHeight > 0 && configuredHeight <= std::numeric_limits<i32>::max() ? as<i32>( configuredHeight ) : 720 };

    cVars.Register<eastl::string>( "application.title", "Window title.", gConfiguration.Get<eastl::string>( "application.title" ), CVarFlags::eArchive );
    cVars.Register<i32>( "application.width", "Initial window width in pixels.", defaultWidth, CVarFlags::eArchive | CVarFlags::eStartupOnly );
    cVars.Register<i32>( "application.height", "Initial window height in pixels.", defaultHeight, CVarFlags::eArchive | CVarFlags::eStartupOnly );
    cVars.Register<bool>( "application.resizable", "Whether the initial window is resizable.", gConfiguration.Get<bool>( "application.resizable" ), CVarFlags::eArchive | CVarFlags::eStartupOnly );
    cVars.Register<eastl::string>( "renderer.api", "Renderer backend selected at startup.", gConfiguration.Get<eastl::string>( "renderer.api" ), CVarFlags::eArchive | CVarFlags::eStartupOnly | CVarFlags::eRestartRequired );

    gWindowsService = new (std::nothrow) WindowsService{ WindowsServiceCreateInfo{} };
    gWindowsService->Initialize();

    const WindowProperties properties{
        .mTitle = cVars.GetValue<eastl::string>( "application.title" ),
        .mWidth = GetWindowDimension( cVars.GetValue<i32>( "application.width", defaultWidth ), defaultWidth ),
        .mHeight = GetWindowDimension( cVars.GetValue<i32>( "application.height", defaultHeight ), defaultHeight ),
        .mBackend = InferAPI( cVars.GetValue<eastl::string>( "renderer.api" ) ),
        .mResizable = cVars.GetValue<bool>( "application.resizable" ) };
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

        // gApplication->PushLayer<EditorDebugLayer>( gWindow );
        // gApplication->PushLayer<EditorRayTraceLayer>( gWindow );
        // gApplication->PushLayer<EditorHelloTriangleLayer>( gWindow );
        gApplication->PushLayer<EditorHelloCubeLayer>( gWindow );

        gApplication->PushLayer<EditorLayer>( gWindow );
        gApplication->PushLayer<NetworkDebugLayer>();
    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Init App exception - e.what(): {}", e.what() );

        MKT_FILE_LOGGER_ERROR( "Exception\n{}", e.what() );
        MKT_FILE_LOGGER_ERROR( "StackTrace\n{}", stacktrace::ToString() );
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

auto RunEditor( const int argc, char** argv ) -> int {
    MKT_BEGIN_PROFILER_NAMED();

    if ( argc == 0 || argv == nullptr ) {
        MKT_CORE_LOGGER_ERROR( "The process command line was unavailable." );
        return EXIT_FAILURE;
    }

    CLAParser parser{};
    if ( !parser.Parse( argc, argv ) ) {
        MKT_CORE_LOGGER_ERROR( "{}", parser.GetMessageOutput() );
        return parser.IsHelpRequested() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    for ( const eastl::string& assignment : parser.GetCVarOverrides() ) {
        if ( !CVarRegistry::Get().SetFromAssignment( assignment ) ) {
            MKT_CORE_LOGGER_ERROR( "Invalid CVar override '{}'. Expected --set name=value.", assignment );
            return EXIT_FAILURE;
        }
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

#if defined( _WIN32 )
// https://stackoverflow.com/questions/13871617/winmain-and-main-in-c-extended
auto WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int ) -> int {
    return RunEditor( __argc, __argv );
}
#else
auto main( const int argc, char** argv ) -> int {
    return RunEditor( argc, argv );
}
#endif
