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
#include "Core/Event.hh"
#include "Platform/Window.hh"
#include "Random.hh"
#include "Singleton.hh"

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
        std::vector<std::string> CommandLineArguments{}; /**< Command-line arguments. */
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


    protected:
        /**
         * @brief Initializes this application. Must call once when
         * the application is created. This operation is responsible
         * of initializing the application internal structures like
         * required layers amongst other subsystems.
         * @param appSpec Specification for application initialization.
         * */
        virtual auto Init(AppSpec&& appSpec) -> bool = 0;


        /**
         * @brief Shuts down this application. Call once
         * to terminate the application and free all of its
         * resources.
         * */
        virtual auto Shutdown() -> void = 0;


        /**
         * @brief Processes and handles application pending events.
         * */
        virtual auto ProcessEvents() -> void = 0;


        /**
         * @brief Updates the application state.
         * */
        virtual auto UpdateState() -> void = 0;


        /**
         * @brief Checks if the application is running.
         * @returns True if the application is running, false otherwise.
         * */
        auto IsRunning() -> bool {
            return m_State == Status::RUNNING || m_State == Status::IDLE;
        }

    protected:
        /**
         * @brief Represents the current state of this application.
         * */
        enum class Status {
            RUNNING,  /**< Application is running. */
            STOPPED,  /**< Application has stopped. */
            IDLE,     /**< Application is idle. */
        };

        /**< Globally unique identifier for the application. Used to subscribe to events. */
        Random::GUID::UUID m_Guid{};

        /**< Pointer to the main window. */
        std::shared_ptr<Window> m_MainWindow{};

        /**< Specification for application initialization. */
        AppSpec m_Spec{};

        /**< Current state of the application. */
        Status m_State{ Status::RUNNING };
    };
}


#endif // MIKOTO_APPLICATION_HH
