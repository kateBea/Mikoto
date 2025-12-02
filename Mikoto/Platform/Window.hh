/**
 * Window.hh
 * Created by kate on 5/26/23.
 * */

#ifndef MIKOTO_WINDOW_HH
#define MIKOTO_WINDOW_HH

// C++ Standard Library
#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

// Project Headers
#include <Renderer/Core/RenderUtility.hh>
#include <Common/Common.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseCodes.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class ScreenMode {
        WINDOW_MODE_FULLSCREEN,
        WINDOW_MODE_WINDOWED,
        WINDOW_MODE_BORDERLESS,
    };

    struct WindowProperties {
        std::string Title{};

        Int32 Width{};
        Int32 Height{};

        GraphicsAPI Backend{};

        bool Resizable{ false };
    };

    /**
     * General interface for desktop Windows. We may define different types of
     * windows depending on the platform if extra platform support is necessary
     * to create a context for a specific graphics API.
     *
     * Important to initialize and terminate the windows explicitly by explicit calls
     * to <code>Init()</code> and <code>Shutdown()</code>, this allows for more flexibility
     * as to when we want to destroy a window or just hide it and fully dispose of it.
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
        explicit Window( const WindowProperties& props = WindowProperties{} )
            : m_Properties{ props } {
        }

        auto SetWidth( const Int32 width ) -> void { m_Properties.Width = width; }
        auto SetHeight( const Int32 height ) -> void { m_Properties.Height = height; }

        /**
         * @brief Returns the width of this window.
         * @returns The width of the window.
         * */
        MKT_NODISCARD auto GetWidth() const -> Int32 { return m_Properties.Width; }

        /**
         * @brief Returns the height of this window.
         * @returns The height of the window.
         * */
        MKT_NODISCARD auto GetHeight() const -> Int32 { return m_Properties.Height; }

        /**
         * @brief Returns the title of this window.
         * @returns The title of the window.
         * */
        MKT_NODISCARD auto GetTitle() const -> const std::string& { return m_Properties.Title; }

        MKT_NODISCARD auto GetScreenMode() const -> ScreenMode { return m_ScreenMode; }

        /**
         * @brief Checks if the window is minimized.
         * @returns A boolean indicating if the window is minimized.
         * */
        MKT_NODISCARD auto IsMinimized() const -> bool { return GetWidth() == 0 || GetHeight() == 0; }
        MKT_NODISCARD auto IsMaximized() const -> bool { return m_ScreenMode == ScreenMode::WINDOW_MODE_FULLSCREEN; }

        /**
         * @brief Returns a handle to the native Window structure.
         * @returns Handle to the implemented native window.
         * */
        MKT_NODISCARD virtual auto GetNativeWindow() const -> std::any = 0;

        /**
         * @brief Checks if the window is resizable.
         * @returns A boolean indicating if the window is resizable.
         * */
        MKT_NODISCARD auto IsResizable() const -> bool { return m_Properties.Resizable; }

        /**
         * @brief Allows or disallows resizing of the window.
         * @param value The value indicating whether the window should be resizable.
         * */
        auto AllowResizing( const bool value ) -> void { m_Properties.Resizable = value; }

        virtual auto SetScreenMode( ScreenMode mode ) -> void = 0;

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

         MKT_NODISCARD virtual auto IsKeyPressed( KeyCode keyCode ) const -> bool = 0;
         MKT_NODISCARD virtual auto IsKeyReleased( KeyCode keyCode ) const -> bool = 0;

         MKT_NODISCARD virtual auto IsMouseKeyPressed( MouseButton button ) const -> bool = 0;
         MKT_NODISCARD virtual auto IsMouseKeyReleased( MouseButton button ) const -> bool = 0;

         MKT_NODISCARD virtual auto GetMouseX() const -> double = 0;
         MKT_NODISCARD virtual auto GetMouseY() const -> double = 0;
         MKT_NODISCARD virtual auto GetMousePos() const -> std::pair<double, double> = 0;

        MKT_NODISCARD virtual auto ShouldClose() const -> bool = 0;

        /**
         * @brief Creates a Window for the currently active platform.
         * @param properties Determines the properties of the window to be created.
         * @returns A pointer to the newly created window.
         * */
        static auto Create( const WindowProperties& properties ) -> Window*;

        /**
         * @brief Default virtual destructor for this Window.
         * */
        virtual ~Window() = default;

    public:
        DISABLE_COPY_FOR( Window );

    protected:
        WindowProperties m_Properties{};
        ScreenMode m_ScreenMode{ ScreenMode::WINDOW_MODE_WINDOWED }; /**< The current screen mode for this window. */
    };
}// namespace Mikoto

#endif// MIKOTO_WINDOW_HH
