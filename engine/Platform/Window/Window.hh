/**
 * Window.hh
 * Created by kate on 5/26/23.
 * */
#ifndef MIKOTO_WINDOW_HH
#define MIKOTO_WINDOW_HH

// C++ Standard Library
#include <any>
#include <string>
#include <string_view>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

// Project Headers
#include <Utility/Types.hh>
#include <Utility/Common.hh>
#include <Core/Events/Event.hh>
#include <Renderer/RenderingUtilities.hh>

namespace Mikoto {
    class WindowProperties {
    public:
        explicit WindowProperties(std::string_view name = "Mikoto Engine", GraphicsAPI backend = GraphicsAPI::VULKAN_API, Int32_T width = 1920, Int32_T height = 1080)
            :   m_Title{ name }, m_Width{ width }, m_Height{ height }, m_Backend{ backend }
        {}

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Title; }
        MKT_NODISCARD auto GetWidth() const -> Int32_T { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }
        MKT_NODISCARD auto GetBackend() const -> GraphicsAPI { return m_Backend; }

        auto SetWidth(Int32_T width) -> void { m_Width = width; }
        auto SetHeight(Int32_T height) -> void { m_Height = height; }
        auto SetTitle(std::string_view name) -> void { m_Title = name; }
        auto SetBackend(GraphicsAPI api) -> void { m_Backend = api; }

    private:
        std::string m_Title{};
        Int32_T m_Width{};
        Int32_T m_Height{};
        GraphicsAPI m_Backend{};
    };

    /**
     * General interface for desktop Windows. We may define different types of
     * windows depending on the platform if extra platform support is necessary
     * to create a context for a specific graphics API.
     *
     * Important to initialize and terminate the windows explicitly for now by explicit calls
     * to <code>Init()</code> and <code>ShutDown()</code>, this allows for more flexibility as to
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

        explicit Window(WindowProperties&& props = WindowProperties{})
            :   m_Properties{ std::move( props )}, m_WindowCreateSuccess{ false } {}

        /**
         * Returns the width of this window
         * @returns the width of the implicit parameter
         * */
        MKT_NODISCARD auto GetWidth() const -> Int32_T { return m_Properties.GetWidth(); }

        /**
         * Returns the height of this window
         * @returns the height of the implicit parameter
         * */
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Properties.GetHeight(); }

        /**
         * Returns the width and height of this window
         * @returns the width and height of the implicit parameter
         * */
        MKT_NODISCARD auto GetExtent() const -> std::pair<Int32_T, Int32_T> { return { GetWidth(), GetHeight() }; }

        /**
         * Returns a handle to the native Window
         * @returns handle to implemented window
         * */
        MKT_NODISCARD virtual auto GetNativeWindow() -> std::any = 0;

        /**
         * Initializes this window along with its internal required structures.
         * This function must be called once right after the window constructor has been created.
         * */
        virtual auto Init() -> void = 0;
        virtual auto OnUpdate() -> void = 0;
        virtual auto ShutDown() -> void = 0;
        virtual auto SetEventCallback(EventCallbackFunc_T func) -> void = 0;

        virtual ~Window() = default;
    public:
        /*************************************************************
         * DELETED OPERATIONS
         * ***********************************************************/
        Window(const Window&)           = delete;
        auto operator=(const Window&)   = delete;

        Window(Window&&)                = delete;
        auto operator=(Window&&)        = delete;

    protected:
        /*************************************************************
         * DATA MEMBERS
         * ***********************************************************/
        WindowProperties m_Properties{};
        bool m_WindowCreateSuccess{};
    };
}

#endif // MIKOTO_WINDOW_HH
