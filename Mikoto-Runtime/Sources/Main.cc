#include <fstream>
#include <charconv>
#include <iostream>
#include <filesystem>

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Platform/WindowsService.hh>
#include <Platform/Windows/Windows.hh>

#include <Application/RuntimeApp.hh>
#include <Application/PackageGame.hh>


namespace fs = std::filesystem;
using namespace mikoto;
using namespace mikoto::core;

struct StartupOptions {
    fs::path mProject{};
    fs::path mPackageDirectory{};
    fs::path mResources{};
    renderer::rhi::GraphicsAPI mApi{ renderer::rhi::GraphicsAPI::eVulkan };
    mikoto::core::i32 mWidth{ 1280 };
    mikoto::core::i32 mHeight{ 720 };
    bool mFullscreen{};
    bool mShowHelp{};
};

MKT_NODISCARD static auto ExecutablePath( mikoto::core::cstr argument ) -> fs::path {
#if defined( MIKOTO_PLATFORM_WINDOWS )
    eastl::wstring path( 32768, L'\0' );
    const auto length{ GetModuleFileNameW( nullptr, path.data(), static_cast<DWORD>( path.size() ) ) };

    if ( length && length < path.size() ) {
        path.resize( length );
        return fs::path{ path.c_str() };
    }
#elif defined( MIKOTO_PLATFORM_LINUX )
    std::error_code error{};
    const auto path{ fs::read_symlink( "/proc/self/exe", error ) };

    if ( !error ) {
        return path;
    }
#endif
    return fs::absolute( fs::u8path( argument ) );
}

MKT_NODISCARD static auto ReadDimension( eastl::string_view text ) -> mikoto::core::i32 {
    constexpr mikoto::core::i32 kMaximumDimension{ 16384 };
    mikoto::core::i32 dimension{};
    const auto [end, error]{ std::from_chars( text.data(), text.data() + text.size(), dimension ) };

    if ( error != std::errc{} || end != text.data() + text.size() || dimension < 1 || dimension > kMaximumDimension ) {
        MKT_THROW_RUNTIME_ERROR( "Window dimensions must be between 1 and 16384." );
    }

    return dimension;
}

MKT_NODISCARD static auto ParseOptions( mikoto::core::i32 argc, char** argv, const fs::path& executable ) -> StartupOptions {
    StartupOptions options{};
    options.mResources = executable.parent_path() / "Resources";

    for ( mikoto::core::i32 index{ 1 }; index < argc; ++index ) {
        const eastl::string_view option{ argv[index] };
        const auto value{ [&]() -> eastl::string_view {
            if ( ++index >= argc ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Missing value for {}", option ) );
            }

            return argv[index];
        } };

        if ( option == "--help" || option == "-h" ) {
            options.mShowHelp = true;
            return options;
        }

        if ( option == "--project" || option == "--package" || option == "--resources" ) {
            const auto text{ value() };
            const auto path{ fs::absolute( fs::u8path( text.begin(), text.end() ) ) };

            if ( option == "--project" ) {
                options.mProject = path;
            }
            else if ( option == "--package" ) {
                options.mPackageDirectory = path;
            }
            else {
                options.mResources = path;
            }
        }
        else if ( option == "--api" ) {
            const auto api{ value() };

            if ( api == "vulkan" ) {
                options.mApi = renderer::rhi::GraphicsAPI::eVulkan;
            }
            else if ( api == "d3d12" ) {
                options.mApi = renderer::rhi::GraphicsAPI::eD3D12;
            }
            else {
                MKT_THROW_RUNTIME_ERROR( "Supported renderer APIs: vulkan, d3d12." );
            }
        }
        else if ( option == "--width" ) {
            options.mWidth = ReadDimension( value() );
        }
        else if ( option == "--height" ) {
            options.mHeight = ReadDimension( value() );
        }
        else if ( option == "--fullscreen" ) {
            options.mFullscreen = true;
        }
        else {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Unknown option: {}", option ) );
        }
    }

#if !defined( MIKOTO_PLATFORM_WINDOWS )
    if ( options.mApi == renderer::rhi::GraphicsAPI::eD3D12 ) {
        MKT_THROW_RUNTIME_ERROR( "D3D12 requires Windows." );
    }
#endif
    return options;
}

