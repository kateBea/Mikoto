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
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>

namespace Mikoto {
    /**
     * @brief Represents properties for a generic window.
     * */
    class WindowProperties {
    public:
        /**
         * @brief Constructs window properties with default values.
         * @param name    The name/title of the window. Defaults to "Mikoto".
         * @param backend The graphics backend for the window. Defaults to Vulkan.
         * @param width   The width of the window. Default is 1920.
         * @param height  The height of the window. Default is 1080.
         * */
        explicit WindowProperties(std::string_view  name = "Mikoto Engine",
                                   GraphicsAPI      backend = GraphicsAPI::VULKAN_API,
                                   Int32_T          width = 1920,
                                   Int32_T          height = 1080)
            :   m_Title{ name }, m_Width{ width }, m_Height{ height }, m_Backend{ backend }
        {

        }


        /**
         * @brief Retrieves the title of the window.
         * @returns A constant reference to the string containing this window's title.
         * */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Title; }


        /**
         * @brief Retrieves the width of this window.
         * @returns The width of the window.
         * */
        MKT_NODISCARD auto GetWidth() const -> Int32_T { return m_Width; }


        /**
         * @brief Retrieves the height of this window.
         * @returns The height of the window.
         * */
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }


        /**
         * @brief Retrieves the graphics backend used for this window.
         * @returns The graphics backend used for this window.
         * */
        MKT_NODISCARD auto GetBackend() const -> GraphicsAPI { return m_Backend; }


        /**
         * @brief Checks if this window is resizable.
         * @returns True if this window is resizeable, false otherwise.
         * */
        MKT_NODISCARD auto IsResizable() const -> bool { return m_Resizable; }


        /**
         * @brief Sets the width of this window.
         * @param width The width to set for this window.
         * */
        auto SetWidth(Int32_T width) -> void { m_Width = width; }


        /**
         * @brief Sets the height of this window.
         * @param height The new height for this window.
         * */
        auto SetHeight(Int32_T height) -> void { m_Height = height; }


        /**
         * @brief Sets the title of this window.
         * @param name The new title for this window.
         * */
        auto SetTitle(std::string_view name) -> void { m_Title = name; }


        /**
         * @brief Enable or disable resizing for this window.
         * @param value The value indicating whether this window should be resizable.
         * */
        auto AllowResizing(bool value) -> void { m_Resizable = value; }

    private:
        std::string m_Title{};       /**< The title of the window. */
        Int32_T     m_Width{};       /**< The width of the window. */
        Int32_T     m_Height{};      /**< The height of the window. */
        GraphicsAPI m_Backend{};     /**< The graphics backend for the window. */
        bool        m_Resizable{};   /**< Indicates if the window is resizable. */
    };


    /**
     * General interface for desktop Windows. We may define different types of
     * windows depending on the platform if extra platform support is necessary
     * to create a context for a specific graphics API.
     *
     * Important to initialize and terminate the windows explicitly by explicit calls
     * to <code>Init()</code> and <code>Shutdown()</code>, this allows for more flexibility
     * as to when we want to destroy a window or simply hide it and fully dispose of it.
     *
     * A single instance of Window manages a single window, hence why the copy
     * operations are disabled and move semantics are enabled.
     * */
    class Window {
    public:
        /**
         * @brief Constructs this Window with the specified properties.
         * @param props Properties for the window.
         * */
        explicit Window(WindowProperties&& props = WindowProperties{})
            :   m_Properties{ std::move(props) }
        {

        }


        /**
         * @brief Returns the width of this window.
         * @returns The width of the window.
         * */
        MKT_NODISCARD auto GetWidth() const -> Int32_T { return m_Properties.GetWidth(); }


        /**
         * @brief Returns the height of this window.
         * @returns The height of the window.
         * */
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Properties.GetHeight(); }


        /**
         * @brief Returns the title of this window.
         * @returns The title of the window.
         * */
        MKT_NODISCARD auto GetTitle() const -> const std::string& { return m_Properties.GetName(); }


        /**
         * @brief Checks if the window is minimized.
         * @returns A boolean indicating if the window is minimized.
         * */
        MKT_NODISCARD auto IsMinimized() const -> bool { return GetWidth() == 0 || GetHeight() == 0; }


        /**
         * @brief Returns a handle to the native Window structure.
         * @returns Handle to the implemented native window.
         * */
        MKT_NODISCARD virtual auto GetNativeWindow() const -> std::any = 0;


        /**
         * @brief Checks if the window is resizable.
         * @returns A boolean indicating if the window is resizable.
         * */
        MKT_NODISCARD auto IsResizable() const -> bool { return m_Properties.IsResizable(); }


        /**
         * @brief Allows or disallows resizing of the window.
         * @param value The value indicating whether the window should be resizable.
         * */
        auto AllowResizing(bool value) -> void { m_Properties.AllowResizing(value); }


        /**
         * @brief Initializes this window along with its internal required structures.
         * This function must be called once right after the window has been created.
         * */
        virtual auto Init() -> void = 0;


        /**
         * @brief Shuts down this window and releases its associated resources.
         * */
        virtual auto Shutdown() -> void = 0;


        /**
         * @brief Processes pending events for this window.
         * */
        virtual auto ProcessEvents() -> void = 0;


        /**
         * @brief Creates a Window for the currently active platform.
         * @param properties Determines the properties of the window to be created.
         * @returns A pointer to the newly created window.
         * */
        static auto Create( WindowProperties&& properties ) -> std::shared_ptr<Window>;


        /**
         * @brief Default virtual destructor for this Window.
         * */
        virtual ~Window() = default;

    public:
        DELETE_COPY_FOR(Window);

    protected:
        WindowProperties m_Properties{}; /**< Properties for this window. */

    };
}

#endif // MIKOTO_WINDOW_HH
