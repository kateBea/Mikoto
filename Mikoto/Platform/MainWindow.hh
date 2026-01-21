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

#ifndef MIKOTO_MAIN_WINDOW_HH
#define MIKOTO_MAIN_WINDOW_HH

#include <any>
#include <atomic>

#include <volk.h>
#include <GLFW/glfw3.h>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Platform/Window.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseCodes.hh>

namespace Mikoto {

    class MainWindow final : public Window {
    public:
        explicit MainWindow( const WindowProperties& properties );

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto ProcessEvents() -> void override;

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

        MKT_NODISCARD auto GetNativeWindow() const -> std::any override { return m_Window; }

        ~MainWindow() override = default;

    private:
        // [Internal usage]
        auto SetBasicHints() -> void;
        auto InstallCallbacks() -> void;
        auto SetCustomTitle() -> void;

        auto MoveToMonitorCenter() const -> void;

        auto CreateNativeHandle() -> void;

    private:
        // To restore dimensions on exiting full screen mode
        Int32 m_WidthPreFullScreen{};
        Int32 m_HeightPreFullScreen{};

        GLFWwindow* m_Window{};
    };
}

#endif// MIKOTO_MAIN_WINDOW_HH
