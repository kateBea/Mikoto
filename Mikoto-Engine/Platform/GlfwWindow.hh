/**
 * MainWindow.hh
 * Created by kate on 5/26/23.
 * */

#ifndef MIKOTO_MAIN_WINDOW_HH
#define MIKOTO_MAIN_WINDOW_HH

// C++ Standard Library
#include <any>

// Third-Party Libraries
#include <volk.h>
#include <GLFW/glfw3.h>

// Project Headers
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Platform/Window.hh>

namespace Mikoto {
    /**
     * @brief Represents a window using GLFW as its backend.
     * This class represents a window created using GLFW, inheriting from the Window base class.
     * */
    class GlfwWindow : public Window {
    public:
        /**
         * @brief Constructs a GLFW window with given properties.
         *
         * @param properties The properties specifying the window's characteristics.
         * */
        explicit GlfwWindow(WindowProperties&& properties);

        /**
         * @brief Default destructor for this GLFW window.
         * */
        ~GlfwWindow() override = default;

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

        /**
         * @brief Creates a window surface for Vulkan.
         *
         * @param instance The Vulkan instance to create the surface.
         * @param surface  Pointer to a Vulkan surface handle to be created.
         * */
        auto CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) -> void;

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
        struct GLFWWindowCreateSpec {
            Int32_T Width{};          /**< The width of the GLFW window. Default is 0. */
            Int32_T Height{};         /**< The height of the GLFW window. Default is 0. */
            std::string_view Title{}; /**< The title of the GLFW window. Default is an empty string. */
        };

        /**
         * @brief Installs necessary callbacks for the GLFW window.
         * */
        auto InstallCallbacks() -> void;

        /**
         * @brief Spawns the window at the center of the screen.
         * */
        auto SpawnOnCenter() const -> void;

        /**
         * @brief Initializes the GLFW library.
         * */
        static auto InitGLFW() -> void;

        /**
         * @brief Destroys the GLFW window.
         *
         * @param window The GLFW window to be destroyed.
         * */
        static auto DestroyGLFWWindow(GLFWwindow *window) -> void;

        /**
         * @brief Creates a GLFW window based on the given specifications.
         *
         * @param spec Specifications for creating the GLFW window.
         * @return The created GLFW window.
         * */
        static auto CreateGLFWWindow(const GLFWWindowCreateSpec &spec) -> GLFWwindow *;

    private:
        static inline UInt32_T s_WindowsCount{ 0 };    /**< The count of active GLFW windows. */
        static inline bool s_GLFWInitSuccess{false};   /**< Flag indicating the success of GLFW initialization. */

        GLFWwindow* m_Window{};                        /**< Pointer to the GLFW window. */
    };

}

#endif // MIKOTO_MAIN_WINDOW_HH
