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
#include <EditorApp.hh>
#include <Assets/AssetsManager.hh>
#include <Core/CoreEvents.hh>
#include <Core/EventManager.hh>
#include <Core/FileManager.hh>
#include <Core/TimeManager.hh>
#include <GUI/ImGuiManager.hh>
#include <Platform/Input/InputManager.hh>
#include <Renderer/Core/RenderContext.hh>
#include <Scene/SceneManager.hh>
#include <Threading/TaskManager.hh>

namespace Mikoto {
    /**
     * GetApplicationSpec
     * @param args
     * @deprecated Will use config loader with TOML
     * @return App specs
     * */
    static auto GetApplicationSpec( const std::vector<std::string> &args ) -> ApplicationData {
        return {
            .WindowWidth = 1920,
            .WindowHeight = 1080,
            .Name = "Mikoto Editor",
            .WorkingDirectory = std::filesystem::current_path(),
            .Executable = Path_T{ args[0] },
            .RenderingBackend = GraphicsAPI::VULKAN_API,
            .CommandLineArguments = { args.begin(), args.end() },
        };
    }


    auto EditorApp::Run( const Int32_T argc, char **argv ) -> Int32_T {
        ParseArguments( argc, argv );

        auto exitCode{ EXIT_SUCCESS };
        auto appSpecs{ GetApplicationSpec( m_CommandLineArgs ) };

        try {

            Init( std::move( appSpecs ) );

            while ( IsRunning() ) {
                ProcessEvents();
                UpdateState();
                Present();
            }

            Shutdown();

        } catch ( const std::exception &exception ) {
            MKT_COLOR_STYLE_PRINT_FORMATTED( MKT_FMT_COLOR_RED, MKT_FMT_STYLE_BOLD, "{}", exception.what() );
            exitCode = EXIT_FAILURE;
        }

        return exitCode;
    }

    auto EditorApp::ParseArguments( const Int32_T argc, char **argv ) -> void {
        for ( const auto limit{ std::addressof( argv[argc] ) }; argv != limit; ++argv ) {
            m_CommandLineArgs.emplace_back( *argv );
        }
    }

    auto EditorApp::Init(ApplicationData &&appSpec) -> void {
        TimeManager::Init();
        TaskManager::Init();

        m_State = Status::RUNNING;
        m_Spec = std::move(appSpec);

        MKT_APP_LOGGER_INFO("=================================================================");
        MKT_APP_LOGGER_INFO("Executable                : {}", m_Spec.Executable.string());
        MKT_APP_LOGGER_INFO("Current working directory : {}", m_Spec.WorkingDirectory.string());
        MKT_APP_LOGGER_INFO("=================================================================");

        FileManager::Assets::SetRootPath("../Resources");

        WindowProperties windowProperties{ m_Spec.Name, m_Spec.RenderingBackend, m_Spec.WindowWidth, m_Spec.WindowHeight };
        windowProperties.AllowResizing(true);
        m_MainWindow = Window::Create(std::move(windowProperties));

        if (m_MainWindow) {
            m_MainWindow->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR("EditorApp - Could not create application main window.");
        }

        FileManager::Init();
        InputManager::Init(m_MainWindow.get());
        RenderContextData contextSpec{ .TargetAPI = m_Spec.RenderingBackend, .Handle = m_MainWindow };

        RenderContext::Init(std::move(contextSpec));
        ImGuiManager::Init(m_MainWindow);

        // Initialize the assets' manager.
        // Important to do after initializing the renderer loads
        // some prefabs that require having a render context ready.
        AssetsManager::Init(AssetsManagerSpec{ .AssetRootDirectory = "../Resources" });
        SceneManager::Init();

        InitLayers();
        InstallEventCallbacks();

        MKT_APP_LOGGER_INFO("=================================================================");
        MKT_APP_LOGGER_INFO("Init time {} seconds", TimeManager::GetTime());
        MKT_APP_LOGGER_INFO("=================================================================");
    }

    auto EditorApp::InstallEventCallbacks() -> void {
        EventManager::Subscribe(m_Guid.Get(),
                                EventType::APP_CLOSE_EVENT,
                                [this](Event &event) -> bool {
                                    m_State = Status::STOPPED;
                                    event.SetHandled(true);
                                    MKT_APP_LOGGER_WARN("EditorApp::EventManager - Handled App Event close");
                                    return false;
                                });

        EventManager::Subscribe(m_Guid.Get(),
                                EventType::WINDOW_CLOSE_EVENT,
                                [this](Event &event) -> bool {
                                    m_State = Status::STOPPED;
                                    event.SetHandled(true);
                                    MKT_APP_LOGGER_WARN("EditorApp::EventManager - Handled Window Event close");
                                    return false;
                                });

        EventManager::Subscribe(m_Guid.Get(),
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

        DestroyLayers();
        AssetsManager::Shutdown();
        SceneManager::Shutdown();

        RenderContext::PushShutdownCallback([]() -> void {
            // ImGui requires the Context to be alive so
            // it is shutdown after the context is deleted.
            ImGuiManager::Shutdown();
        });

        RenderContext::Shutdown();
        InputManager::Shutdown();
        EventManager::Shutdown();
        FileManager::Shutdown();

        m_MainWindow->Shutdown();
        TaskManager::Shutdown();
    }

    auto EditorApp::DestroyLayers() -> void {
        m_EditorLayer->OnDetach();
    }

    auto EditorApp::InitLayers() -> void {
        m_EditorLayer = std::make_unique<EditorLayer>();
        m_EditorLayer->OnAttach();
    }

    auto EditorApp::UpdateLayers() -> void {
        const auto timeStep{ TimeManager::GetTimeStep() };
        m_EditorLayer->OnUpdate( timeStep );
    }

    auto EditorApp::RenderImGuiFrame() -> void {
        ImGuiManager::BeginFrame();
        m_EditorLayer->PushImGuiDrawItems();
        ImGuiManager::EndFrame();
    }

    auto EditorApp::ProcessEvents() -> void {
        m_MainWindow->ProcessEvents();
        EventManager::ProcessEvents();
    }

    auto EditorApp::UpdateState() -> void {
        TimeManager::UpdateTimeStep();

        if (!m_MainWindow->IsMinimized()) {
            RenderContext::PrepareFrame();
            UpdateLayers();

#if !(NDEBUG)
            // [ DEBUG: Multithreading ]
            if (InputManager::IsKeyPressed(Key_E)) {
                TaskManager::Execute(
                        []() -> void {
                            MKT_APP_LOGGER_DEBUG("Hello thread. Count: {}", TaskManager::GetWorkersCount());
                        });
            }
        }
#endif

        RenderImGuiFrame();
        RenderContext::SubmitFrame();
    }

    auto EditorApp::Present() -> void {
        RenderContext::PresentFrame();
    }
}
