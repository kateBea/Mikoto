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

        MKT_NODISCARD auto GetNativeWindow() -> std::any override { return m_Window; }

        auto Init() -> void override;
        auto OnUpdate() -> void override;
        auto ShutDown() -> void override;
        auto SetEventCallback(EventCallbackFunc_T func) -> void override { m_Callback = func; }

        auto CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) -> void;

        ~MainWindow() override = default;
    private:
        auto InstallCallbacks() -> void;
        auto SpawnOnCenter() const -> void;
        static auto InitGLFW() -> void;

    private:
        /**
         * Tells whether the GLFW was initialized successfully.
         * Needed before creating windows
         * */
        inline static bool s_GLFWInitSuccess{ false };

        /**
         * Keeps track of the amount of active GLFW windows
         * */
        inline static UInt32_T s_WindowsCount{};

        /**
         * GLFW window handle
         * */
        GLFWwindow* m_Window{};

        /**
         * Function called when and event is triggered
         * */
        EventCallbackFunc_T m_Callback{};
    };
}

#endif // MIKOTO_MAIN_WINDOW_HH
