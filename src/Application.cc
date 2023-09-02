/**
 * Application.cc
 * Created by kate on 5/25/23.
 * */

// C++ Standard Library
#include <memory>
#include <cmath>

// Project headers
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Core/GUIManager.hh>
#include <Core/Application.hh>
#include <Renderer/Renderer.hh>
#include <Core/Events/AppEvents.hh>
#include <Core/ImGui/ImGuiLayer.hh>
#include <Platform/InputManager.hh>
#include <Renderer/RenderCommand.hh>
#include <Platform/Window/MainWindow.hh>

namespace Mikoto {
    // TODO: turn static classes into namespaces
    auto Application::Init() -> void {
        // Initialize the time manager
        TimeManager::Init();

        // Allocations
        m_MainWindow = std::make_shared<MainWindow>();
        m_LayerStack = std::make_unique<LayerStack>();
        m_ImGuiLayer = std::make_shared<ImGuiLayer>(); // This should not be done here, should be handled by the gui manager which has an implementation for the gui for opengl and another for vulkan

        // Initialize the main window
        m_MainWindow->Init();
        m_MainWindow->SetEventCallback(MKT_BIND_EVENT_FUNC(Application::OnEvent));

        // Initialize the input manager
        InputManager::Init();

        // Initialize rendering subsystems
        RenderContext::Init(m_MainWindow);
        RenderContext::EnableVSync();
        Renderer::Init();

        // Initialize the GUI layer
        // TODO: turn layer manager into a namespace
        m_LayerStack->Init();
        PushOverlay(m_ImGuiLayer);

        MKT_CORE_LOGGER_DEBUG("Init time {}", TimeManager::GetTime());
    }

    // Should probably not be here
    auto Application::OnEvent(Event& event) -> void {
        MKT_CORE_LOGGER_TRACE("{}", event.DisplayData());

        EventDispatcher evDis{ event };
        if (evDis.Forward<WindowCloseEvent>(MKT_BIND_EVENT_FUNC(Application::OnWindowClose)))
            MKT_CORE_LOGGER_TRACE("HANDLED {}", event.DisplayData());

        if (evDis.Forward<WindowResizedEvent>(MKT_BIND_EVENT_FUNC(Application::OnResizeEvent)))
            MKT_CORE_LOGGER_TRACE("HANDLED {}", event.DisplayData());

        Renderer::OnEvent(event);

        for (auto it{ m_LayerStack->rbegin() }; it != m_LayerStack->rend(); ++it) {
            (*it)->OnEvent(event);
            if (event.IsHandled())
                break;
        }
    }

    auto Application::OnWindowClose([[maybe_unused]] WindowCloseEvent& event) -> bool {
        m_State = State::STOPPED;

        // We don't want to propagate this event
        return true;
    }

    auto Application::OnResizeEvent(WindowResizedEvent& event) -> bool {
        m_MainWindowMinimized = event.GetWidth() == 0 || event.GetHeight() == 0;
        m_State = m_MainWindowMinimized ? State::IDLE : State::RUNNING;
        RenderCommand::UpdateViewPort(0, 0, event.GetWidth(), event.GetHeight());
        return false;
    }

    auto Application::PushLayer(const std::shared_ptr<Layer>& layer) -> void {
        m_LayerStack->AddLayer(layer);
    }

    auto Application::PushOverlay(const std::shared_ptr<Layer>& overlay) -> void {
        m_LayerStack->AddOverlay(overlay);
        overlay->OnAttach();
    }

    auto Application::ShutDown() -> void {
        MKT_CORE_LOGGER_INFO("Shutting down Mikoto Engine");

        m_LayerStack->PopOverlay(m_ImGuiLayer);
        m_ImGuiLayer->OnDetach();

        InputManager::ShutDown();
        Renderer::ShutDown();

        m_LayerStack->ShutDown();
        m_MainWindow->ShutDown();
    }

    auto Application::GetMainWindow() -> Window& {
        MKT_ASSERT(m_MainWindow, "Application main window is NULL");
        return *m_MainWindow;
    }

    auto Application::IsRunning() -> bool {
        return m_State == State::RUNNING || m_State == State::IDLE;
    }

    auto Application::UpdateState() -> void {
        TimeManager::UpdateDeltaTime();

        if (!m_MainWindowMinimized) {
            auto ts{ TimeManager::GetDeltaTime() };
            for (auto& layer : *m_LayerStack)
                layer->OnUpdate(ts);
        }

        m_ImGuiLayer->BeginFrame();
        for (auto& layer : *m_LayerStack)
            layer->OnImGuiRender();
        m_ImGuiLayer->EndFrame();

        m_MainWindow->OnUpdate();
    }

    auto Application::Stop() -> void {
        m_State = State::STOPPED;
    }

    auto Application::BlockImGuiLayerEvents(bool value) -> void {
        m_ImGuiLayer->SetBlockEvents(value);
    }

    auto Application::GetMainWindowPtr() -> std::shared_ptr<Window> {
        return m_MainWindow;
    }
}