/**
 * Application.hh
 * Created by kate on 5/25/23.
 * */

#ifndef KATE_ENGINE_ENGINE_MANAGER_HH
#define KATE_ENGINE_ENGINE_MANAGER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Singleton.hh>

#include <Core/ImGui/ImGuiLayer.hh>
#include <Core/LayerStack.hh>
#include <Core/Events/AppEvents.hh>
#include <Core/Events/Event.hh>
#include <Core/TimeManager.hh>

#include <Platform/Window/Window.hh>

namespace kaTe {
    /**
     * This class is essentially a wrapper around all the subsystems of our
     * application, and it serves as a way of communicating the different
     * components of ours engine
     * */
    class Application : public Singleton<Application> {
    public:
        auto Init() -> void;
        auto ShutDown() -> void;

        auto OnEvent(Event& event) -> void;

        auto PushLayer(const std::shared_ptr<Layer>& layer) -> void;
        auto PushOverlay(const std::shared_ptr<Layer>& overlay) -> void;

        auto BlockImGuiLayerEvents(bool value) -> void;

        auto UpdateState() -> void;
        auto IsRunning() -> bool;
        auto Stop() -> void;

        auto GetMainWindow() -> Window&;
        auto GetMainWindowPtr() -> std::shared_ptr<Window>;


    private:
        /**
         * Represents the current state of this application
         * */
        enum class State {
            NONE,
            RUNNING,
            STOPPED,
            IDLE,  // can be used when we minimize the main window
            COUNT,
        };

        /*************************************************************
         * APPLICATION EVENT HANDLERS -----------------------------
         * ********************************************************+ */

        /**
         * Callback function for WindowClose event. For now
         * it always returns true as we have no needs to
         * forward this event any further and handler it right away
         * */
        bool OnWindowClose(WindowCloseEvent &event);

        /**
         * Callback function for WindowResizedEvent event. For now
         * it always returns true as we have no needs to
         * forward this event any further and handler it right away
         * */
        bool OnResizeEvent(WindowResizedEvent &event);

        // MEMBER VARIABLES ----------------------------
        bool m_MainWindowMinimized{ false };
        State m_State{ State::RUNNING };

        std::shared_ptr<Window> m_MainWindow{};
        std::unique_ptr<LayerStack> m_LayerStack{};
        std::shared_ptr<ImGuiLayer> m_ImGuiLayer{};

    };
}


#endif//KATE_ENGINE_ENGINE_MANAGER_HH
