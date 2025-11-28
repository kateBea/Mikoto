/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

#include <exception>

// Project Headers
#include <Application/EditorApp.hh>
#include <Core/Configuration.hh>
#include <Core/Profiler.hh>
#include <Layers/EditorLayer.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>

class BaseConfiguration final : public Mikoto::Configuration {
public:
    explicit BaseConfiguration( const Mikoto::Path& filePath ) {
        Load( filePath );
    }

    auto Load( const Mikoto::Path& filePath ) -> void override {
        toml::parse_result result{ toml::parse_file( filePath.string() ) };

        if ( result.failed() ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to load configuration file: {}", filePath.string() ) );
        }

        m_Data.clear();

        const toml::table& tbl{ result.table() };
        for ( auto&& [sectionName, sectionValue]: tbl ) {
            if ( auto* section = sectionValue.as_table() ) {

                for ( auto&& [key, value]: *section ) {
                    std::string fullKey = fmt::format( "{}{}{}", sectionName.str(), SEPARATOR, key.str() );

                    value.visit( [&]( auto&& v ) {
                        m_Data[fullKey] = ToNativeType( v );
                    } );
                }
            }
        }

        m_IsLoaded = true;
    }

private:
    // Separator between section and key
    static constexpr std::string_view SEPARATOR{ "." };

    /**
     * Converts a TOML value to std::any
     * @param v Toml value
     * @return std::any containing the value, or null if the type is unsupported
     */
    static auto ToNativeType( const auto& v ) -> std::any {
        using namespace Mikoto;

        using VType = std::decay_t<decltype( v )>;

        if constexpr ( toml::is_boolean<VType> ) {
            return std::make_any<bool>( v );
        } else if constexpr ( toml::is_integer<VType> ) {
            return std::make_any<Int64>( v );
        } else if constexpr ( toml::is_floating_point<VType> ) {
            return std::make_any<double>( v );
        } else if constexpr ( toml::is_string<VType> ) {
            return std::make_any<std::string>( v );
        }

        return std::any{};
    }
};

auto main( const int argc, char** argv ) -> int {
    MKT_BEGIN_PROFILER_NAMED();

    using namespace Mikoto;

    if ( argc != 1 ) {
        std::printf( "No arguments required." );
        return 1;
    }

    // Load configuration
    constexpr std::string_view confidPath{ "./app-config.toml" };
    const BaseConfiguration config{ confidPath };

    if (!config.IsLoaded()) {
        std::printf( "Could not load file at %s·", confidPath.data() );
        return 1;
    }

    // App window
    WindowProperties properties{};
    properties.Resizable = config.Get<bool>( "application.resizable" );
    properties.Title = config.Get<std::string>( "application.title" );
    properties.Backend = InferAPI( config.Get<std::string>( "renderer.api" ) );
    properties.Width = static_cast<Int32>( config.Get<Int64>( "application.width" ) );
    properties.Height = static_cast<Int32>( config.Get<Int64>( "application.height" ));

    Window* window{ Window::Create( properties ) };

    if ( window ) {
        window->Init();
    } else {
        MKT_THROW_RUNTIME_ERROR( "Failed to create main application window!" );
    }

    const auto app{ new EditorApp{} };

    app->SetWindow( window );

    Int32 ret{ EXIT_SUCCESS };

    try {
        app->Init();

        // Register the editor
        EditorLayerCreateInfo spec{
            .Name{ "Editor Layer" },
            .TargetWindow{ window },
            .ModelsRootDirectory{ config.Get<std::string>( "assets.path" ) }
        };

        app->PushLayer<EditorLayer>( spec );

        ret = app->Run( argc, argv );
    } catch ( const std::exception& e ) {
        ret = EXIT_FAILURE;
        MKT_CORE_LOGGER_ERROR( "Main - Exception: e.what(): {}", e.what() );
    }

    app->Shutdown();
    delete app;

    window->Shutdown();
    delete window;

    return ret;
}