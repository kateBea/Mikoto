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
#include <SandboxApp.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioService.hh>
#include <Core/InputService.hh>
#include <Filesystem/FileService.hh>
#include <Logging/Logger.hh>
#include <Logging/StackTrace.hh>
#include <Threading/TaskManager.hh>
#include <Threading/TaskService.hh>

namespace Mikoto {

    auto SandboxApp::Run( const Int32 argc, char **argv ) -> Int32 {
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

    auto SandboxApp::Init() -> void {

        // Load configuration
        BaseConfiguration configApp{ "./engine-config.toml" };

        // App window
        m_Window = Window::Create({
            .Title{ configApp.Get<std::string>( "application.title", "Mikoto Sandbox" ) },
            .Width{ static_cast<Int32>( configApp.Get<Int64>( "application.width", 1920 ) ) },
            .Height{ static_cast<Int32>( configApp.Get<Int64>( "application.height", 1080 ) ) },
            .Backend{ InferAPI( configApp.Get<std::string>( "renderer.api", "" ) ) },
            .Resizable{ configApp.Get<bool>( "application.resizable", true ) }
        });

        if ( m_Window ) {
            m_Window->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR( "Failed to create main application window!" );
        }

        // Configure and initialize the engine
        const RootConfig config{
            .LockFrameRate{ configApp.Get<bool>( "renderer.vsync", false )  },
            .TargetWindow{ m_Window.get() }
        };

        Root::Init(config);

        SetupEventCallbacks();
    }

    auto SandboxApp::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG("Shutting down Mikoto Editor...");

        Root::Shutdown();
    }

    auto SandboxApp::TestCode() -> void {
        static bool firstRun{ true };
        if ( firstRun ) {
            firstRun = false;

            const auto device{ AudioService::Get().GetDevice() };
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
                MKT_CORE_LOGGER_ERROR( "Audio handle is empty! {}", file->GetPath() );
            } else {
                m_SourceHandle = handle->CreateSource();
                m_SourceHandle->SetLooping( true );
            }

            file = FileService::GetPtr()->LoadFile( "./vtuber_8899707_rockoTensei.mp3" );
            const AudioLoadDescription desc2{
                .AudioFile{ file } ,
                .Volume{ 0.5f },
            };
            AudioHandle handle1{ device->LoadAudio( desc2 ) };
            if ( handle1.IsEmpty() ) {
                MKT_CORE_LOGGER_ERROR( "Audio handle is empty! {}", file->GetPath() );
            } else {
                m_SourceHandle2 = handle1->CreateSource();
                m_SourceHandle2->SetLooping( true );
            }
        }

        if ( InputService::Get().IsKeyPressed( KeyCode::Key_Escape ) ) {
            TaskManager* manager{ TaskService::Get().GetManager() };

            manager->SubmitTask( new Task<void>( []() -> void {
                MKT_CORE_LOGGER_TRACE( "Hello from Thread:" );
            } ) );
        }
    }

    auto SandboxApp::Update() -> void {
        TestCode();

        if ( !m_Window->IsMinimized() ) {
            Root::StartFrame();

            Root::UpdateState();

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
                    [this, deltaVolume = 0.5f]( Event &event ) -> bool {
                        auto& keyEvent{ static_cast<KeyPressedEvent&>( event ) };
                        MKT_CORE_LOGGER_TRACE( "Key pressed: {}", GetStringRepresentation(static_cast<KeyCode>(keyEvent.GetKeyCode())) );

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_1 ) ) {
                            if (!m_Target.IsEmpty()) {  m_Target->Stop(); }

                            m_Target = m_SourceHandle;
                            MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Swtching music");

                            m_Target->Play();

                        } else if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_2 ) ) {
                            if (!m_Target.IsEmpty()) {  m_Target->Stop(); }
                            m_Target = m_SourceHandle2;

                            m_Target->Play();
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Space ) ) {
                            if (m_Target->IsPlaying()) {
                                m_Target->Stop();
                                MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Pausing music" );
                            } else {
                                m_Target->Play();
                                MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Playing music" );
                            }
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Up ) ) {
                            m_SourceHandle->IncreaseVolume(deltaVolume);
                            MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Increasing volume" );
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Down ) ) {
                            m_SourceHandle->DecreaseVolume(deltaVolume);
                            MKT_CORE_LOGGER_TRACE( "EditorApp::EventManager - Decreasing volume" );
                        }

                        return true;
                    } );


        EventService::Get().Subscribe( this );
    }
}// namespace Mikoto
