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
#include <Core/Logger.hh>
#include <Core/Application.hh>
#include <Core/Events/AppEvents.hh>
#include <Platform/InputManager.hh>
#include <Platform/Window/MainWindow.hh>
#include <Renderer/RenderCommand.hh>
#include <Renderer/Renderer.hh>
#include <Editor/EditorLayer.hh>

namespace Mikoto {
    auto Application::Init(AppSpec&& appSpec) -> void {
        // Initialize the time manager
        TimeManager::Init();

        m_Spec = std::move(appSpec);

        MKT_APP_LOGGER_INFO("Program executable (absolute path): {}", m_Spec.Executable.string());
        MKT_APP_LOGGER_INFO("Program current working directory (absolute path): {}", m_Spec.WorkingDirectory.string());

        WindowProperties windowProperties{};
        windowProperties.SetTitle(m_Spec.Name);
        windowProperties.SetWidth(m_Spec.WindowWidth);
        windowProperties.SetHeight(m_Spec.WindowHeight);
        windowProperties.SetBackend(m_Spec.RenderingBackend);

        m_MainWindow = std::make_shared<MainWindow>(std::move(windowProperties));
        m_LayerStack = std::make_unique<LayerStack>();
        m_ImGuiLayer = std::make_shared<ImGuiLayer>();

        m_MainWindow->Init();

        // Application::OnEvent will be called everytime there's an event from the window
        m_MainWindow->SetEventCallback(MKT_BIND_EVENT_FUNC(Application::OnEvent));

        // Initialize the input manager
        InputManager::Init();

        // Initialize rendering subsystems
        RendererSpec renderSpec{};
        renderSpec.Backend = m_Spec.RenderingBackend;

        RenderContextSpec contextSpec{};
        contextSpec.Backend = renderSpec.Backend;
        contextSpec.WindowHandle = GetMainWindowPtr();

        RenderContext::Init(std::move(contextSpec));
        RenderContext::EnableVSync();
        Renderer::Init(std::move(renderSpec));

        PushOverlay(m_ImGuiLayer);

        if (m_Spec.WantEditor)
            PushLayer(std::make_shared<EditorLayer>());

        MKT_APP_LOGGER_INFO("Init time {}", TimeManager::GetTime());
    }

    auto Application::OnEvent(Event& event) -> void {
        MKT_APP_LOGGER_TRACE("{}", event.DisplayData());

        EventDispatcher dispatcher{ event };
        if (dispatcher.Forward<WindowCloseEvent>(MKT_BIND_EVENT_FUNC(Application::OnWindowClose)))
            MKT_APP_LOGGER_TRACE("HANDLED {}", event.DisplayData());

        if (dispatcher.Forward<WindowResizedEvent>(MKT_BIND_EVENT_FUNC(Application::OnResizeEvent)))
            MKT_APP_LOGGER_TRACE("HANDLED {}", event.DisplayData());

        Renderer::OnEvent(event);

        // Propagate events through all layers, starting from the most top one.
        // If a given layer decides to set the event as handled, we no longer propagate it, breaking the loop
        for (auto it{ m_LayerStack->rbegin() }; it != m_LayerStack->rend(); ++it) {
            (*it)->OnEvent(event);

            if (event.IsHandled())
                break;
        }
    }

    auto Application::OnWindowClose(WindowCloseEvent& event) -> bool {
        m_State = State::STOPPED;
        event.SetHandled(true);
        return event.IsHandled();
    }

    auto Application::OnResizeEvent(WindowResizedEvent& event) -> bool {
        m_MainWindowMinimized = event.GetWidth() == 0 || event.GetHeight() == 0;
        m_State = m_MainWindowMinimized ? State::IDLE : State::RUNNING;
        RenderCommand::UpdateViewPort(0, 0, event.GetWidth(), event.GetHeight());

        return event.IsHandled();
    }

    auto Application::PushLayer(const std::shared_ptr<Layer>& layer) -> void {
        m_LayerStack->AddLayer(layer);
        layer->OnAttach();
    }

    auto Application::PushOverlay(const std::shared_ptr<Layer>& overlay) -> void {
        m_LayerStack->AddOverlay(overlay);
        overlay->OnAttach();
    }

    auto Application::ShutDown() -> void {
        MKT_APP_LOGGER_INFO("Shutting down Mikoto Engine");

        for (auto it{ m_LayerStack->rbegin() }; it != m_LayerStack->rend(); ++it) {
            (*it)->OnDetach();
        }

        m_LayerStack->PopOverlay(m_ImGuiLayer);

        InputManager::ShutDown();
        Renderer::ShutDown();

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
        TimeManager::UpdateTimeStep();

        if (!m_MainWindowMinimized) {
            auto ts{ TimeManager::GetTimeStep() };
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

    // TODO: review
    auto Application::BlockImGuiLayerEvents(bool value) -> void {
        m_ImGuiLayer->SetBlockEvents(value);
    }

    auto Application::GetMainWindowPtr() -> std::shared_ptr<Window> {
        return m_MainWindow;
    }
}