MKT_NODISCARD static auto ReadManifestProject( const fs::path& executable ) -> fs::path {
    std::ifstream input{ executable.parent_path() / "game.yaml" };

    if ( !input ) {
        MKT_THROW_RUNTIME_ERROR( "Pass --project FILE, or place a packaged game.yaml beside the executable." );
    }

    const auto document{ YAML::Load( input ) };

    if ( !document.IsMap() ) {
        MKT_THROW_RUNTIME_ERROR( "game.yaml must contain a Project path." );
    }

    const auto project{ document["Project"] };

    if ( !project.IsScalar() || project.Scalar().empty() ) {
        MKT_THROW_RUNTIME_ERROR( "game.yaml must contain a non-empty Project path." );
    }

    return executable.parent_path() / fs::u8path( project.Scalar() );
}

MKT_NODISCARD static auto CreateGameWindow( platform::WindowsService& windows, const project::Project& project,
    const StartupOptions& options ) -> platform::IWindow* {
    auto* window{ windows.Create( platform::WindowProperties{
        .mTitle = eastl::string{ project.GetName() },
            .mWidth = options.mWidth,
                .mHeight = options.mHeight,
                    .mBackend = options.mApi,
                        .mResizable = true,
                            .mCustomTitleBar = false } ) };

    if ( !window ) {
        MKT_THROW_RUNTIME_ERROR( "Could not create the game window." );
    }

    if ( options.mFullscreen ) {
        window->SetScreenMode( platform::ScreenMode::eFullScreen );
    }

    return window;
}


auto main( int argc, char** argv ) -> int {
    using namespace mikoto;
    using namespace mikoto::core;

    // Keep borrowed project/window state alive through exceptional shutdown too.
    project::Project project{ "Game" };
    eastl::unique_ptr<platform::WindowsService> windows{};
    eastl::unique_ptr<runtime::RuntimeApp> app{};

    const auto cleanup{ [&] {
        if ( app ) {
            app->Shutdown();
            app.reset();
        }

        if ( windows ) {
            windows->Shutdown();
            windows.reset();
        }
    } };

    try {
        const auto executable{ ExecutablePath( argv[0] ) };
        auto options{ ParseOptions( argc, argv, executable ) };

        if ( options.mShowHelp ) {
            std::cout << "MikotoRuntime [--project FILE] [--api vulkan|d3d12] [--width N] [--height N] [--fullscreen]\n"
                         "              [--resources DIRECTORY] [--package NEW_DIRECTORY]\n"
                         "Without --project, loads game.yaml beside the executable. --package creates a distributable game folder.\n";
            return 0;
        }

        if ( options.mProject.empty() ) {
            options.mProject = ReadManifestProject( executable );
        }

        project.Deserialize( filesystem::Path{ options.mProject } );

        if ( !fs::is_directory( options.mResources / "Shaders" ) ) {
            MKT_THROW_RUNTIME_ERROR( "Engine Resources/Shaders directory is missing. Build the runtime staging target or use --resources." );
        }

        if ( !options.mPackageDirectory.empty() ) {
            runtime::PackageGame( project, executable, options.mResources, options.mPackageDirectory );
            std::cout << "Game packaged at " << options.mPackageDirectory << '\n';
            return 0;
        }

        // Engine resources use Resources/ relative to the working directory.
        // Game asset references are resolved relative to their scene files.
        if ( options.mResources.filename() != "Resources" ) {
            MKT_THROW_RUNTIME_ERROR( "The resource directory must be named Resources." );
        }

        fs::current_path( options.mResources.parent_path() );

        windows = eastl::make_unique<platform::WindowsService>( platform::WindowsServiceCreateInfo{} );
        windows->Initialize();
        auto* window{ CreateGameWindow( *windows, project, options ) };

        app = eastl::make_unique<runtime::RuntimeApp>( window, project );
        app->Init();
        app->Run();
        cleanup();
        return 0;
    } catch ( const std::exception& error ) {
        std::cerr << "MikotoRuntime: " << error.what() << '\n';

        try {
            cleanup();
        } catch ( const std::exception& shutdownError ) {
            std::cerr << "Shutdown: " << shutdownError.what() << '\n';
        }

        return 1;
    }
}
