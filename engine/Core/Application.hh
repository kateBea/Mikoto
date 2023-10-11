/**
 * Application.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_APPLICATION_HH
#define MIKOTO_APPLICATION_HH

// C++ Standard Library
#include <memory>
#include <unordered_set>

// Project Headers
#include <Utility/Random.hh>
#include <Utility/Singleton.hh>

#include <Core/Event.hh>
#include <Platform/Window.hh>
#include <Renderer/RenderingUtilities.hh>

#include <Core/TimeManager.hh>

#include <Core/GameLayer.hh>
#include <Core/RendererLayer.hh>
#include <Editor/EditorLayer.hh>

namespace Mikoto {
    /**
     * Holds application initialization data. This
     * struct is mostly relevant at application initialization
     * */
     struct AppSpec {
         Int32_T WindowWidth{};
         Int32_T WindowHeight{};
         std::string Name{};
         Path_T WorkingDirectory{};
         Path_T Executable{};
         GraphicsAPI RenderingBackend{};
         std::unordered_set<std::string> CommandLineArguments{};
         bool WantGUI{};
         bool WantEditor{};
     };

    class Application : public Singleton<Application> {
    public:
        explicit Application() = default;

        auto Init(AppSpec&& appSpec) -> void;
        auto Shutdown() -> void;

        auto ProcessEvents() -> void;

        auto UpdateState() -> void;
        auto IsRunning() -> bool;
        auto Present() -> void;

        auto GetMainWindow() -> Window&;

        ~Application() = default;

    private:
        enum class State {
            RUNNING,
            STOPPED,
            IDLE,
        };

    private:
        auto InitializeLayers() -> void;
        auto ReleaseLayers() -> void;
        auto UpdateLayers() -> void;
        auto ImGuiFrameRender() -> void;
        auto InstallEventCallbacks() -> void;

    private:
        Random::GUID::UUID m_Guid{};

        // Layers
        std::unique_ptr<GameLayer> m_GameLayer{};
        std::unique_ptr<EditorLayer> m_EditorLayer{};
        std::unique_ptr<RendererLayer> m_RendererLayer{};

        // State data
        AppSpec m_Spec{};
        State m_State{ State::RUNNING };
        std::shared_ptr<Window> m_MainWindow{};

        // TODO: Temporary window for testing
        std::shared_ptr<Window> m_SecondaryWindow{};
    };
}


#endif // MIKOTO_APPLICATION_HH
