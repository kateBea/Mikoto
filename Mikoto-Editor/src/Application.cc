/**
 * Application.cc
 * Created by kate on 5/25/23.
 * */

// C++ Standard Library
#include <cmath>
#include <utility>

// Project headers
#include <Application.hh>
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
    auto Application::Init(AppSpec&& appSpec) -> void {
        m_Spec = std::move(appSpec);
        TimeManager::Init();

        // Set the assets root path (this path contains import files like shaders, prefabs, etc.)
        FileManager::Assets::SetRootPath("../Assets");

        TaskManager::Init();

        MKT_APP_LOGGER_INFO("=================================================================");
        MKT_APP_LOGGER_INFO("Executable                : {}", m_Spec.Executable.string());
        MKT_APP_LOGGER_INFO("Current working directory : {}", m_Spec.WorkingDirectory.string());
        MKT_APP_LOGGER_INFO("=================================================================");

        WindowProperties windowProperties{
            m_Spec.Name,
            m_Spec.RenderingBackend,
            m_Spec.WindowWidth,
            m_Spec.WindowHeight
        };

        windowProperties.AllowResizing(true);

        m_MainWindow = Window::Create(std::move(windowProperties));

        if (m_MainWindow) {
            m_MainWindow->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR("Could not create application main window!");
        }

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

        InitializeLayers();

        InstallEventCallbacks();

        MKT_APP_LOGGER_INFO("=================================================================");
        MKT_APP_LOGGER_INFO("Init time {} seconds", TimeManager::GetTime());
        MKT_APP_LOGGER_INFO("=================================================================");
    }

    auto Application::InstallEventCallbacks() -> void {
        EventManager::Subscribe(m_Guid.Get(),
            EventType::APP_CLOSE_EVENT,
            [this](Event& event) -> bool {
                m_State = State::STOPPED;
                event.SetHandled(true);
                MKT_APP_LOGGER_WARN("Handled App Event close");
                return false;
            });

        EventManager::Subscribe(m_Guid.Get(),
            EventType::WINDOW_CLOSE_EVENT,
            [this](Event& event) -> bool {
                m_State = State::STOPPED;
                event.SetHandled(true);
                MKT_APP_LOGGER_WARN("Handled Window Event close");
                return false;
            });

        EventManager::Subscribe(m_Guid.Get(),
            EventType::WINDOW_RESIZE_EVENT,
            [this](Event&) -> bool {
                m_State = m_MainWindow->IsMinimized() ? State::IDLE : State::RUNNING;
                MKT_APP_LOGGER_WARN("Handled Window Resize Event");
                return false;
            });
    }

    auto Application::Shutdown() -> void {
        MKT_APP_LOGGER_INFO("=====================================");
        MKT_APP_LOGGER_INFO("Shutting down application. Cleanup...");
        MKT_APP_LOGGER_INFO("=====================================");
        // Release layers resources
        ReleaseLayers();
        AssetsManager::Shutdown();
        SceneManager::Shutdown();
        RenderContext::Shutdown();
        InputManager::Shutdown();
        EventManager::Shutdown();
        FileManager::Shutdown();

        m_MainWindow->Shutdown();
        TaskManager::Shutdown();
    }

    auto Application::IsRunning() -> bool {
        return m_State == State::RUNNING || m_State == State::IDLE;
    }

    auto Application::ReleaseLayers() -> void {
        m_EditorLayer->OnDetach();
    }

    auto Application::InitializeLayers() -> void {
        m_EditorLayer = std::make_unique<EditorLayer>();
        m_EditorLayer->OnAttach();
    }

    auto Application::UpdateLayers() -> void {
        auto timeStep{ TimeManager::GetTimeStep() };

        m_EditorLayer->OnUpdate(timeStep);
    }

    auto Application::ImGuiFrameRender() -> void {
        ImGuiManager::BeginFrame();

        m_EditorLayer->PushImGuiDrawItems();

        ImGuiManager::EndFrame();
    }

    auto Application::ProcessEvents() -> void {
        // Poll events from the window event queue
        m_MainWindow->ProcessEvents();

        // Poll pending events from our manager
        EventManager::ProcessEvents();
    }

    auto Application::UpdateState() -> void {
        // Update the time step
        TimeManager::UpdateTimeStep();

        // Update the layers if the window is not minimized
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

        // Draw overlays
        ImGuiFrameRender();

        RenderContext::SubmitFrame();
    }

    auto Application::Present() -> void {
        RenderContext::Present();
    }
}