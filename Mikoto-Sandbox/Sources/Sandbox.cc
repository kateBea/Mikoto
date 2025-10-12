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
#include <Logging/StackTrace.hh>
#include <Memory/Allocator.hh>
#include <Renderer/Buffer.hh>
#include <Renderer/GpuUtility.hh>
#include <Renderer/RenderService.hh>
#include <SandboxApp.hh>
#include <Threading/TaskManager.hh>
#include <Threading/TaskService.hh>
#include <MusicPlayerLayer.hh>

namespace Mikoto {

    auto SandboxApp::Run( const Int32 argc, char **argv ) -> Int32 {
        MKT_CORE_LOGGER_DEBUG( "Initializing Mikoto Editor..." );

        Int32 exitCode{ EXIT_SUCCESS };

        try {
            Init();

            while ( IsRunning() ) {
                Update();
            }

        } catch ( const std::exception &e ) {
            MKT_CORE_LOGGER_CRITICAL( "Error occurred {}", e.what() );
            exitCode = EXIT_FAILURE;
        }

        Shutdown();

        return exitCode;
    }

    auto SandboxApp::Init() -> void {

        // Load configuration
        BaseConfiguration configApp{ "./engine-config.toml" };

        // App window
        m_Window = Window::Create( { .Title{ configApp.Get<std::string>( "application.title" ) },
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
    }

    auto SandboxApp::Shutdown() -> void {
        MKT_CORE_LOGGER_DEBUG( "Shutting down Mikoto Editor..." );

        m_LayerStack.Clear();

        Root::Shutdown();
    }

    auto SandboxApp::TestCode() -> void {
        static bool firstRun{ true };
        if ( firstRun ) {
            firstRun = false;


            auto file{ FileService::Get()->LoadFile( "./ringtone.mp3" ) };
            if ( !file ) {
                MKT_CORE_LOGGER_ERROR( "Failed to load audio file!" );
            }

            const AudioLoadDescription desc{
                .AudioFile{ file },
                .Volume{ 0.5f },
            };

            // Load from a device
            auto *audioDevice{ AudioService::Get()->GetDevice() };
            if ( AudioHandle handle{ audioDevice->LoadAudio( desc ) }; handle.IsEmpty() ) {
                MKT_CORE_LOGGER_ERROR( "Audio handle is empty! {}", file ? file->GetPath() : std::string{ "No file" } );
            } else {
                m_Audios.emplace_back( handle );
            }

            // Load from asset service
            file = FileService::GetPtr()->LoadFile( "./vtuber_8899707_rockoTensei.mp3" );
            const AudioLoadDescription desc2{
                .AudioFile{ file },
                .Volume{ 0.5f },
            };
            if ( AudioHandle handle1{ AssetsService::Get()->LoadAsset<Audio>( desc2 ) }; handle1.IsEmpty() ) {
                MKT_CORE_LOGGER_ERROR( "Audio handle is empty! {}", file ? file->GetPath() : std::string{ "No file" } );
            } else {
                m_Audios.emplace_back( handle1 );
            }

            const auto device{ AudioService::Get()->GetDevice() };
            if ( device->GetAudio( "./virtual_5855285.mp3" ).IsEmpty() ) {
                MKT_CORE_LOGGER_WARN( "Audio track virtual_5855285.mp3 does not exists" );

                auto file{ FileService::Get()->LoadFile( "./virtual_5855285.mp3" ) };
                const AudioLoadDescription desc{
                    .AudioFile{ file },
                    .Volume{ 0.5f },
                };

                AudioHandle handle{ AssetsService::Get()->LoadAsset<Audio>( desc ) };
                if ( handle.IsEmpty() ) {
                    MKT_CORE_LOGGER_ERROR( "Audio handle is empty! {}", file ? file->GetPath() : std::string{ "No file" } );
                } else {
                    m_Audios.emplace_back( handle );
                }

                if ( !device->GetAudio( "./virtual_5855285.mp3" ).IsEmpty() ) {
                    MKT_CORE_LOGGER_DEBUG( "Audio track virtual_5855285.mp3 has been loaded." );
                }

                // MKT_CORE_LOGGER_DEBUG( "Loading audio asset asynchronously" );
                // AssetsService::Get()->LoadAssetAsync<Audio>( AudioLoadDescription {
                //     .AudioFile{ FileService::Get()->LoadFile( "./harajuku_8211997.mp3" ) },
                //     .Volume{ 1.0 }
                // } );
            }

            static bool gpuDevTest{ true };
            if ( gpuDevTest ) {
                // Some example data: a few floats for a vertex buffer
                std::array vertexData{
                    0.0f, 0.5f, 0.0f,  // Vertex 1 (x, y, z)
                    -0.5f, -0.5f, 0.0f,// Vertex 2
                    0.5f, -0.5f, 0.0f  // Vertex 3
                };

                // Describe the buffer
                BufferDescription desc{};
                desc.WithSizeBytes( vertexData.size() * sizeof( vertexData[0] ) )
                        .WithData( AsBytes( vertexData.data() ) )
                        .WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
                        .WithBufferDataType( BufferDataType::BUFFER_DATA_FLOAT32 )
                        .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

                // Create it through the GPU device (Vulkan or otherwise)
                const auto gpuDev{ RenderService::Get()->GetGpuDevice() };
                BufferHandle vertexBuffer{ gpuDev->CreateBuffer( desc ) };

                // Allocate staging buffer to copy over the texture data
                BufferDescription stagingDesc{};
                stagingDesc.WithData( nullptr )
                        .WithUsage( BufferUsage::BUFFER_USAGE_STAGING )
                        .WithSizeBytes( MKT_MEGABYTES( 10 ) )
                        .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STREAM );
                BufferHandle stagingBuffer{ gpuDev->CreateBuffer( stagingDesc ) };

                TextureLoadDescription loadDesc{};
                loadDesc
                        .WithFile( FileService::Get()->LoadFile( "./texture.png" ) )
                        .WithType( TextureType::TEXTURE_2D );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadDesc ) };

                gpuDevTest = false;
            }
        }
    }

    auto SandboxApp::Update() -> void {
        TestCode();

        TimeService::Get()->Update();
        const double timeStep{ TimeService::Get().GetTimeStep( TimeUnit::SECONDS ) };

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
                    [this, deltaVolume = 0.5f]( Event &event ) -> bool {
                        auto &keyEvent{ static_cast<KeyPressedEvent &>( event ) };
                        MKT_CORE_LOGGER_TRACE( "Key pressed: {}", GetStringRepresentation( static_cast<KeyCode>( keyEvent.GetKeyCode() ) ) );

                        Int32 position{ keyEvent.GetKeyCode() - static_cast<Int32>( Key_0 ) };
                        if ( position < m_Audios.size() ) {
                            if ( !m_Target.IsEmpty() ) { m_Target->Stop(); }

                            m_Target = m_Audios.at( position )->CreateSource();
                            MKT_CORE_LOGGER_TRACE( "SandboxApp::EventManager - Switching music {}", m_Audios.at( position )->GetFile()->GetPath() );

                            if ( !m_Target.IsEmpty() ) { m_Target->Play(); }
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Enter ) ) {
                            // Load new audio
                            TaskService::Get()->Submit( [this]() -> void {
                                std::string path{};
                                MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_AQUA, "Enter audio path: " );
                                std::getline( std::cin, path );

                                auto file{ FileService::Get()->LoadFile( path ) };
                                if ( !file ) {
                                    MKT_CORE_LOGGER_ERROR( "Failed to load audio file at {}!", path );
                                    return;
                                }

                                const AudioLoadDescription desc{
                                    .AudioFile{ file },
                                    .Volume{ 0.5f },
                                };

                                // Load from a device
                                if ( AudioHandle handle{ AssetsService::Get()->LoadAsset<Audio>( desc ) }; handle.IsEmpty() ) {
                                    MKT_CORE_LOGGER_ERROR( "Audio handle is empty! {}", file ? file->GetPath() : std::string{ "No file" } );
                                } else {
                                    m_Audios.emplace_back( handle );
                                    MKT_CORE_LOGGER_TRACE( "Loaded audio track {}", file ? file->GetPath() : std::string{ "No file" } );
                                }
                            } );
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Space ) ) {
                            if ( !m_Target.IsEmpty() && m_Target->IsPlaying() ) {
                                m_Target->Stop();
                                MKT_CORE_LOGGER_TRACE( "SandboxApp - Pausing music" );
                            } else {
                                m_Target->Play();
                                MKT_CORE_LOGGER_TRACE( "SandboxApp - Playing music" );
                            }
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Up ) ) {
                            if ( !m_Target.IsEmpty() ) m_Target->IncreaseVolume( deltaVolume );
                            MKT_CORE_LOGGER_TRACE( "SandboxApp - Increasing volume" );
                        }

                        if ( keyEvent.GetKeyCode() == static_cast<Int32>( KeyCode::Key_Down ) ) {
                            if ( !m_Target.IsEmpty() ) m_Target->DecreaseVolume( deltaVolume );
                            MKT_CORE_LOGGER_TRACE( "SandboxApp - Decreasing volume" );
                        }

                        return true;
                    } );

        EventService::Get()->Subscribe( this );
    }
}// namespace Mikoto
