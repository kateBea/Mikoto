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

#ifndef MIKOTO_INPUT_MANAGER_HH
#define MIKOTO_INPUT_MANAGER_HH

#include <memory>
#include <utility>
#include <string>

#include <Common/Common.hh>
#include <Common/Subsystem.hh>
#include <Common/Singleton.hh>

#include <Core/KeyCodes.hh>
#include <Core/MouseCodes.hh>

#include <Platform/Window.hh>

#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct InputServiceCreateInfo {
        Window* MainWindow{ nullptr };
    };

    class InputService final : public Subsystem, public Singleton<InputService> {
    public:
        explicit InputService( const InputServiceCreateInfo& options );

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float timeStep) -> void override;

        MKT_NODISCARD auto IsKeyPressed( KeyCode keyCode ) const -> bool;
        MKT_NODISCARD auto IsKeyReleased( KeyCode keyCode ) const -> bool;

        MKT_NODISCARD auto IsMouseKeyPressed( MouseButton button ) const -> bool;
        MKT_NODISCARD auto IsMouseKeyReleased( MouseButton button ) const -> bool;

        MKT_NODISCARD auto GetMouseX() const -> double;
        MKT_NODISCARD auto GetMouseY() const -> double;
        MKT_NODISCARD auto GetMousePos() const -> std::pair<double, double>;

        MKT_NODISCARD auto GetClipBoardContents() const -> std::string;

        // Returns a list of directories dropped on a window
        MKT_NODISCARD auto GetDroppedPaths(Window* window) const -> std::vector<std::string>;

        auto SetFocus( Window* newHandle ) -> void;

        ~InputService() override = default;

    private:
        Window* m_Handle{ nullptr };
    };
}

#endif// MIKOTO_INPUT_MANAGER_HH