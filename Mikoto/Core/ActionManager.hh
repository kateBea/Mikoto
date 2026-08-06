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

#ifndef MIKOTO_ACTION_MANAGER_HH
#define MIKOTO_ACTION_MANAGER_HH

#include <EASTL/functional.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Event.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Service.hh>
#include <Core/KeyCodes.hh>
#include <Core/Singleton.hh>
#include <Core/EventSystem.hh>

namespace mikoto::core {

    struct ShortcutKey {
        core::KeyCode key;
        i32 modifiers{};

        bool operator==(const ShortcutKey& other) const {
            return key == other.key && modifiers == other.modifiers;
        }
    };

    struct ShortcutKeyHash {
        auto operator()(const ShortcutKey& sk) const -> size_t {
            size_t h1{ eastl::hash<int>{}(sk.key) };
            size_t h2{ eastl::hash<i32>{}(core::as<i32>(sk.modifiers)) };
            return h1 ^ (h2 << 1); // Displace bits to combine hashes
        }
    };

    struct ActionManagerCreateInfo {
    };

    class ActionManager final : public IService {
    public:
        explicit ActionManager(const ActionManagerCreateInfo& option);

        ~ActionManager() override = default;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        using ShortcutAction = eastl::function<void()>;

        MKT_NODISCARD auto Dispatch(core::KeyCode key, core::ModKey mods) const -> bool;
        auto Bind(core::KeyCode key, core::ModKey mods, ShortcutAction action) -> void;

    private:
        ankerl::unordered_dense::map<ShortcutKey, ShortcutAction, ShortcutKeyHash> mActions{};
    };

}

#endif//MIKOTO_ACTION_MANAGER_HH
