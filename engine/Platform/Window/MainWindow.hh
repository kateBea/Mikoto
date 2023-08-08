/**
 * LinuxWindow.hh
 * Created by kate on 5/26/23.
 * */

#ifndef KATE_ENGINE_LINUX_WINDOW_HH
#define KATE_ENGINE_LINUX_WINDOW_HH

// C++ Standard Library
#include <any>

// Third-Party Libraries
#include <volk.h>
#include <GLFW/glfw3.h>

// Project Headers
#include <Utility/Common.hh>

#include <Platform/Window/Window.hh>

#include <Renderer/RenderContext.hh>

namespace kaTe {
    /**
     * Tells whether the GLFW was initialized successfully.
     * Needed before creating windows
     * */
    inline bool g_GLFWInitSuccess{ false };

    /**
     * Window specialization for Linux.
     * */
    class MainWindow : public Window {
    public:
        explicit MainWindow(const WindowProperties& properties = WindowProperties{});

        KT_NODISCARD auto GetWidth() const -> Int32_T override { return m_Properties.GetWidth(); }
        KT_NODISCARD auto GetHeight() const -> Int32_T override { return m_Properties.GetHeight(); }
        KT_NODISCARD auto GetExtent() const -> std::pair<Int32_T, Int32_T> override { return { GetWidth(), GetHeight() }; }

        /**
         * Returns a pointer to a structure containing the
         * native Window structure
         * */
        KT_NODISCARD auto GetNativeWindow() -> std::any override { return m_Window; }

        auto Init() -> void override;
        auto OnUpdate() -> void override;
        auto ShutDown() -> void override;
        auto SetEventCallback(EventCallbackFunc_T func) -> void override { m_Callback = func; }

        KT_NODISCARD auto IsVSyncEnabled() const -> bool { return RenderContext::IsVSyncActive(); }

        auto CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) -> void;

        ~MainWindow() override = default;
    private:
        auto SetCallbacks() -> void;
        auto SpawnOnCenter() const -> void;
        static auto InitGLFW() -> void;

    private:
        GLFWwindow* m_Window{};
        EventCallbackFunc_T m_Callback{};
        bool m_CurrentGraphicsAPIIsOpenGL{ false };
    };

}

#endif // KATE_ENGINE_LINUX_WINDOW_HH
