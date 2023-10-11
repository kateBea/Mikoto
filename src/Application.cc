/**
 * Application.cc
 * Created by kate on 5/25/23.
 * */

// C++ Standard Library
#include <memory>
#include <cmath>
#include <utility>

// Project headers
#include <Utility/Common.hh>
#include <Core/CoreEvents.hh>
#include <Core/Application.hh>
#include <Core/Logger.hh>
#include <Core/Serializer.hh>
#include <Platform/InputManager.hh>
#include <Renderer/RenderCommand.hh>
#include <Renderer/Renderer.hh>
#include <Editor/EditorLayer.hh>
#include <Platform/MainWindow.hh>
#include <Scene/SceneManager.hh>
#include <ImGui/ImGuiManager.hh>
#include <Core/EventManager.hh>

namespace Mikoto {
    auto Application::Init(AppSpec&& appSpec) -> void {
        // Initialize the time manager
        TimeManager::Init();

        m_Spec = std::move(appSpec);

        MKT_APP_LOGGER_INFO("Program executable (absolute path): {}", m_Spec.Executable.string());
        MKT_APP_LOGGER_INFO("Program current working directory (absolute path): {}", m_Spec.WorkingDirectory.string());

        // Initialize the main window
        WindowProperties windowProperties{};
        windowProperties.SetTitle(m_Spec.Name);
        windowProperties.SetWidth(m_Spec.WindowWidth);
        windowProperties.SetHeight(m_Spec.WindowHeight);
        windowProperties.SetBackend(m_Spec.RenderingBackend);
        windowProperties.AllowResizing(true);
        m_MainWindow = std::make_shared<MainWindow>(std::move(windowProperties));

        m_MainWindow->Init();

        //m_SecondaryWindow->SetEventCallback(MKT_BIND_EVENT_FUNC(Application::OnEvent));

        // Serializer Init
        Serializer::Init();

        // Initialize the input manager
        InputManager::Init(std::addressof(*m_MainWindow));

        // Initialize rendering subsystems. To be done in the renderer layer
        RendererSpec renderSpec{};
        renderSpec.Backend = m_Spec.RenderingBackend;

        RenderContextSpec contextSpec{};
        contextSpec.Backend = renderSpec.Backend;
        contextSpec.WindowHandle = m_MainWindow;

        RenderContext::Init(std::move(contextSpec));
        RenderContext::EnableVSync();
        Renderer::Init(std::move(renderSpec));

        ImGuiManager::Init(m_MainWindow);

        InitializeLayers();

        SceneManager::Init();

        InstallEventCallbacks();

        MKT_APP_LOGGER_INFO("Init time {}", TimeManager::GetTime());
    }

    auto Application::InstallEventCallbacks() -> void {
        EventManager::Subscribe(m_Guid.Get(),
            EventType::APP_CLOSE_EVENT,
            [this](Event& event) -> bool
            {
                m_State = State::STOPPED;
                event.SetHandled(true);
                MKT_CORE_LOGGER_WARN("Handled App Event close");
                return false;
            });

        EventManager::Subscribe(m_Guid.Get(),
            EventType::WINDOW_CLOSE_EVENT,
            [this](Event& event) -> bool
            {
                m_State = State::STOPPED;
                event.SetHandled(true);
                MKT_CORE_LOGGER_WARN("Handled Window Event close");
                return false;
            });

        EventManager::Subscribe(m_Guid.Get(),
            EventType::WINDOW_RESIZE_EVENT,
            [this](Event& event) -> bool
            {
                WindowResizedEvent* wre{ static_cast<WindowResizedEvent*>(std::addressof(event)) };

                m_State = m_MainWindow->IsMinimized() ? State::IDLE : State::RUNNING;
                RenderCommand::UpdateViewPort(0, 0, wre->GetWidth(), wre->GetHeight());
                MKT_CORE_LOGGER_WARN("Handled Window Resize Event");
                return false;
            });
    }

    auto Application::Shutdown() -> void {
        MKT_APP_LOGGER_INFO("Shutting down application. Cleanup...");

        ImGuiManager::Shutdown();

        // Release layers resources
        ReleaseLayers();

        // Shutdown subsystems
        Renderer::Shutdown();
        Serializer::Shutdown();
        InputManager::Shutdown();
        EventManager::Shutdown();

        m_MainWindow->Shutdown();
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
        m_RendererLayer->OnDetach();
    }

    auto Application::InitializeLayers() -> void {
        m_GameLayer = std::make_unique<GameLayer>();
        m_RendererLayer = std::make_unique<RendererLayer>();

        m_GameLayer->OnAttach();
        m_RendererLayer->OnAttach();

        if (m_Spec.WantEditor) {
            m_EditorLayer = std::make_unique<EditorLayer>();
            m_EditorLayer->OnAttach();
        }
    }

    auto Application::UpdateLayers() -> void {
        auto timeStep{ TimeManager::GetTimeStep() };

        m_EditorLayer->OnUpdate(timeStep);
        m_GameLayer->OnUpdate(timeStep);
        m_RendererLayer->OnUpdate(timeStep);
    }

    auto Application::ImGuiFrameRender() -> void {
        ImGuiManager::BeginFrame();

        m_EditorLayer->PushImGuiDrawItems();
        m_GameLayer->PushImGuiDrawItems();
        m_RendererLayer->PushImGuiDrawItems();

        ImGuiManager::EndFrame();
    }

    auto Application::ProcessEvents() -> void {
        // Process events currently in the event queue
        m_MainWindow->ProcessEvents();

        EventManager::ProcessEvents();
    }

    auto Application::UpdateState() -> void {
        // Update the time step
        TimeManager::UpdateTimeStep();

        // Update the layers if the window is not minimized (save resources)
        if (!m_MainWindow->IsMinimized()) {
            m_MainWindow->BeginFrame();

            UpdateLayers();
        }

        // Push ImGui elements for render
        ImGuiFrameRender();
    }

    auto Application::Present() -> void {
        // End frame
        m_MainWindow->EndFrame();

        // Swap buffers for presentation
        m_MainWindow->Present();
    }
}