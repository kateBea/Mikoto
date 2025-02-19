/**
 * Application.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_APPLICATION_HH
#define MIKOTO_APPLICATION_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Models/Enums.hh>

#include <Common/Singleton.hh>
#include <Library/Random/Random.hh>

namespace Mikoto {

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
        ~Application() override = default;

    protected:
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
        virtual auto UpdateState() -> void = 0;

        /**
         * @brief Checks if the application is running.
         * @returns True if the application is running, false otherwise.
         * */
        MKT_NODISCARD auto IsRunning() const -> bool {
            return m_State == Status::RUNNING || m_State == Status::IDLE;
        }

    protected:
        /** Globally unique identifier for the application. Used to subscribe to events. */
        GlobalUniqueID m_Guid{};

        /** Current state of the application. */
        Status m_State{ Status::RUNNING };
    };
}


#endif // MIKOTO_APPLICATION_HH
