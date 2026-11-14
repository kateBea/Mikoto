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
#include <limits>
#include <exception>
#include <stdexcept>
#include <filesystem>
#include <system_error>

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/numeric_limits.h>

#include <Core/CVar.hh>
#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/CLAParser.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>

#include <Platform/Window.hh>
#include <Platform/WindowsService.hh>
#include <Platform/Windows/Windows.hh>

#include <Assets/Utility.hh>
#include <Assets/ImageProcessing.hh>

#include <Application/EditorApp.hh>
#include <Application/Configuration.hh>

#include <Layers/EditorLayer.hh>
#include <Layers/TextDebugLayer.hh>
#include <Layers/EditorDebugLayer.hh>
#include <Layers/EditorRayTraceLayer.hh>
#include <Layers/EditorHelloCubeLayer.hh>
#include <Layers/EditorHelloTriangleLayer.hh>
#include <Layers/EditorPathTraceDebugModule.hh>
#include <Layers/EditorTextureCompressionLayer.hh>

#if MIKOTO_MODULE_NETWORKING
#include <Layers/NetworkDebugLayer.hh>
#endif

#include <Renderer/Rhi/Types.hh>

using namespace mikoto::core;
using namespace mikoto::editor;
using namespace mikoto::platform;
using namespace mikoto::filesystem;
using namespace mikoto::renderer;

mikoto::editor::EditorApp* gApplication{ nullptr };

mikoto::platform::IWindow* gWindow{ nullptr };
mikoto::platform::WindowsService* gWindowsService{ nullptr };

constexpr eastl::string_view kConfigPath{ "app-config.toml" };
mikoto::editor::BaseConfiguration gConfiguration{};

MKT_NODISCARD static auto GetWindowDimension( const i32 value, const i32 fallback ) -> i32 {
    MKT_BEGIN_PROFILER();
    return value > 0 ? value : fallback;
}

