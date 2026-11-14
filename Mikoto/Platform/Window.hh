//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_PLATFORM_WINDOW_HH
#define MIKOTO_PLATFORM_WINDOW_HH


// error C2039: 'GetDefaultAllocator': is not a member of 'eastl'
#include <EASTL/allocator.h>
#include <EASTL/utility.h>
#include <EASTL/any.h>
#include <EASTL/string.h>
#include <EASTL/type_traits.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/FileSystem.hh>

#include <Platform/KeyCodes.hh>
#include <Platform/MouseCodes.hh>

#include <Renderer/Rhi/Types.hh>

namespace mikoto::platform {

    enum class ScreenMode {
        eFullScreen,
        eWindowed,
        eBorderless,
    };

    enum class CursorMode {
        eNormal,
        eHidden,
        eDisabled
    };

    enum class CursorType {
        eArrow,
        eHand,
        eText,
        eCrossHair,
        eResizeHorizontal,
        eResizeVertical
    };

    /**
     * @brief OpenGL context and presentation operations supplied by a window backend.
     * @details Call these operations on the thread required by the window backend.
     * SDL3 requires the main thread for its context operations.
     */
    class IOpenGLWindowContext {
    public:
        using Proc = void (*)();

        /**
         * @brief Makes this context current on the calling thread.
         * @returns True when the context was made current.
         */
        MKT_NODISCARD virtual auto MakeCurrent() -> bool = 0;

        /**
         * @brief Resolves a GL function using the current context.
         * @param name Null-terminated procedure name.
         * @returns The function address, or nullptr when unavailable.
         */
        MKT_NODISCARD virtual auto GetProcAddress( core::cstr name ) const -> Proc = 0;

        /**
         * @brief Presents the window's OpenGL back buffer.
         * @returns True on successful presentation.
         */
        MKT_NODISCARD virtual auto SwapBuffers() -> bool = 0;

        /**
         * @brief Sets the swap interval on the current context.
         * @param interval Number of refresh intervals between swaps. Zero disables synchronization.
         * @returns True if the interval was accepted.
         */
        MKT_NODISCARD virtual auto SetSwapInterval( core::i32 interval ) -> bool = 0;

        virtual ~IOpenGLWindowContext() = default;
    };

    struct WindowProperties {
        eastl::string mTitle{};

        core::i32 mWidth{};
        core::i32 mHeight{};

        renderer::rhi::GraphicsAPI mBackend{};

        bool mResizable{ false };
        // Opt in per window. Unsupported platforms retain native decorations.
        bool mCustomTitleBar{ false };
    };

    /**
     * @brief Provides backend-independent window, input and cursor operations.
     * @details Implementations own the native handle and all window state.
     * Construct and initialize a concrete window through WindowsService.
     */
    class IWindow {
    public:
        /**
         * @brief Requests a new window width.
         * @param width Width to apply to the native window.
         */
        virtual auto SetWidth( core::i32 width ) -> void = 0;


        /**
         * @brief Requests a new window height.
         * @param height Height to apply to the native window.
         */
        virtual auto SetHeight( core::i32 height ) -> void = 0;


        /**
         * @brief Replaces the stored title and updates the native window.
         * @param title Title text copied by the implementation.
         */
        virtual auto SetTitle( eastl::string_view title ) -> void = 0;


        /**
         * @brief Retrieves the current window width.
         * @returns Width maintained by the implementation as native size changes arrive.
         */
        MKT_NODISCARD virtual auto GetWidth() const -> core::i32 = 0;


        /**
         * @brief Retrieves the current window height.
         * @returns Height maintained by the implementation as native size changes arrive.
         */
        MKT_NODISCARD virtual auto GetHeight() const -> core::i32 = 0;


        /**
         * @brief Retrieves the stored window title.
         * @returns Borrowed title reference, valid while the window exists.
         */
        MKT_NODISCARD virtual auto GetTitle() const -> const eastl::string& = 0;


        /**
         * @brief Retrieves the selected display mode.
         * @returns Current windowed, borderless or fullscreen mode.
         */
        MKT_NODISCARD virtual auto GetScreenMode() const -> ScreenMode = 0;


        /**
         * @brief Reports whether the window has no drawable area.
         * @returns True when minimized or when either dimension is zero.
         */
        MKT_NODISCARD virtual auto IsMinimized() const -> bool = 0;


