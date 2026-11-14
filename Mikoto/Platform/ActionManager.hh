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

#ifndef MIKOTO_PLATFORM_ACTION_MANAGER_HH
#define MIKOTO_PLATFORM_ACTION_MANAGER_HH

#include <EASTL/string.h>
#include <EASTL/functional.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Profiler.hh>

#include <Platform/Event.hh>
#include <Platform/KeyCodes.hh>
#include <Platform/EventSystem.hh>

namespace mikoto::platform {

    struct ShortcutKey {
        platform::KeyCode key;
        core::i32 modifiers{};

        MKT_NODISCARD auto GetName() const -> eastl::string;
        MKT_NODISCARD auto operator==( const ShortcutKey &other ) const -> bool;
    };

    struct ShortcutKeyHash {
        auto operator()(const ShortcutKey& sk) const -> core::usize;
    };

    struct ActionManagerCreateInfo {
    };

    class ActionManager final : public core::IService {
    public:
        using ShortcutAction = eastl::function<void()>;

        struct ShortcutBinding {
            ShortcutAction mAction{};
            eastl::string mDescription{};
        };

        using ShortcutMap = ankerl::unordered_dense::map<ShortcutKey, ShortcutBinding, ShortcutKeyHash>;

    public:
        explicit ActionManager(const ActionManagerCreateInfo& option);

        ~ActionManager() override = default;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto GetBindings() const -> const ShortcutMap&;
        MKT_NODISCARD auto Dispatch(platform::KeyCode key, platform::ModKey mods) const -> bool;

        auto Bind( platform::KeyCode key, platform::ModKey mods, ShortcutAction action, eastl::string_view description = {} ) -> void;
        auto BindCVarToggle( platform::KeyCode key, platform::ModKey mods, eastl::string_view cvarName, eastl::string_view description = {} ) -> void;

    private:
        ShortcutMap mActions{};
    };

}

#endif// MIKOTO_PLATFORM_ACTION_MANAGER_HH
