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
#include <Filesystem/FileService.hh>
#include <GameLayer.hh>
#include <Logging/Logger.hh>
#include <SandboxApp.hh>
#include <Threading/TaskManager.hh>
#include <Threading/TaskService.hh>
#include <MusicPlayerLayer.hh>
#include <GraphicsLayer.hh>
#include <Core/Timer.hh>

namespace Mikoto {

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
    }

    auto SandboxApp::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG( "Shutting down Mikoto Sandbox..." );

        m_LayerStack.Clear();

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
