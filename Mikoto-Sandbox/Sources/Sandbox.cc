/**
 * EditorApp.cc
 * Created by kate on 6/7/23.
 * */

// C++ Standard Library
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

// Project headers
#include <Assets/AssetsService.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioService.hh>
#include <Core/Configuration.hh>
#include <Core/CoreEvents.hh>
#include <Core/InputService.hh>
#include <Core/Root.hh>
#include <Core/TimeService.hh>
#include <Core/Timer.hh>
#include <Filesystem/FileService.hh>
#include <GameLayer.hh>
#include <GraphicsLayer.hh>
#include <Logging/Logger.hh>
#include <MusicPlayerLayer.hh>
#include <SandboxApp.hh>
#include <Threading/TaskManager.hh>
#include <Threading/TaskService.hh>

#include "NetworkLayer.hh"

namespace Mikoto {

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

    auto SandboxApp::Run( const Int32, char** ) -> Int32 {
        MKT_CORE_LOGGER_DEBUG( "Initializing Mikoto Sandbox..." );

        Int32 exitCode{ EXIT_SUCCESS };

        try {
            Init();

            while ( IsRunning() ) {
                Update();
            }

        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_CRITICAL( "Error occurred {}", e.what() );
            MKT_FILE_LOGGER_ERROR( "Error occurred {}", e.what() );
            exitCode = EXIT_FAILURE;
        }

        Shutdown();

        return exitCode;
    }

    auto SandboxApp::Init() -> void {
        MKT_PROFILE_SCOPE();

        // Load configuration
        BaseConfiguration configApp{ "./app-config.toml" };

        // App window
        m_Window = Window::Create( {
            .Title{ configApp.Get<std::string>( "application.title" ) },
            .Width{ static_cast<Int32>( configApp.Get<Int64>( "application.width" ) ) },
            .Height{ static_cast<Int32>( configApp.Get<Int64>( "application.height" ) ) },
            .Backend{ InferAPI( configApp.Get<std::string>( "renderer.api" ) ) },
            .Resizable{ configApp.Get<bool>( "application.resizable" ) } } );

        if ( m_Window ) {
            m_Window->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR( "Failed to create main application window!" );
        }

        // Configure and initialize the engine
        const RootConfig config{
            .LockFrameRate{ configApp.Get<bool>( "renderer.vsync", false ) },
            .TargetWindow{ m_Window.get() }
        };

        Root::Init( config );

        SetupEventCallbacks();

        m_LayerStack.PushLayer<GameLayer>( "Game - Layer" );
        m_LayerStack.PushLayer<MusicPlayerLayer>( "MusicPlayer - Layer" );
        m_LayerStack.PushLayer<GraphicsLayer>( "Graphics - Layer", m_Window.get() );
        m_LayerStack.PushLayer<NetworkLayer>( "Network - Layer" );
    }

    auto SandboxApp::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG( "Shutting down Mikoto Sandbox..." );

        m_LayerStack.Shutdown();

        Root::Shutdown();
    }

    auto SandboxApp::Update() -> void {

        TimeService::Get()->Update();
        const double timeStep{ TimeService::Get()->GetTimeStep( TimeUnit::SECONDS ) };

        if ( !m_Window->IsMinimized() ) {
            Root::StartFrame();

            m_LayerStack.OnUpdate( static_cast<float>( timeStep ) );
            Root::UpdateState( static_cast<float>( timeStep ) );

            Root::EndFrame();
        }
    }

    auto SandboxApp::SetupEventCallbacks() -> void {

        AddHandler( EventType::WINDOW_CLOSE_EVENT,
                    [this]( Event &event ) -> bool {
                        m_State = ApplicationStatus::STOPPED;
                        event.SetHandled( true );
                        MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Handled Window Event close" );
                        return true;
                    } );

        AddHandler( EventType::KEY_PRESSED_EVENT,
                    [this]( Event &event ) -> bool {
                        const auto &keyEvent{ dynamic_cast<KeyPressedEvent &>( event ) };
                        TaskService::Get()->Submit( [pressedKey = keyEvent.GetKeyCode()]() -> void {
                                MKT_CORE_LOGGER_INFO( "Key pressed: {}", GetStringRepresentation( static_cast<KeyCode>( pressedKey ) ) );
                        });

                        return true;
                    } );

        EventService::Get()->Subscribe( this );
    }
}// namespace Mikoto
