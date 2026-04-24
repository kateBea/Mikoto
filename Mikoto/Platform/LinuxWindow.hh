//    Copyright 2025 ケイト
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

#ifndef MIKOTO_PLATFORM_LINUX_LINUX_WINDOW_HH
#define MIKOTO_PLATFORM_LINUX_LINUX_WINDOW_HH

#include <Core/Platform.hh>
#include <Platform/Window.hh>

#if defined(MIKOTO_PLATFORM_LINUX) && defined(MKT_USE_XCB_WINDOW)
#include <xcb/xcb.h>

namespace mikoto::platform {

    class LinuxWindow final : public Window {
    public:
        explicit LinuxWindow( const WindowProperties& props );
        ~LinuxWindow() override;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;

        MKT_NODISCARD auto GetNativeWindow() const -> std::any override;

        auto SetScreenMode( ScreenMode mode ) -> void override;

        auto SetCursorMode( CursorMode mode ) -> void override;
        auto SetCursorType( CursorType type ) -> void override;

        auto ResetCursorType() -> void override;

        MKT_NODISCARD auto IsKeyPressed( KeyCode keyCode ) const -> bool override;
        MKT_NODISCARD auto IsKeyReleased( KeyCode keyCode ) const -> bool override;

        MKT_NODISCARD auto IsMouseKeyPressed( MouseButton button ) const -> bool override;
        MKT_NODISCARD auto IsMouseKeyReleased( MouseButton button ) const -> bool override;

        MKT_NODISCARD auto GetMouseX() const -> double override;
        MKT_NODISCARD auto GetMouseY() const -> double override;
        MKT_NODISCARD auto GetMousePos() const -> std::pair<double, double> override;

        MKT_NODISCARD auto ShouldClose() const -> bool override;

    private:
        xcb_connection_t* m_Connection{ nullptr };
        xcb_window_t m_Window{ 0 };
        xcb_screen_t* m_Screen{ nullptr };
        xcb_intern_atom_reply_t* m_AtomWmDeleteWindow{ nullptr };

        bool m_ShouldClose{ false };
    };

} // namespace Mikoto

#endif

#endif // MIKOTO_PLATFORM_LINUX_LINUX_WINDOW_HH
