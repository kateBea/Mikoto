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
#include <Audio/AudioService.hh>
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
            MKT_CORE_LOGGER_CRITICAL("Error occurred", e.what());
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

    auto EditorApp::TestCode() -> void {
        static bool firstRun{ true };
        if ( firstRun ) {
            firstRun = false;

            auto device{ AudioService::Get().GetDevice() };
            auto file{ FileService::GetPtr()->LoadFile( "./ringtone.mp3" ) };

            if ( !file ) {
                MKT_CORE_LOGGER_ERROR( "Failed to load audio file!" );
            }

            const AudioLoadDescription desc{
                .AudioFile{ file } ,
                .Volume{ 0.5f },
            };

            AudioHandle handle{ device->LoadAudio( desc ) };
            if ( handle.IsEmpty() ) {
                MKT_CORE_LOGGER_ERROR( "Audio handle is empty!" );
            } else {
                m_SourceHandle = handle->CreateSource();
                m_SourceHandle->SetLooping( true );
                m_SourceHandle->Play();
            }
        }
    }

    auto EditorApp::Update() -> void {
        TestCode();

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

        AddHandler( EventType::KEY_PRESSED_EVENT,
                    [this, deltaVolume = 0.5f]( Event &event ) -> bool {
                        auto& keyEvent{ static_cast<KeyPressedEvent&>( event ) };
                        MKT_CORE_LOGGER_TRACE( "Key pressed: {}", GetStringRepresentation(static_cast<KeyCode>(keyEvent.GetKeyCode())) );

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Space ) ) {
                            if (m_SourceHandle->IsPlaying()) {
                                m_SourceHandle->Stop();
                            } else {
                                m_SourceHandle->Play();
                            }

                            MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Pausing music" );
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Up ) ) {
                            m_SourceHandle->IncreaseVolume(deltaVolume);
                            MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Decreasing volume" );
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Down ) ) {
                            m_SourceHandle->DecreaseVolume(deltaVolume);
                            MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Increasing volume" );
                        }

                        return true;
                    } );


        EventService::Get().Subscribe( this );
    }
}// namespace Mikoto
