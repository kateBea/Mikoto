/**
 * EditorApp.cc
 * Created by kate on 6/7/23.
 * */

// C++ Standard Library
#include <cstdlib>
#include <stdexcept>
#include <utility>

#include <iostream>
#include <string>

// Project headers
#include <Audio/AudioDevice.hh>
#include <Core/Configuration.hh>
#include <Core/CoreEvents.hh>
#include <Core/Root.hh>
#include <Core/TimeService.hh>
#include <EditorApp.hh>
#include <Logging/Logger.hh>
#include <Layers/EditorLayer.hh>

namespace Mikoto {

    auto EditorApp::Run( const Int32, char ** ) -> Int32 {
        MKT_CORE_LOGGER_DEBUG("Initializing Mikoto Editor...");

        Int32 exitCode{ EXIT_SUCCESS };

        try {
            Init();

            while (IsRunning()) {
                Update();
            }

        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_CRITICAL("Error occurred {}", e.what());
            exitCode = EXIT_FAILURE;
        }

        Shutdown();

        return exitCode;
    }

    auto EditorApp::Init() -> void {
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
            .LockFrameRate{ false },
            .TargetWindow{ m_Window.get() }
        };

        Root::Init(config);

        SetupEventCallbacks();

        m_LayerStack.PushLayer<EditorLayer>(EditorLayerCreateInfo{
            .Name{ "Editor Layer" },
            .TargetWindow{ m_Window.get() },
            .ModelsRootDirectory{ configApp.Get<std::string>( "paths.assets" ) },
        });
    }

    auto EditorApp::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG("Shutting down Mikoto Editor...");

        Root::Shutdown();
    }

    auto EditorApp::Update() -> void {
        TimeService::Get()->Update();
        const double timeStep{ TimeService::Get().GetTimeStep( TimeUnit::SECONDS ) };

        if ( !m_Window->IsMinimized() ) {
            Root::StartFrame();

            m_LayerStack.OnUpdate( static_cast<float>( timeStep ) );

            Root::UpdateState( timeStep );

            Root::EndFrame();
        }
    }

    auto EditorApp::SetupEventCallbacks() -> void {

        AddHandler( EventType::WINDOW_CLOSE_EVENT,
                    [this]( Event &event ) -> bool {
                        m_State = ApplicationStatus::STOPPED;
                        event.SetHandled( true );
                        MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Handled Window Event close" );
                        return true;
                    } );

        EventService::Get().Subscribe( this );
    }
}// namespace Mikoto
