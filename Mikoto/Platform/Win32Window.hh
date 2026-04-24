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

#ifndef MIKOTO_WIN32WINDOW_HH
#define MIKOTO_WIN32WINDOW_HH

#include <Core/Platform.hh>
#include <Platform/Window.hh>

// TODO:
// https://youtu.be/D-PC-huX-l8?list=PLqCJpWy5Fohd3S7ICFXwUomYW0Wv67pDD
#if defined(MIKOTO_PLATFORM_WINDOWS) && defined(MKT_USE_WIN32_WINDOW)

// Must go before anything else
// or at least before you include Windows.h
#include <Platform/PlatformWin32.hh>


namespace mikoto::platform {

    class Win32Window final : public Window {
    public:
        explicit Win32Window( const WindowProperties& props );
        ~Win32Window() override;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;

        MKT_NODISCARD auto GetNativeWindow() const -> std::any override { return m_WindowHandle; }

        auto SetScreenMode( ScreenMode mode ) -> void override;

        auto SetCursorMode( CursorMode mode ) -> void override;
        auto SetCursorType( CursorType type ) -> void override;

        auto ResetCursorType() -> void override;

        MKT_NODISCARD auto IsKeyPressed( KeyCode keyCode ) const -> bool override;
        MKT_NODISCARD auto IsKeyReleased( KeyCode keyCode ) const -> bool override;

        MKT_NODISCARD auto IsMouseKeyPressed( MouseButton button ) const -> bool override;
        MKT_NODISCARD auto IsMouseKeyReleased( MouseButton button ) const -> bool override;

        MKT_NODISCARD auto ShouldClose() const -> bool override;

        MKT_NODISCARD auto GetMouseX() const -> double override;
        MKT_NODISCARD auto GetMouseY() const -> double override;
        MKT_NODISCARD auto GetMousePos() const -> std::pair<double, double> override;

        auto SetShouldClose( bool close ) -> void { m_ShouldClose = close; }

    private:
        auto RegisterWindowClass() -> void;
        auto CenterWindow() -> void;

    private:
        HWND m_WindowHandle{ nullptr };
        HINSTANCE m_Instance{ nullptr };
        bool m_ShouldClose{ false };

        Int32 m_PrevW{};
        Int32 m_PrevH{};
    };

} // namespace Mikoto


#endif

#endif//MIKOTO_WIN32WINDOW_HH
