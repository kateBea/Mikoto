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

#ifndef MIKOTO_PLATFORM_MAIN_WINDOW_HH
#define MIKOTO_PLATFORM_MAIN_WINDOW_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_events.h>

#include <Platform/Window.hh>
#include <Platform/KeyCodes.hh>

namespace mikoto::platform {

    class MainWindow final : public IWindow, private IOpenGLWindowContext {
    public:
        explicit MainWindow( const WindowProperties& properties );
        
        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;

        auto SetCursorPos( core::float2 pos ) -> void override;
        auto SetScreenMode( ScreenMode mode ) -> void override;
        auto SetCursorMode( CursorMode mode ) -> void override;
        auto SetCursorType( CursorType type ) -> void override;

        auto SetWidth( core::i32 width ) -> void override;
        auto SetHeight( core::i32 height ) -> void override;
        auto SetTitle( eastl::string_view title ) -> void override;
        auto SetIcon( const core::ubyte* data, core::u32 width, core::u32 height ) -> bool override;

        auto Minimize() -> void override;
        auto RequestClose() -> void override;
        auto ToggleMaximized() -> void override;

        auto SetCustomTitleBar( bool enabled ) -> bool override;
        auto SetTitleBarDragArea( core::f32 x, core::f32 y, core::f32 width, core::f32 height ) -> void override;

        auto ResetCursorType() -> void override;
        auto AllowResizing( bool value ) -> void override;

        MKT_NODISCARD auto GetWidth() const -> core::i32 override;
        MKT_NODISCARD auto GetHeight() const -> core::i32 override;
        MKT_NODISCARD auto GetTitle() const -> const eastl::string& override;
        MKT_NODISCARD auto GetScreenMode() const -> ScreenMode override;
        
        MKT_NODISCARD auto GetApi() const -> renderer::rhi::GraphicsAPI override;
        
        MKT_NODISCARD auto IsMinimized() const -> bool override;
        MKT_NODISCARD auto IsMaximized() const -> bool override;
        MKT_NODISCARD auto IsResizable() const -> bool override;
        MKT_NODISCARD auto IsScreenMode( ScreenMode mode ) const -> bool override;
        MKT_NODISCARD auto HasCustomTitleBar() const -> bool override;

        MKT_NODISCARD auto IsApi( renderer::rhi::GraphicsAPI api ) const -> bool override;
        
        MKT_NODISCARD auto IsCursorMode( CursorMode mode ) const -> bool override;
        MKT_NODISCARD auto IsCursorType( CursorType type ) const -> bool override;
        
        MKT_NODISCARD auto GetCursorMode() const -> CursorMode override;
        MKT_NODISCARD auto GetCursorType() const -> CursorType override;

        MKT_NODISCARD auto GetClipBoardContents() const -> eastl::string override;

        MKT_NODISCARD auto IsKeyPressed( platform::KeyCode keyCode ) const -> bool override;
        MKT_NODISCARD auto IsKeyReleased( platform::KeyCode keyCode ) const -> bool override;

        MKT_NODISCARD auto IsMouseKeyPressed( platform::MouseButton button ) const -> bool override;
        MKT_NODISCARD auto IsMouseKeyReleased( platform::MouseButton button ) const -> bool override;

        MKT_NODISCARD auto GetMouseX() const -> core::f64 override;
        MKT_NODISCARD auto GetMouseY() const -> core::f64 override;
        MKT_NODISCARD auto GetMousePos() const -> eastl::pair<core::f64, core::f64> override;

        MKT_NODISCARD auto ShouldClose() const -> bool override;

        MKT_NODISCARD auto GetSystemHandle() const -> void* override;
        MKT_NODISCARD auto GetNativeWindow() const -> eastl::any override;
        MKT_NODISCARD auto GetOpenGLContext() -> IOpenGLWindowContext* override;

        ~MainWindow() override;

    private:
        // [Internal usage]
        friend auto PollWindowEvents() -> void;

        MKT_NODISCARD auto MakeCurrent() -> bool override;
        MKT_NODISCARD auto GetProcAddress( core::cstr name ) const -> Proc override;
        MKT_NODISCARD auto SwapBuffers() -> bool override;
        MKT_NODISCARD auto SetSwapInterval( core::i32 interval ) -> bool override;

        auto RefreshWindowChrome() -> void;
        auto PushEvent( const SDL_Event& event ) -> void;

    private:
        eastl::string mTitle{};

        core::i32 mWidth{};
        core::i32 mHeight{};
        renderer::rhi::GraphicsAPI mBackend{};

        CursorMode mCursorMode{ CursorMode::eNormal };
        CursorType mCursorType{ CursorType::eArrow };
        ScreenMode mScreenMode{ ScreenMode::eWindowed };

        bool mIsResizable{ false };
        bool mIsMinimized{};
        bool mIsMaximized{};

        core::i32 mWidthPreFullScreen{};
        core::i32 mHeightPreFullScreen{};

        SDL_Window* mWindow{};
        SDL_GLContextState* mOpenGLContext{};
        SDL_Cursor* mCursor{};

        bool mShouldClose{};
        bool mCustomTitleBarRequested{};

        core::f64 mMouseX{};
        core::f64 mMouseY{};
        core::i32 mWindowedX{};
        core::i32 mWindowedY{};
        core::float2 mCapturedCursorPosition{};
    };
}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_MAIN_WINDOW_HH
