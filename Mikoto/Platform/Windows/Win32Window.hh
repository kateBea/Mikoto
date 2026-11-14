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


// Must go before anything else
// or at least before you include Windows.h

#ifndef MIKOTO_PLATFORM_WIN32_WINDOW_HH
#define MIKOTO_PLATFORM_WIN32_WINDOW_HH

#include <Core/Platform.hh>
#include <Core/Profiler.hh>

#include <Platform/Window.hh>
#include <Platform/Windows/Windows.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS) && defined(MKT_USE_WIN32_WINDOW)

namespace mikoto::platform {

    class Win32Window final : public IWindow {
    public:
        explicit Win32Window( const WindowProperties& props );
        ~Win32Window() override;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;

        MKT_NODISCARD auto GetNativeWindow() const -> std::any override {
            MKT_BEGIN_PROFILER();
            return m_WindowHandle;
        }

        auto SetScreenMode( ScreenMode mode ) -> void override;

        auto SetCursorMode( CursorMode mode ) -> void override;
        auto SetCursorType( CursorType type ) -> void override;

        auto ResetCursorType() -> void override;

        MKT_NODISCARD auto IsKeyPressed( platform::KeyCode keyCode ) const -> bool override;
        MKT_NODISCARD auto IsKeyReleased( platform::KeyCode keyCode ) const -> bool override;

        MKT_NODISCARD auto IsMouseKeyPressed( platform::MouseButton button ) const -> bool override;
        MKT_NODISCARD auto IsMouseKeyReleased( platform::MouseButton button ) const -> bool override;

        MKT_NODISCARD auto ShouldClose() const -> bool override;

        MKT_NODISCARD auto GetMouseX() const -> double override;
        MKT_NODISCARD auto GetMouseY() const -> double override;
        MKT_NODISCARD auto GetMousePos() const -> std::pair<double, double> override;

    };

} // namespace Mikoto

#endif

#endif// MIKOTO_PLATFORM_WIN32_WINDOW_HH
