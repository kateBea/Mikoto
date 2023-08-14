/**
 * Window.hh
 * Created by kate on 5/26/23.
 * */
#ifndef KATE_ENGINE_WINDOW_HH
#define KATE_ENGINE_WINDOW_HH

// C++ Standard Library
#include <string>
#include <string_view>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <any>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Events/Event.hh>

namespace Mikoto {
    class WindowProperties {
    public:
        explicit WindowProperties(std::string_view name = "Mikoto Engine", Int32_T width = 1920, Int32_T height = 1080)
            :   m_Title{ name }, m_Width{ width }, m_Height{ height }
        {}

        KT_NODISCARD auto GetName() const -> const std::string& { return m_Title; }
        KT_NODISCARD auto GetWidth() const -> Int32_T { return m_Width; }
        KT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }

        auto SetWidth(Int32_T width) -> void { m_Width = width; }
        auto SetHeight(Int32_T height) -> void { m_Height = height; }
        auto SetTitle(std::string_view name) -> void { m_Title = name; }

    private:
        std::string m_Title{};
        Int32_T    m_Width{};
        Int32_T    m_Height{};
    };

    /**
    * General interface for desktop Windows. We may define different types of
    * windows depending on the platform if extra platform support is necessary
     * to create a context for a specific graphics API.
     *
     * Important to initialize and terminate the windows explicitly for now by explicit calls
     * to <code>init()</code> and <code>ShutDown()</code>, this allows for more flexibility as to
     * when an entity has to free its resources amongst other termination operations
     *
     * A single instance of Window manages a single window, hence why the copy
     * operations are disabled and move semantics are enabled, this way if we attempt
     * to copy a Window we will get a compile error to ensure this constrain, what we can do instead
     * is moving ownership of a Window resources from one to another by using move semantics
    * */
    class Window {
    public:
        using EventCallbackFunc_T = std::function<void(Event&)>;

        explicit Window([[maybe_unused]] const WindowProperties& props = WindowProperties{})
            :   m_WindowCreateSuccess{ false } {}

        KT_NODISCARD virtual auto GetWidth() const -> Int32_T = 0;
        KT_NODISCARD virtual auto GetHeight() const -> Int32_T = 0;
        KT_NODISCARD virtual auto GetExtent() const -> std::pair<Int32_T, Int32_T> = 0;

        KT_NODISCARD virtual auto GetNativeWindow() -> std::any = 0;

        virtual auto Init() -> void = 0;
        virtual auto OnUpdate() -> void = 0;
        virtual auto ShutDown() -> void = 0;
        virtual auto SetEventCallback(EventCallbackFunc_T func) -> void = 0;

        virtual ~Window() = default;
    public:
        Window(const Window&) = delete;
        auto operator=(const Window&) noexcept -> Window& = delete;

        Window(Window&&) = delete;
        auto operator=(Window&&) noexcept -> Window& = delete;
    protected:
        WindowProperties    m_Properties{};
        bool                m_WindowCreateSuccess{};
    };

}   // END NAMESPACE Mikoto

#endif // KATE_ENGINE_WINDOW_HH
