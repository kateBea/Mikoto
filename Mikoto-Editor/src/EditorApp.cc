/**
 * EditorApp.cc
 * Created by kate on 6/7/23.
 * */

// C++ Standard Library
#include <stdexcept>
#include <cstdlib>
#include <cmath>
#include <utility>

// Project headers
#include <EditorApp.hh>
#include <Common/Application.hh>
#include <Assets/AssetsManager.hh>
#include <Core/CoreEvents.hh>
#include <Core/EventManager.hh>
#include <Core/FileManager.hh>
#include <Core/TimeManager.hh>
#include <GUI/ImGuiManager.hh>
#include <Platform/InputManager.hh>
#include <Renderer/RenderContext.hh>
#include <Scene/SceneManager.hh>
#include <Threading/TaskManager.hh>

namespace Mikoto {

    static auto GetApplicationSpec(const std::vector<std::string>& args) -> AppSpec {
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

    auto EditorApp::Run(Int32_T argc, char** argv) -> Int32_T {
        ParseArguments(argc, argv);

        Int32_T exitCode{ EXIT_SUCCESS };
        auto appSpecs{ GetApplicationSpec(m_CommandLineArgs) };

        if (Init( std::move(appSpecs) )) {
            try {

                while (IsRunning()) {
                    ProcessEvents();
                    UpdateState();

                    RenderContext::Present();
                }

                Shutdown();

            } catch( const std::exception& exception ) {
                MKT_COLOR_STYLE_PRINT_FORMATTED(MKT_FMT_COLOR_RED, MKT_FMT_STYLE_BOLD, "Exception! {}", exception.what());
                exitCode = EXIT_FAILURE;
            }
        }

        return exitCode;
    }

    auto EditorApp::ParseArguments(Int32_T argc, char** argv) -> void {
        const auto limit{ std::addressof(argv[argc]) };
        for ( ; argv < limit; ++argv) {
            m_CommandLineArgs.emplace_back(*argv);
        }
    }

    auto EditorApp::Init(AppSpec&& appSpec) -> bool {
        TimeManager::Init();

        m_Spec = std::move(appSpec);

        // Set the assets root path (this path contains import files like shaders, prefabs, etc.)
        FileManager::Assets::SetRootPath("..\\Assets");

        TaskManager::Init();

        MKT_APP_LOGGER_INFO("=================================================================");
        MKT_APP_LOGGER_INFO("Executable                : {}", m_Spec.Executable.string());
        MKT_APP_LOGGER_INFO("Current working directory : {}", m_Spec.WorkingDirectory.string());
        MKT_APP_LOGGER_INFO("=================================================================");

        WindowProperties windowProperties{ m_Spec.Name, m_Spec.RenderingBackend, m_Spec.WindowWidth, m_Spec.WindowHeight };
        windowProperties.AllowResizing(true);

        m_MainWindow = Window::Create(std::move(windowProperties));

        if (m_MainWindow) {
            m_MainWindow->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR("Could not create application main window!");
        }


        // THESE BELOW SHOULD BE INITIALIZED BY THE ENGINE ITSELF
        // NOT THE APPLICATION.

        // Serializer Init
        FileManager::Init();

        // Initialize the input manager
        InputManager::Init(m_MainWindow.get());

        RenderContextSpec contextSpec{};
        contextSpec.Backend = m_Spec.RenderingBackend;
        contextSpec.WindowHandle = m_MainWindow;

        // Initialize the render context
        RenderContext::Init(std::move(contextSpec));
        RenderContext::EnableVSync();

        // Initialize the assets' manager. Important to do at the end
        // as it loads some prefabs which require to have a render context ready.
        AssetsManagerSpec assetsManagerSpec{};
        assetsManagerSpec.AssetRootDirectory = "../Assets";

        AssetsManager::Init(std::move(assetsManagerSpec));

        // Initialize the scene manager
        SceneManager::Init();

        CreateLayers();

        InstallEventCallbacks();

        MKT_APP_LOGGER_INFO("=================================================================");
        MKT_APP_LOGGER_INFO("Init time {} seconds", TimeManager::GetTime());
        MKT_APP_LOGGER_INFO("=================================================================");

        return true;
    }

    auto EditorApp::InstallEventCallbacks() -> void {
        EventManager::Subscribe(m_Guid.Get(),
                                 EventType::APP_CLOSE_EVENT,
                                 [this](Event& event) -> bool {
                                     m_State = Status::STOPPED;
                                     event.SetHandled(true);
                                     MKT_APP_LOGGER_WARN("Handled App Event close");
                                     return false;
                                 });

        EventManager::Subscribe(m_Guid.Get(),
                                 EventType::WINDOW_CLOSE_EVENT,
                                 [this](Event& event) -> bool {
                                     m_State = Status::STOPPED;
                                     event.SetHandled(true);
                                     MKT_APP_LOGGER_WARN("Handled Window Event close");
                                     return false;
                                 });

        EventManager::Subscribe(m_Guid.Get(),
                                 EventType::WINDOW_RESIZE_EVENT,
                                 [this](Event&) -> bool {
                                     m_State = m_MainWindow->IsMinimized() ? Status::IDLE : Status::RUNNING;
                                     MKT_APP_LOGGER_WARN("Handled Window Resize Event");
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

    auto EditorApp::CreateLayers() -> void {
        m_EditorLayer = std::make_unique<EditorLayer>();
        m_EditorLayer->OnAttach();
    }

    auto EditorApp::UpdateLayers() -> void {
        auto timeStep{ TimeManager::GetTimeStep() };
        m_EditorLayer->OnUpdate(timeStep);
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

            // [ DEBUG: Multithreading ]
            if (InputManager::IsKeyPressed(KeyCode::Key_E)) {
                TaskManager::Execute(
                        []() -> void {
                            MKT_APP_LOGGER_DEBUG("Hello thread. Count: {}", TaskManager::GetWorkersCount());
                        });
            }
        }

        RenderImGuiFrame();
        RenderContext::SubmitFrame();
    }
}