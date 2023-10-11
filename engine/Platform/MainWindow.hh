/**
 * LinuxWindow.hh
 * Created by kate on 5/26/23.
 * */

#ifndef MIKOTO_MAIN_WINDOW_HH
#define MIKOTO_MAIN_WINDOW_HH

// C++ Standard Library
#include <any>

// Third-Party Libraries
#include "GLFW/glfw3.h"
#include "volk.h"

// Project Headers
#include "Renderer/RenderContext.hh"
#include "Utility/Common.hh"
#include "Window.hh"

namespace Mikoto {
    class MainWindow : public Window {
    public:
        explicit MainWindow(WindowProperties&& properties = WindowProperties{});

        MKT_NODISCARD auto GetNativeWindow() const -> std::any override { return m_Window; }

        auto Init() -> void override;
        auto Present() -> void override;
        auto Shutdown() -> void override;

        auto CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) -> void;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

        auto ProcessEvents() -> void override;

        ~MainWindow() override = default;
    private:
        auto InstallCallbacks() -> void;
        auto SpawnOnCenter() const -> void;

        struct GLFWWindowCreateSpec {
            Int32_T Width{};
            Int32_T Height{};
            std::string_view Title{};
        };

        static auto InitGLFW() -> void;
        static auto DestroyGLFWWindow(GLFWwindow* window) -> void;
        static auto CreateGLFWWindow(const GLFWWindowCreateSpec& spec) -> GLFWwindow*;

    private:
        inline static bool s_GLFWInitSuccess{ false };
        inline static UInt32_T s_WindowsCount{};
        GLFWwindow* m_Window{};
    };
}

#endif // MIKOTO_MAIN_WINDOW_HH
