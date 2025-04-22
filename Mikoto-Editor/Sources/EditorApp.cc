/**
 * EditorApp.cc
 * Created by kate on 6/7/23.
 * */

// C++ Standard Library
#include <cstdlib>
#include <stdexcept>
#include <utility>

// Project headers
#include <Core/EventService.hh>
#include <Core/Logger.hh>
#include <Core/ServiceManager.hh>
#include <Core/StackTrace.hh>
#include <EditorApp.hh>
#include <FileSystem/FileService.hh>
#include <Layers/EditorLayer.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Timing/TimeService.hh>
#include <Timing/Timer.hh>

namespace Mikoto {

    static auto GetCommandDescription( const std::string_view command ) -> std::string {
        if ( command == "-h" || command == "--help" ) {
            return "Displays the help menu.";
        }

        return "Unknown command.";
    }

    auto EditorApp::Run( const Int32_T argc, char **argv ) -> Int32_T {
        Int32_T exitCode{ EXIT_SUCCESS };

        try {
            SetupCmdArguments();
            CheckArguments( argc, argv );

            Init();

            while ( IsRunning() ) {
                Update();
            }

            Shutdown();

        } catch ( const std::exception &exception ) {
            MKT_STACK_TRACE();

            MKT_COLOR_STYLE_PRINT_FORMATTED( MKT_FMT_COLOR_RED, MKT_FMT_STYLE_BOLD, "{}", exception.what() );

            exitCode = EXIT_FAILURE;
        }

        return exitCode;
    }

    auto EditorApp::SetupCmdArguments() -> void {
        m_ArgsParser = ArgsParser::Create( { .Description{ "Mikoto args parse" }, .ProgramName{ "Mikoto Editor" } } );

        const ArgsParser::Command helpCommand{
            .Parameter{ "--help" },
            .Description{ GetCommandDescription( "--help" ) },
            .IsRequired{ false },
            .Action{ [this]() -> void {
                // Print help function

                return;
            } },
        };

        m_ArgsParser->Emplace( helpCommand );
    }

    auto EditorApp::CheckArguments( const Int32_T argc, char **argv ) const -> void {
        if ( !m_ArgsParser->Validate( argc, argv ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid command line arguments" );
        }
    }

    auto EditorApp::Init() -> void {
        MKT_PROFILE_SCOPE();

        const auto configFilePath{ PathBuilder()
                                           .WithPath( FileService::GetInstance()->GetCurrentWorkingDirectory() )
                                           .WithPath( "engine-config.toml" )
                                           .Build() };

        m_Options = ConfigLoader::LoadFromFile( configFilePath );

        m_MainWindow = Window::Create( { .Title{ m_Options.EngineName },
                                         .Width{ m_Options.WindowWidth },
                                         .Height{ m_Options.WindowHeight },
                                         .Backend{ m_Options.RendererAPI },
                                         .Resizable{ m_Options.AllowWindowResizing } } );

        if ( m_MainWindow ) {
            m_MainWindow->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR( "EditorApp::Init - Could not create application window." );
        }

        const EngineConfig config{
            .Options{ m_Options },
            .TargetWindow{ m_MainWindow.get() },
        };

        ServiceManager::Init( config );

        InitLayers();

        InstallEventCallbacks();
    }

    auto EditorApp::InstallEventCallbacks() -> void {
        AddHandler( EventType::APP_CLOSE_EVENT,
                    [this]( Event &event ) -> bool {
                        m_State = ApplicationStatus::STOPPED;
                        event.SetHandled( true );
                        MKT_APP_LOGGER_WARN( "EditorApp::EventManager - Handled App Event close" );
                        return false;
                    } );

        AddHandler( EventType::WINDOW_CLOSE_EVENT,
                    [this]( Event &event ) -> bool {
                        m_State = ApplicationStatus::STOPPED;
                        event.SetHandled( true );
                        MKT_APP_LOGGER_WARN( "EditorApp::EventManager - Handled Window Event close" );
                        return false;
                    } );

        AddHandler( EventType::WINDOW_RESIZE_EVENT,
                    [this]( Event & ) -> bool {
                        m_State = m_MainWindow->IsMinimized() ? ApplicationStatus::IDLE : ApplicationStatus::RUNNING;
                        MKT_APP_LOGGER_WARN( "EditorApp::EventManager - Handled Window Resize Event" );
                        return false;
                    } );

        EventService::GetInstance()->Subscribe( this, EventType::APP_CLOSE_EVENT );
        EventService::GetInstance()->Subscribe( this, EventType::WINDOW_CLOSE_EVENT );
        EventService::GetInstance()->Subscribe( this, EventType::WINDOW_RESIZE_EVENT );
    }

    auto EditorApp::Shutdown() -> void {
        MKT_APP_LOGGER_INFO( "=====================================" );
        MKT_APP_LOGGER_INFO( "Shutting down application. Cleanup..." );
        MKT_APP_LOGGER_INFO( "=====================================" );

        // Shutdown layers
        DestroyLayers();

        // Shutdown systems
        ServiceManager::Shutdown();

        // Shutdown the main window
        m_MainWindow->Shutdown();
    }

    auto EditorApp::DestroyLayers() -> void {
        for ( const auto &[id, layer]: m_LayerRegistry ) {
            layer->OnDetach();
        }

        m_LayerRegistry.Clear();
    }

    auto EditorApp::InitLayers() -> void {

        EditorLayerCreateInfo editorLayerCreateInfo{
            .TargetWindow{ m_MainWindow.get() },
        };

        m_LayerRegistry.Register<EditorLayer>( editorLayerCreateInfo );

        for ( const auto &[id, layer]: m_LayerRegistry ) {
            layer->OnAttach();
        }
    }

    auto EditorApp::UpdateLayers() const -> void {
        const double timeStep{ TimeService::GetInstance()->GetTimeStep( TimeUnit::SECONDS ) };

        for ( const auto &[id, layer]: m_LayerRegistry ) {
            // Handle GUI Logic, this does not render the GUI
            // Simply updates the GUI state
            layer->PushImGuiDrawItems( timeStep );

            layer->OnUpdate( timeStep );
        }
    }

    auto EditorApp::Update() -> void {
        if ( !m_MainWindow->IsMinimized() ) {
            ServiceManager::StartFrame();

            // Update the layers. We determine the state of the application
            // In the Editor layer we will update the scene, the camera, and the renderer
            UpdateLayers();

            ServiceManager::Update();
            ServiceManager::EndFrame();
        }
    }
}// namespace Mikoto