        /**
         * @brief Reports the native maximized state.
         * @returns True when the window is maximized.
         */
        MKT_NODISCARD virtual auto IsMaximized() const -> bool = 0;


        /**
         * @brief Requests that the native window be minimized.
         * @details The minimized state changes when the backend processes the request.
         */
        virtual auto Minimize() -> void = 0;


        /**
         * @brief Restores a maximized window or maximizes a restored window.
         */
        virtual auto ToggleMaximized() -> void = 0;


        /**
         * @brief Tests the selected display mode.
         * @param mode Display mode to compare against.
         * @returns True when the current mode matches @p mode.
         */
        MKT_NODISCARD virtual auto IsScreenMode( ScreenMode mode ) const -> bool = 0;


        /**
         * @brief Reports whether user resizing is enabled.
         * @returns True when the window allows resizing.
         */
        MKT_NODISCARD virtual auto IsResizable() const -> bool = 0;


        /**
         * @brief Reports whether custom title-bar handling is active.
         * @returns True when the backend has installed the custom title bar.
         */
        MKT_NODISCARD virtual auto HasCustomTitleBar() const -> bool = 0;


        /**
         * @brief Enables custom title-bar handling or restores native decorations.
         * @param enabled Whether to use a custom title bar.
         * @returns True when the backend applies the requested state.
         * @pre Call on the window's main thread.
         */
        virtual auto SetCustomTitleBar( bool enabled ) -> bool = 0;


        /**
         * @brief Sets the client rectangle used for dragging a custom title bar.
         * @details An empty rectangle disables dragging. The rectangle is used while
         * custom title-bar handling is active.
         * @param x Left edge as a fraction of the client width.
         * @param y Top edge as a fraction of the client height.
         * @param width Width as a fraction of the client width.
         * @param height Height as a fraction of the client height.
         * @pre Call on the window's main thread.
         */
        virtual auto SetTitleBarDragArea( core::f32 x, core::f32 y, core::f32 width, core::f32 height ) -> void = 0;


        /**
         * @brief Tests the graphics API selected for this window.
         * @param api Graphics API to compare against.
         * @returns True when the selected API matches @p api.
         */
        MKT_NODISCARD virtual auto IsApi( renderer::rhi::GraphicsAPI api ) const -> bool = 0;

        /**
         * @brief Retrieves the graphics API selected at construction.
         * @returns API used to configure the native window for rendering.
         */
        MKT_NODISCARD virtual auto GetApi() const -> renderer::rhi::GraphicsAPI = 0;


        /**
         * @brief Enables or disables user resizing of the native window.
         * @param value Whether resizing should be allowed.
         */
        virtual auto AllowResizing( bool value ) -> void = 0;


        /**
         * @brief Copies the current text clipboard contents.
         * @returns Clipboard text, or an empty string when unavailable.
         */
        MKT_NODISCARD virtual auto GetClipBoardContents() const -> eastl::string = 0;


        /**
         * @brief Requests a windowed, borderless or fullscreen display mode.
         * @param mode Mode to apply; implementations preserve the windowed size.
         */
        virtual auto SetScreenMode( ScreenMode mode ) -> void = 0;


        /**
         * @brief Copies tightly packed RGBA8 pixels into a window icon.
         * @param data RGBA8 data containing width * height tightly packed pixels.
         * @param width Width in pixels.
         * @param height Height in pixels.
         * @returns True when the native icon is updated.
         * @pre Call on the window's main thread.
         */
        virtual auto SetIcon( const core::ubyte* data, core::u32 width, core::u32 height ) -> bool = 0;


        /**
         * @brief Creates the native window and installs its event handling.
         * @details Calling this on an initialized window has no effect.
         */
        virtual auto Init() -> void = 0;


        /**
         * @brief Releases native window resources and associated input focus.
         * @details Calling this after resources have been released has no effect.
         */
        virtual auto Shutdown() -> void = 0;


        /**
         * @brief Pumps pending native events into the platform event system.
         */
        virtual auto ProcessEvents() -> void = 0;


        /**
         * @brief Borrows the window's OpenGL context operations.
         * @returns A context valid until window shutdown, or nullptr when unavailable.
         */
        MKT_NODISCARD virtual auto GetOpenGLContext() -> IOpenGLWindowContext* = 0;


        /**
         * @brief Retrieves the window backend's native handle.
         * @returns Type-erased borrowed handle, such as SDL_Window*.
         */
        MKT_NODISCARD virtual auto GetNativeWindow() const -> eastl::any = 0;