auto InitCVariables() -> void {
    MKT_BEGIN_PROFILER();

    MKT_ASSERT( gWindow != nullptr, "Window must be initialized before CVar registration." );

    // Load Engine configuration variables into the CVar registry for runtime access.
    mikoto::core::CVarRegistry::Get()->Register<eastl::string>(
        "application.title",
        "Window title.",
        gConfiguration.Get<eastl::string>( "application.title", "Mikoto - Engine" ),
        mikoto::core::CVarFlags::eArchive );

    mikoto::core::CVarRegistry::Get()->Register<eastl::string>(
        "engine.version",
        "Engine version from the application configuration.",
        mikoto::string::Format(
            "{}.{}.{}",
            gConfiguration.Get<mikoto::core::i64>( "engine.version_major" ),
            gConfiguration.Get<mikoto::core::i64>( "engine.version_minor" ),
            gConfiguration.Get<mikoto::core::i64>( "engine.version_patch" ) ),
        mikoto::core::CVarFlags::eReadOnly );

    mikoto::core::CVarRegistry::Get()->Register<mikoto::core::i32>(
        "application.width",
        "Initial window width in pixels.",
        gWindow->GetWidth(),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly );

    mikoto::core::CVarRegistry::Get()->Register<mikoto::core::i32>(
        "application.height",
        "Initial window height in pixels.",
        gWindow->GetHeight(),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly );

    mikoto::core::CVarRegistry::Get()->Register<bool>(
        "application.resizable",
        "Whether the initial window is resizable.",
        gConfiguration.Get<bool>( "application.resizable", true ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly );

    mikoto::core::CVarRegistry::Get()->Register<eastl::string>(
        "renderer.api",
        "Renderer backend.",
        gConfiguration.Get<eastl::string>( "renderer.api", "vulkan" ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    mikoto::core::CVarRegistry::Get()->Register<bool>(
        "renderer.gpu_validation",
        "Enable expensive GPU-based validation in Debug builds.",
        gConfiguration.Get<bool>( "renderer.gpu_validation", false ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    mikoto::core::CVarRegistry::Get()->Register<eastl::string>(
        "language_iso",
        "Engine language specification.",
        gConfiguration.Get<eastl::string>( "engine.language", "en-us" ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    // Device configurations
    mikoto::core::CVarRegistry::Get()->Register<eastl::string>(
        "device_type",
        "Type of adapter.",
        gConfiguration.Get<eastl::string>( "renderer.device_type", "Invalid" ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    mikoto::core::CVarRegistry::Get()->Register<bool>(
        "device_feature_hardware_raytracing",
        "Request hardware raytracing support.",
        gConfiguration.Get<bool>( "renderer.device_feature_hardware_raytracing", false ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    mikoto::core::CVarRegistry::Get()->Register<bool>(
        "device_feature_texture_compression",
        "Request support for texture compression.",
        gConfiguration.Get<bool>( "renderer.device_feature_texture_compression", false ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    mikoto::core::CVarRegistry::Get()->Register<bool>(
        "device_feature_hardware_wireframe",
        "Request support hardware wireframe.",
        gConfiguration.Get<bool>( "renderer.device_feature_hardware_wireframe", false ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    mikoto::core::CVarRegistry::Get()->Register<bool>(
        "device_feature_presentation_support",
        "Request support for window presentation.",
        gConfiguration.Get<bool>( "renderer.device_feature_presentation_support", false ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    mikoto::core::CVarRegistry::Get()->Register<bool>(
        "device_feature_video_encoding",
        "Request support for video encoding.",
        gConfiguration.Get<bool>( "renderer.device_feature_video_encoding", false ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );

    mikoto::core::CVarRegistry::Get()->Register<bool>(
        "device_feature_video_decoding",
        "Request support for video decoding.",
        gConfiguration.Get<bool>( "renderer.device_feature_video_decoding", false ),
        mikoto::core::CVarFlags::eArchive | mikoto::core::CVarFlags::eStartupOnly | mikoto::core::CVarFlags::eRestartRequired );
}

auto InitWindow() -> bool {
    MKT_BEGIN_PROFILER();

    if ( !gConfiguration.IsLoaded() ) {
        MKT_CORE_LOGGER_ERROR( "Could not load configuration at '{}'.", kConfigPath );
        return false;
    }
    
    // Initialize the Window service first
    gWindowsService = new ( std::nothrow ) mikoto::platform::WindowsService{ mikoto::platform::WindowsServiceCreateInfo{} };
    gWindowsService->Initialize();

    if ( !gWindowsService->IsInitialized() ) {
        return false;
    }

    // Load main window properties.
    const mikoto::core::i64 configuredWidth{ gConfiguration.Get<mikoto::core::i64>( "application.width" ) };
    const mikoto::core::i64 configuredHeight{ gConfiguration.Get<mikoto::core::i64>( "application.height" ) };
    const mikoto::core::i32 defaultWidth{ configuredWidth > 0 && configuredWidth <= eastl::numeric_limits<mikoto::core::i32>::max() ? mikoto::core::as<mikoto::core::i32>( configuredWidth ) : 1280 };
    const mikoto::core::i32 defaultHeight{ configuredHeight > 0 && configuredHeight <= eastl::numeric_limits<mikoto::core::i32>::max() ? mikoto::core::as<mikoto::core::i32>( configuredHeight ) : 720 };
    
    // Create the main window
    const mikoto::platform::WindowProperties properties{
        .mTitle = gConfiguration.Get<eastl::string>( "application.title", "Mikoto - Engine" ),
        .mWidth = GetWindowDimension( gConfiguration.Get<mikoto::core::i32>( "application.width", defaultWidth ), defaultWidth ),
        .mHeight = GetWindowDimension( gConfiguration.Get<mikoto::core::i32>( "application.height", defaultHeight ), defaultHeight ),
        .mBackend = mikoto::renderer::rhi::InferAPI( gConfiguration.Get<eastl::string>( "renderer.api" ) ),
        .mResizable = gConfiguration.Get<bool>( "application.resizable" ),
        .mCustomTitleBar = true
    };
    gWindow = gWindowsService->Create( properties );

    return true;
}

template<typename DebugClass, typename... Args>
auto RegisterDebugLayer( const char* configName, Args&&... args ) -> void {
    if ( gConfiguration.Get<bool>( configName, false ) ) {
        gApplication->PushLayer<DebugClass>( eastl::forward<Args>( args )... );
    }
}

auto InitEditor() -> bool {
    MKT_BEGIN_PROFILER();

    if ( !gWindow ) {
        return false;
    }

    gApplication = new ( std::nothrow ) mikoto::editor::EditorApp{ gWindow };

    try {
        MKT_FILE_LOGGER_INFO( "Startup working directory: {}", mikoto::filesystem::GetProcessPath().GetC_Str() );
        const mikoto::core::i64 compressionWorkers{ gConfiguration.Get<mikoto::core::i64>( "assets.texture_compression_workers", 0 ) };

        if ( compressionWorkers < 0 || compressionWorkers > eastl::numeric_limits<mikoto::core::u32>::max() ) {
            MKT_THROW_INVALID_ARGUMENT( "assets.texture_compression_workers must be a nonnegative 32-bit integer." );
        }

        mikoto::asset::SetTextureCompressionWorkerCount( mikoto::core::as<mikoto::core::u32>( compressionWorkers ) );
        MKT_CORE_LOGGER_INFO( "Texture compression workers: {}", mikoto::asset::GetTextureCompressionWorkerCount() );

        gApplication->Init();

        // You can enable these from the configuration file to test some features standalone. See app-config.toml for examples.
        RegisterDebugLayer<mikoto::editor::EditorRayTraceLayer>( "debug.ray_tracing_showcase", gWindow );
        RegisterDebugLayer<mikoto::editor::EditorPathTraceDebugModule>( "debug.path_tracing_showcase" );

        RegisterDebugLayer<mikoto::editor::TextDebugLayer>( "debug.text_rendering" );

        RegisterDebugLayer<mikoto::editor::EditorTextureCompressionLayer>( "debug.texture_compression_benchmark" );

        RegisterDebugLayer<mikoto::editor::EditorDebugLayer>( "debug.bindless_showcase", gWindow );
        RegisterDebugLayer<mikoto::editor::EditorHelloTriangleLayer>( "debug.simple_raster_triangle", gWindow );
        RegisterDebugLayer<mikoto::editor::EditorHelloCubeLayer>( "debug.simple_raster_cube", gWindow );

#if MIKOTO_MODULE_NETWORKING
        RegisterDebugLayer<mikoto::editor::NetworkDebugLayer>( "debug.simple_networking" );
#endif
        // Editor layer always gets pushed
        gApplication->PushLayer<mikoto::editor::EditorLayer>( gWindow );

    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Startup App Exception - e.what(): {}", e.what() );

        // With _HAS_EXCEPTIONS=0, MSVC's system_error::what() is only
        // "system error". I preserve the actionable OS/library error separately.
        if ( const auto* error{ dynamic_cast<const std::system_error*>( &e ) } ) {
            MKT_FILE_LOGGER_ERROR( "System error: category={}, code={}, message={}",
                error->code().category().name(), error->code().value(), error->code().message() );
        }

        MKT_FILE_LOGGER_ERROR( "StackTrace\n{}", mikoto::logging::stacktrace::ToString() );

        return false;
    }

    return true;
}

auto Cleanup() -> void {
    MKT_BEGIN_PROFILER();

    // Cleanup app
    if ( gApplication ) {
        gApplication->Shutdown();
        delete gApplication;
    }

    // Cleanup window service
    if ( gWindowsService ) {
        gWindowsService->Shutdown();
        delete gWindowsService;
    }
}

auto Run() -> void {
    MKT_BEGIN_PROFILER();

    if ( !gApplication || !gWindow ) {
        return;
    }

    try {
        gApplication->Run();
    } catch ( const std::exception& e ) {
        MKT_CORE_LOGGER_ERROR( "Run App Exception: e.what(): {}", e.what() );
        MKT_FILE_LOGGER_ERROR( "Run App Exception: {}", e.what() );
    }
}

auto RunEditor( const int argc, char** argv ) -> int {
    MKT_BEGIN_PROFILER();

    mikoto::core::CLAParser parser{ "MikotoEditor" };

    if ( !parser.Parse( argc, argv ) ) {
        MKT_CORE_LOGGER_INFO( "{}", parser.GetMessageOutput() );
        return parser.IsHelpRequested() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    for ( const eastl::string& assignment: parser.GetCVarOverrides() ) {
        if ( !mikoto::core::CVarRegistry::Get()->SetFromAssignment( assignment ) ) {
            MKT_CORE_LOGGER_ERROR( "Invalid CVar override '{}'. Expected --set name=value.", assignment );
            return EXIT_FAILURE;
        }
    }

    try {
        gConfiguration.Load( mikoto::filesystem::Path{ kConfigPath } );
    } catch ( const std::exception& error ) {
        MKT_CORE_LOGGER_ERROR( "{}", error.what() );
        return EXIT_FAILURE;
    }

    if ( !InitWindow() ) {
        return EXIT_FAILURE;
    }

    InitCVariables();

    if ( !InitEditor() ) {
        Cleanup();
        return EXIT_FAILURE;
    }

    Run();
    Cleanup();

    return EXIT_SUCCESS;
}

#if defined( _WIN32 ) && MIKOTO_EDITOR_USE_WINMAIN
// Disabled by default; enable the matching CMake option to restore GUI startup.
auto WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int ) -> int {
    MKT_BEGIN_PROFILER();
    return RunEditor( __argc, __argv );
}
#else
auto main( const int argc, char** argv ) -> int {
    MKT_BEGIN_PROFILER();
    return RunEditor( argc, argv );
}
#endif
