// C++ Standard Library
#include <memory>
#include <cmath>

// Project headers
#include <Utility/Common.hh>
#include <Core/Events/AppEvents.hh>
#include <Core/Logger.hh>
#include <Core/Application.hh>
#include <Core/ImGui/ImGuiLayer.hh>
#include <Platform/InputManager.hh>
#include <Platform/Window/MainWindow.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderCommand.hh>


// TODO: rename all constants to use upper cases
namespace Mikoto {
    auto Application::Init() -> void {
        TimeManager::Init();
        KATE_CORE_LOGGER_DEBUG("Initializing Mikoto Engine {}", TimeManager::ToString(TimeManager::GetTime()));
        m_MainWindow = std::make_shared<MainWindow>();
        m_LayerStack = std::make_unique<LayerStack>();

        m_MainWindow->Init();
        m_MainWindow->SetEventCallback(KT_BIND_EVENT_FUNC(Application::OnEvent));

        RenderContext::Init(m_MainWindow);
        RenderContext::EnableVSync();

        InputManager::Init();

        Renderer::Init();

        m_LayerStack->Init();
        m_ImGuiLayer = std::make_shared<ImGuiLayer>();
        PushOverlay(m_ImGuiLayer);

        KATE_CORE_LOGGER_DEBUG("Finished Mikoto Engine initialization {}", TimeManager::ToString(TimeManager::GetTime()));
    }

    // Should probably not be here
    auto Application::OnEvent(Event& event) -> void {
        KATE_CORE_LOGGER_TRACE("{}", event.DisplayData());

        EventDispatcher evDis{ event };
        if (evDis.Forward<WindowCloseEvent>(KT_BIND_EVENT_FUNC(Application::OnWindowClose)))
            KATE_CORE_LOGGER_TRACE("HANDLED {}", event.DisplayData());

        if (evDis.Forward<WindowResizedEvent>(KT_BIND_EVENT_FUNC(Application::OnResizeEvent)))
            KATE_CORE_LOGGER_TRACE("HANDLED {}", event.DisplayData());

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
        KATE_CORE_LOGGER_INFO("Shutting down Mikoto Engine");

        m_LayerStack->PopOverlay(m_ImGuiLayer);
        m_ImGuiLayer->OnDetach();

        InputManager::ShutDown();
        RenderCommand::ShutDown();
        Renderer::ShutDown();

        m_LayerStack->ShutDown();
        m_MainWindow->ShutDown();
    }

    auto Application::GetMainWindow() -> Window& {
        KT_ASSERT(m_MainWindow, "Application main window is NULL");
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