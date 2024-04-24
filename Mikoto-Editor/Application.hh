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
#include <Common/Random.hh>
#include <Common/Singleton.hh>
#include <Core/Event.hh>
#include <EditorLayer.hh>
#include <Platform/Window.hh>

namespace Mikoto {

    /**
     * @brief Holds application initialization data.
     * This struct is mostly relevant at application initialization.
     * */
    struct AppSpec {
        Int32_T WindowWidth{};      /**< The width of the application window. */
        Int32_T WindowHeight{};     /**< The height of the application window. */

        std::string Name{};         /**< The name of the application. */

        Path_T WorkingDirectory{};  /**< The path to the working directory. */
        Path_T Executable{};        /**< The path to the executable. */

        GraphicsAPI RenderingBackend{}; /**< The selected graphics rendering backend. */

        bool WantGUI{};     /**< Flag indicating if GUI is wanted. */

        std::unordered_set<std::string> CommandLineArguments{}; /**< Command-line arguments. */
    };


    /**
     * @brief Manages the editor application lifecycle.
     * */
    class Application : public Singleton<Application> {
    public:
        /**
         * @brief Constructs an empty application.
         * */
        explicit Application() = default;


        /**
         * @brief Destructs this application after exiting its scope.
         * */
        ~Application() = default;


        /**
         * @brief Initializes this application. Must call once when
         * the application is created. This operation is responsible
         * of initializing the application internal structures like
         * required layers amongst other subsystems.
         * @param appSpec Specification for application initialization.
         * */
        auto Init(AppSpec&& appSpec) -> void;


        /**
         * @brief Shuts down this application. Call once
         * to terminate the application and free all of its
         * resources.
         * */
        auto Shutdown() -> void;


        /**
         * @brief Processes and handles application pending events.
         * */
        auto ProcessEvents() -> void;


        /**
         * @brief Updates the application state.
         * */
        auto UpdateState() -> void;


        /**
         * @brief Checks if the application is running.
         * @returns True if the application is running, false otherwise.
         * */
        auto IsRunning() -> bool;


        /**
         * @brief Presents the current rendered contents to the main window.
         * */
        auto Present() -> void;

    private:
        /**
         * @brief Represents the current state of this application.
         * */
        enum class State {
            RUNNING,  /**< Application is running. */
            STOPPED,  /**< Application has stopped. */
            IDLE,     /**< Application is idle. */
        };

    private:
        /**
         * @brief Initializes layers for the application.
         * */
        auto InitializeLayers() -> void;


        /**
         * @brief Releases layers of the application.
         * */
        auto ReleaseLayers() -> void;


        /**
         * @brief Updates layers of the application.
         * */
        auto UpdateLayers() -> void;


        /**
         * @brief Renders the ImGui frame.
         * */
        auto ImGuiFrameRender() -> void;


        /**
         * @brief Installs event callbacks for the application.
         * */
        auto InstallEventCallbacks() -> void;

    private:
        Random::GUID::UUID           m_Guid{};                   /**< Globally unique identifier for the application. Mostly needed to subscribe to events. */
        std::unique_ptr<EditorLayer> m_EditorLayer{};            /**< Pointer to the editor layer. */

        std::shared_ptr<Window>      m_MainWindow{};             /**< Pointer to the main window. */

        AppSpec                      m_Spec{};                   /**< Specification for application initialization. */
        State                        m_State{ State::RUNNING };  /**< Current state of the application. */
    };
}


#endif // MIKOTO_APPLICATION_HH
