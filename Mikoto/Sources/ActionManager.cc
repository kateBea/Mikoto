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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Event.hh>
#include <Core/CoreEvents.hh>
#include <Core/Profiler.hh>
#include <Core/ActionManager.hh>

#include <Logging/Logger.hh>

namespace mikoto::core {

    ActionManager::ActionManager( const ActionManagerCreateInfo & )

    {}

    auto ActionManager::Initialize() -> void {
        MKT_CORE_LOGGER_DEBUG( "Initializing ActionManager" );

        mIsInitialized = true;
    }

    auto ActionManager::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_DEBUG( "Shutting down ActionManager" );

        mActions.clear();
    }

    auto ActionManager::Dispatch( core::KeyCode key, core::ModKey mods ) const -> bool {
        i32 relevantMods{ as<i32>( mods & ( ModKey::eShift | ModKey::eControl | ModKey::eAlt | ModKey::eSuper ) ) };

        ShortcutKey keyCombo{ key, relevantMods };
        auto it{ mActions.find(keyCombo) };
        if (it != mActions.end()) {
            it->second();
            return true;
        }

        return false;
    }

    auto ActionManager::Bind( core::KeyCode key, core::ModKey mods, ShortcutAction action ) -> void {
        ShortcutKey keyCombo{ key, static_cast<i32>(mods) };
        mActions[keyCombo] = action;
    }
}// namespace mikoto::core