        /**
         * @brief Borrows the operating system handle for the native window.
         * @returns The OS handle, such as an HWND, valid until shutdown, or
         * nullptr when unavailable.
         */
        MKT_NODISCARD virtual auto GetSystemHandle() const -> void* = 0;


        /**
         * @brief Tests whether a keyboard key is currently held.
         * @param keyCode Mikoto key to query.
         * @returns True while the key is pressed.
         */
        MKT_NODISCARD virtual auto IsKeyPressed( platform::KeyCode keyCode ) const -> bool = 0;


        /**
         * @brief Tests whether a keyboard key is currently released.
         * @param keyCode Mikoto key to query.
         * @returns True while the key is not pressed; this is not a release event.
         */
        MKT_NODISCARD virtual auto IsKeyReleased( platform::KeyCode keyCode ) const -> bool = 0;


        /**
         * @brief Tests whether a mouse button is currently held.
         * @param button Mikoto mouse button to query.
         * @returns True while the button is pressed.
         */
        MKT_NODISCARD virtual auto IsMouseKeyPressed( platform::MouseButton button ) const -> bool = 0;


        /**
         * @brief Tests whether a mouse button is currently released.
         * @param button Mikoto mouse button to query.
         * @returns True while the button is not pressed; this is not a release event.
         */
        MKT_NODISCARD virtual auto IsMouseKeyReleased( platform::MouseButton button ) const -> bool = 0;


        /**
         * @brief Retrieves the cursor's horizontal position.
         * @returns Window-space X position, accumulated from relative motion when disabled.
         */
        MKT_NODISCARD virtual auto GetMouseX() const -> core::f64 = 0;


        /**
         * @brief Retrieves the cursor's vertical position.
         * @returns Window-space Y position, accumulated from relative motion when disabled.
         */
        MKT_NODISCARD virtual auto GetMouseY() const -> core::f64 = 0;


        /**
         * @brief Retrieves both coordinates of the cursor position.
         * @returns Window-space X and Y, accumulated from relative motion when disabled.
         */
        MKT_NODISCARD virtual auto GetMousePos() const -> eastl::pair<core::f64, core::f64> = 0;


        /**
         * @brief Reports whether the native window has requested closure.
         * @returns True when a close request is pending.
         */
        MKT_NODISCARD virtual auto ShouldClose() const -> bool = 0;


        /**
         * @brief Queues a close request through the same path as the native close button.
         * @details The application handles the request during event processing.
         */
        virtual auto RequestClose() -> void = 0;


        /**
         * @brief Tests the current cursor visibility and capture mode.
         * @param mode Cursor mode to compare against.
         * @returns True when the current mode matches @p mode.
         */
        MKT_NODISCARD virtual auto IsCursorMode( CursorMode mode ) const -> bool = 0;


        /**
         * @brief Tests the current cursor shape.
         * @param type Cursor shape to compare against.
         * @returns True when the current shape matches @p type.
         */
        MKT_NODISCARD virtual auto IsCursorType( CursorType type ) const -> bool = 0;


        /**
         * @brief Retrieves the cursor visibility and capture mode.
         * @returns Current cursor mode.
         */
        MKT_NODISCARD virtual auto GetCursorMode() const -> CursorMode = 0;


        /**
         * @brief Retrieves the selected cursor shape.
         * @returns Current cursor type.
         */
        MKT_NODISCARD virtual auto GetCursorType() const -> CursorType = 0;


        /**
         * @brief Moves the cursor within the native window.
         * @param pos Requested window-space X and Y position.
         */
        virtual auto SetCursorPos( core::float2 pos ) -> void = 0;


        /**
         * @brief Changes cursor visibility and relative-input capture.
         * @param mode Normal, hidden or disabled cursor mode to apply.
         */
        virtual auto SetCursorMode( CursorMode mode ) -> void = 0;


        /**
         * @brief Changes the native cursor shape.
         * @param type Cursor shape to apply.
         */
        virtual auto SetCursorType( CursorType type ) -> void = 0;


        /**
         * @brief Restores the default arrow cursor shape.
         */
        virtual auto ResetCursorType() -> void = 0;


        /**
         * @brief Destroys a concrete window through its interface.
         */
        virtual ~IWindow() = default;
    };
}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_WINDOW_HH
