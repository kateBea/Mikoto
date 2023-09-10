/**
 * LinuxWindow.hh
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
#include <Utility/Common.hh>
#include <Platform/Window/Window.hh>
#include <Renderer/RenderContext.hh>

namespace Mikoto {
    class MainWindow : public Window {
    public:
        explicit MainWindow(WindowProperties&& properties = WindowProperties{});

        MKT_NODISCARD auto GetWidth() const -> Int32_T override { return m_Properties.GetWidth(); }
        MKT_NODISCARD auto GetHeight() const -> Int32_T override { return m_Properties.GetHeight(); }
        MKT_NODISCARD auto GetExtent() const -> std::pair<Int32_T, Int32_T> override { return { GetWidth(), GetHeight() }; }

        /**
         * Returns a handle to the native Window
         * @returns handle to implemented window
         * */
        MKT_NODISCARD auto GetNativeWindow() -> std::any override { return m_Window; }

        auto Init() -> void override;
        auto OnUpdate() -> void override;
        auto ShutDown() -> void override;
        auto SetEventCallback(EventCallbackFunc_T func) -> void override { m_Callback = func; }

        auto CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) -> void;

        ~MainWindow() override = default;
    private:
        auto SetCallbacks() -> void;
        auto SpawnOnCenter() const -> void;
        static auto InitGLFW() -> void;

    private:
        /**
         * Tells whether the GLFW was initialized successfully.
         * Needed before creating windows
         * */
        inline static bool s_GLFWInitSuccess{ false };

        GLFWwindow* m_Window{};
        EventCallbackFunc_T m_Callback{};
    };
}

#endif // MIKOTO_MAIN_WINDOW_HH
