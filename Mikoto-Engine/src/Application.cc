/**
 * Application.cc
 * Created by kate on 5/25/23.
 * */

// C++ Standard Library
#include <memory>
#include <cmath>
#include <utility>

// Project headers
#include <Common/Types.hh>
#include <Common/Common.hh>

#include <Core/Logger.hh>
#include <Core/CoreEvents.hh>
#include <Core/Serializer.hh>
#include <Core/Application.hh>
#include <Core/TimeManager.hh>
#include <Core/EventManager.hh>

#include <Assets/AssetsManager.hh>

#include <Threading/TaskManager.hh>

#include <Platform/GlfwWindow.hh>
#include <Platform/InputManager.hh>

#include <Renderer/Renderer.hh>
#include <Renderer/RenderContext.hh>

#include <Scene/SceneManager.hh>

#include <GUI/ImGuiManager.hh>

namespace Mikoto {
    auto Application::Init(AppSpec&& appSpec) -> void {
        // Initialize the time manager
        TimeManager::Init();

        // Initialize workers
        TaskManager::Init();

        m_Spec = std::move(appSpec);

        MKT_APP_LOGGER_INFO("Program executable (absolute path)                 : {}", m_Spec.Executable.string());
        MKT_APP_LOGGER_INFO("Program current working directory (absolute path)  : {}", m_Spec.WorkingDirectory.string());

        // Initialize the main window
        WindowProperties windowProperties{ m_Spec.Name, m_Spec.RenderingBackend, m_Spec.WindowWidth, m_Spec.WindowHeight };
        windowProperties.AllowResizing(true);

        m_MainWindow = std::make_shared<GlfwWindow>(std::move(windowProperties));
        m_MainWindow->Init();

        // Serializer Init
        Serializer::Init();

        // Initialize the input manager
        InputManager::Init(std::addressof(*m_MainWindow));

        RenderContextSpec contextSpec{};
        contextSpec.Backend = m_Spec.RenderingBackend;
        contextSpec.WindowHandle = m_MainWindow;

        // Initialize the render context
        RenderContext::Init(std::move(contextSpec));
        RenderContext::EnableVSync();

        // For multithreading test purposes
        TaskManager::Execute(
                []() -> void {
                    MKT_APP_LOGGER_DEBUG("Hello there I'm another thread, press E to spawn this message again but is probably not going to be me again. We are {} workers in total", TaskManager::GetWorkersCount());
                });

        // Initialize the scene manager
        SceneManager::Init();

        // Initialize the assets' manager. Important to do at the end
        // as it loads some prefabs which require to have a render context ready.
        AssetsManagerSpec assetsManagerSpec{};
        assetsManagerSpec.AssetRootDirectory = "../Assets";

        AssetsManager::Init(std::move(assetsManagerSpec));

        InitializeLayers();

        InstallEventCallbacks();

        MKT_APP_LOGGER_INFO("Init time {}", TimeManager::GetTime());
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
        MKT_APP_LOGGER_INFO("Shutting down application. Cleanup...");
        // Release layers resources
        ReleaseLayers();

        // Assets manager shutdown
        AssetsManager::Shutdown();

        // Scene manager shutdown
        SceneManager::Shutdown();

        // Renderer Context shutdown
        RenderContext::Shutdown();

        // Input Manager shutdown
        InputManager::Shutdown();

        // Event manager shutdown
        EventManager::Shutdown();

        // Serializer manager shutdown
        Serializer::Shutdown();

        // Timer manager shutdown - Not needed for now

        m_MainWindow->Shutdown();

        // Terminate workers and other threads
        TaskManager::Shutdown();
    }

    auto Application::GetMainWindow() -> Window& {
        MKT_ASSERT(m_MainWindow, "Application main window is NULL");
        return *m_MainWindow;
    }

    auto Application::IsRunning() -> bool {
        return m_State == State::RUNNING || m_State == State::IDLE;
    }

    auto Application::ReleaseLayers() -> void {
        m_EditorLayer->OnDetach();
        m_GameLayer->OnDetach();
    }

    auto Application::InitializeLayers() -> void {
        m_GameLayer = std::make_unique<GameLayer>();

        m_GameLayer->OnAttach();

        if (m_Spec.WantEditor) {
            m_EditorLayer = std::make_unique<EditorLayer>();
            m_EditorLayer->OnAttach();
        }
    }

    auto Application::UpdateLayers() -> void {
        auto timeStep{ TimeManager::GetTimeStep() };

        m_EditorLayer->OnUpdate(timeStep);
        m_GameLayer->OnUpdate(timeStep);
    }

    auto Application::ImGuiFrameRender() -> void {
        ImGuiManager::BeginFrame();

        m_EditorLayer->PushImGuiDrawItems();
        m_GameLayer->PushImGuiDrawItems();

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

            // For multithreading test purposes
            if (InputManager::IsKeyPressed(KeyCode::Key_E)) {
                TaskManager::Execute(
                        []() -> void {
                            MKT_APP_LOGGER_DEBUG("Hello there I'm another thread, press E to spawn this message again but is probably not going to be me again. We are {} workers in total", TaskManager::GetWorkersCount());
                        });
            }
        }

        ImGuiFrameRender();

        RenderContext::SubmitFrame();
    }

    auto Application::Present() -> void {
        RenderContext::Present();
    }
}