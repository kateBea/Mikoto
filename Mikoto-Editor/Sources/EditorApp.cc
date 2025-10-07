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
#include <Core/Configuration.hh>
#include <Core/CoreEvents.hh>
#include <Core/Root.hh>
#include <EditorApp.hh>
#include <Audio/AudioDevice.hh>
#include <Core/InputService.hh>
#include <Filesystem/FileService.hh>
#include <Logging/Logger.hh>
#include <Logging/StackTrace.hh>

namespace Mikoto {

    auto EditorApp::Run( const Int32 argc, char **argv ) -> Int32 {
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

        // App window
        m_Window = Window::Create({
            .Title{ "Mikoto engine" },
            .Width{ 1920 },
            .Height{ 1080 },
            .Backend{ GraphicsAPI::VULKAN_API },
            .Resizable{ true }
        });

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
    }

    auto EditorApp::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG("Shutting down Mikoto Editor...");

        Root::Shutdown();
    }

    auto EditorApp::Update() -> void {

        if ( !m_Window->IsMinimized() ) {
            Root::StartFrame();

            Root::UpdateState();

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
