/**
 * EditorApp.cc
 * Created by kate on 6/7/23.
 * */

// C++ Standard Library
#include <stdexcept>
#include <cstdlib>
#include <utility>

// Project headers
#include <Common/Constants.hh>
#include <Core/Engine.hh>
#include <Core/System/EventSystem.hh>
#include <Core/System/InputSystem.hh>
#include <Core/System/TaskSystem.hh>
#include <Core/System/TimeSystem.hh>
#include <EditorApp.hh>
#include <Layers/EditorLayer.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Profiling/Timer.hh>
#include <Core/Logging/StackTrace.hh>

namespace Mikoto {

    static auto GetCommandDescription( const std::string_view command ) -> std::string {
        if ( command == "-h" || command == "--help" ) {
            return "Displays the help menu.";
        }

        return "Unknown command.";
    }

    auto EditorApp::Run( const Int32_T argc, char **argv ) -> Int32_T {
        Int32_T exitCode{ EXIT_SUCCESS };

        ParseCommandLineArgs( argc, argv );

        try {

            Init();

            while ( IsRunning() ) {
                UpdateState();
            }

            Shutdown();

        } catch ( const std::exception &exception ) {
            MKT_STACK_TRACE();

            MKT_COLOR_STYLE_PRINT_FORMATTED( MKT_FMT_COLOR_RED, MKT_FMT_STYLE_BOLD, "{}", exception.what() );

            exitCode = EXIT_FAILURE;
        }

        return exitCode;
    }

    auto EditorApp::ParseCommandLineArgs( const Int32_T argc, char **argv ) -> void {
        m_CommandLineParser = CreateScope<CommandLineParser>();

        // The first argument is generally the program's executable path
        for ( const auto limit{ argv + argc }; argv < limit; ++argv ) {
            m_CommandLineParser->Insert( *argv, GetCommandDescription(*argv), [argv]() -> void {
                MKT_APP_LOGGER_DEBUG( "Running command {}", *argv);
            } );
        }
    }

    auto EditorApp::Init() -> void {
        MKT_PROFILE_SCOPE();

        const auto configFilePath{ PathBuilder()
            .WithPath( std::filesystem::current_path().string() )
            .WithPath( "engine-config.toml" )
            .Build()
        };

        const auto options{ ConfigLoader::LoadFromFile( configFilePath ) };

        m_MainWindow = Window::Create({
            .Title{ options->EngineName },
            .Width{ options->WindowWidth },
            .Height{ options->WindowHeight },
            .Backend{ options->RendererAPI },
            .Resizable{ options->AllowWindowResizing }
        });

        if ( m_MainWindow ) {
            m_MainWindow->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR( "EditorApp::Init - Could not create application window." );
        }

        const EngineConfig config{
            .Options{ *options },
            .TargetWindow{ m_MainWindow.get() },
        };

        Engine::Init( config );

        InitLayers();

        InstallEventCallbacks();
    }

    auto EditorApp::InstallEventCallbacks() -> void {
        auto& eventManager{ Engine::GetSystem<EventSystem>() };
        eventManager.Subscribe(m_Guid.Get(),
                                EventType::APP_CLOSE_EVENT,
                                [this](Event &event) -> bool {
                                    m_State = Status::STOPPED;
                                    event.SetHandled(true);
                                    MKT_APP_LOGGER_WARN("EditorApp::EventManager - Handled App Event close");
                                    return false;
                                });

        eventManager.Subscribe(m_Guid.Get(),
                                EventType::WINDOW_CLOSE_EVENT,
                                [this](Event &event) -> bool {
                                    m_State = Status::STOPPED;
                                    event.SetHandled(true);
                                    MKT_APP_LOGGER_WARN("EditorApp::EventManager - Handled Window Event close");
                                    return false;
                                });

        eventManager.Subscribe(m_Guid.Get(),
                                EventType::WINDOW_RESIZE_EVENT,
                                [this](Event &) -> bool {
                                    m_State = m_MainWindow->IsMinimized() ? Status::IDLE : Status::RUNNING;
                                    MKT_APP_LOGGER_WARN("EditorApp::EventManager - Handled Window Resize Event");
                                    return false;
                                });
    }

    auto EditorApp::Shutdown() -> void {
        MKT_APP_LOGGER_INFO("=====================================");
        MKT_APP_LOGGER_INFO("Shutting down application. Cleanup...");
        MKT_APP_LOGGER_INFO("=====================================");

        // Shutdown layers
        DestroyLayers();

        // Shutdown systems
        Engine::Shutdown();

        // Shutdown the main window
        m_MainWindow->Shutdown();
    }

    auto EditorApp::DestroyLayers() -> void {
        for ( const auto& layer: m_LayerRegistry | std::views::values ) {
            layer->OnDetach();
        }

        m_LayerRegistry.Clear();
    }

    auto EditorApp::InitLayers() -> void {
        const auto& [Options, TargetWindow]{ Engine::GetConfig() };

        EditorLayerCreateInfo editorLayerCreateInfo{
            .TargetWindow{ m_MainWindow.get() },
            .Backend{ Options.RendererAPI },
            .AssetsRootDirectory{ Options.WorkingDirectory },
        };

        m_LayerRegistry.Register<EditorLayer>( editorLayerCreateInfo );

        for ( const auto& layer: m_LayerRegistry | std::views::values ) {
            layer->OnAttach();
        }
    }

    auto EditorApp::UpdateLayers() const -> void {
        const auto & timeManager{ Engine::GetSystem<TimeSystem>() };
        const auto timeStep{ timeManager.GetTimeStep() };

        for ( auto& layer: m_LayerRegistry | std::views::values ) {
            layer->OnUpdate(timeStep);
        }
    }

    auto EditorApp::UpdateState() -> void {
        if (!m_MainWindow->IsMinimized()) {
            Engine::StartFrame();

            // Handle GUI Logic, this does not render the GUI
            // Simply updates the GUI state
            auto& editorLayer{ *m_LayerRegistry.Get<EditorLayer>() };
            editorLayer.PushImGuiDrawItems();

            // Update the layers. We determine the state of the application
            // In the Editor layer we will update the scene, the camera, and the renderer
            UpdateLayers();

            Engine::UpdateState();

            Engine::EndFrame();
        }
    }
}
