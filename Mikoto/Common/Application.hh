/**
 * Application.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_APPLICATION_HH
#define MIKOTO_APPLICATION_HH

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Common/Singleton.hh>
#include <Library/Random/Random.hh>

#include <Core/LayerStack.hh>

namespace Mikoto {

    /**
     * @brief Represents the current state of a system.
     * Can be used to control whether it is running, stopped or idling.
     * */
    enum class ApplicationStatus {
        RUNNING,
        STOPPED,
        IDLE,
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
         * @brief Creates and initializes the editor app and runs the main loop.
         * @param argc Argument count.
         * @param argv List of null terminated c-strings command line arguments.
         * */
        virtual auto Run() -> void = 0;

        /**
         * @brief Destructs this application after exiting its scope.
         * */
        ~Application() override = default;

        /**
         * @brief Initializes this application. Must call once when
         * the application is created. Initializes the app.
         * */
        virtual auto Init() -> void = 0;

        /**
         * @brief Shuts down this application. Call once
         * to terminate the application and free all of its
         * resources.
         * */
        virtual auto Shutdown() -> void = 0;

        /**
         * @brief Updates the application state.
         * */
        virtual auto Update() -> void = 0;


        template<typename LayerType, typename... Args>
        auto PushLayer(Args&&... args) -> void {
            m_LayerStack.PushLayer<LayerType>( std::forward<Args>(args)... );
        }

        template<typename LayerType>
        auto PushLayer() -> void {
            m_LayerStack.PopLayer<LayerType>();
        }

        /**
         * @brief Checks if the application is running.
         * @returns True if the application is running, false otherwise.
         * */
        MKT_NODISCARD auto IsRunning() const -> bool {
            return m_State == ApplicationStatus::RUNNING || m_State == ApplicationStatus::IDLE;
        }

    protected:

        /** Current state of the application. */
        ApplicationStatus m_State{ ApplicationStatus::RUNNING };

        LayerStack m_LayerStack{};
    };

}// namespace Mikoto


#endif// MIKOTO_APPLICATION_HH
