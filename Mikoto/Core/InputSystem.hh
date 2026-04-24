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

#ifndef MIKOTO_INPUT_SYSTEM_HH
#define MIKOTO_INPUT_SYSTEM_HH

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseCodes.hh>
#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>

#include <Platform/Window.hh>

namespace mikoto::core {

    struct InputServiceCreateInfo {
        platform::Window* mWindow{ nullptr };
    };

    class InputSystem final : public ISubsystem, public Singleton<InputSystem> {
    public:
        explicit InputSystem( const InputServiceCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update(float timeStep) -> void override;

        MKT_NODISCARD auto IsKeyPressed( KeyCode keyCode ) const -> bool;
        MKT_NODISCARD auto IsKeyReleased( KeyCode keyCode ) const -> bool;

        MKT_NODISCARD auto IsMouseKeyPressed( MouseButton button ) const -> bool;
        MKT_NODISCARD auto IsMouseKeyReleased( MouseButton button ) const -> bool;

        MKT_NODISCARD auto GetMouseX() const -> double;
        MKT_NODISCARD auto GetMouseY() const -> double;
        MKT_NODISCARD auto GetMousePos() const -> eastl::pair<double, double>;

        MKT_NODISCARD auto GetClipBoardContents() const -> eastl::string;

        // Returns a list of directories dropped on a window
        MKT_NODISCARD auto GetDroppedPaths(platform::Window* window) const -> eastl::vector<eastl::string>;

        auto SetFocus( platform::Window* newHandle ) -> void;

        ~InputSystem() override = default;

    private:
        platform::Window* mHandle{ nullptr };
    };
}

#endif// MIKOTO_INPUT_SYSTEM_HH