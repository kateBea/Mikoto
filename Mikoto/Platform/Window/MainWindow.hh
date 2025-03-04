/**
 * MainWindow.hh
 * Created by kate on 5/26/23.
 * */

#ifndef MIKOTO_MAIN_WINDOW_HH
#define MIKOTO_MAIN_WINDOW_HH

// C++ Standard Library
#include <any>
#include <atomic>

// Third-Party Libraries
#include <GLFW/glfw3.h>
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Platform/Window/Window.hh>

namespace Mikoto {
    /**
     * @brief Represents a platform-agnostic window using GLFW.
     * */
    class MainWindow final : public Window {
    public:
        /**
         * @brief Constructs a GLFW window with given properties.
         *
         * @param properties The properties specifying the window's characteristics.
         * */
        explicit MainWindow( const WindowProperties& properties );


        /**
         * @brief Default destructor for this GLFW window.
         * */
        ~MainWindow() override = default;


        /**
         * @brief Initializes this GLFW window.
         * */
        auto Init() -> void override;


        /**
         * @brief Shuts down this GLFW window.
         * */
        auto Shutdown() -> void override;


        /**
         * @brief Processes events for this GLFW window.
         * */
        auto ProcessEvents() -> void override;

        auto SetScreenMode( ScreenMode mode ) -> void override;


        /**
         * @brief Retrieves the native window as std::any.
         *
         * @return The native window wrapped in std::any.
         * */
        MKT_NODISCARD auto GetNativeWindow() const -> std::any override { return m_Window; }

    private:
        /**
         * @brief Represents specifications for creating a GLFW window.
         *
         * This struct encapsulates specifications for creating a GLFW window,
         * including width, height, and the title of the window.
         * */
        struct MainWindowCreateSpec {
            Int32_T Width{};          /**< The width of the GLFW window. Default is 0. */
            Int32_T Height{};         /**< The height of the GLFW window. Default is 0. */
            std::string Title{}; /**< The title of the GLFW window. Default is an empty string. */
        };


        /**
         * @brief Installs necessary callbacks for the GLFW window.
         * */
        auto InstallCallbacks() -> void;


        /**
         * @brief Spawns the window at the center of the screen.
         * */
        auto MoveToMonitorCenter() const -> void;


        /**
         * @brief Initializes the GLFW library.
         * */
        static auto InitGLFW() -> void;


        /**
         * @brief Destroys the GLFW window.
         *
         * @param window The GLFW window to be destroyed.
         * */
        static auto DestroyGLFWWindow( GLFWwindow *window ) -> void;


        /**
         * @brief Creates a GLFW window based on the given specifications.
         * @param spec Specifications for creating the GLFW window.
         * @return The created GLFW window.
         * */
        static auto Create( const MainWindowCreateSpec &spec ) -> GLFWwindow *;

    private:
        static inline std::atomic_uint32_t s_WindowsCount{ 0 };
        static inline std::atomic_bool s_GLFWInitSuccess{ false };

        Int32_T m_WidthPreFullScreen{};
        Int32_T m_HeightPreFullScreen{};

        GLFWwindow* m_Window{};
    };
}

#endif// MIKOTO_MAIN_WINDOW_HH
