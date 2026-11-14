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

#ifndef MIKOTO_SCRIPTING_LUA_SCRIPTING_SERVICE_HH
#define MIKOTO_SCRIPTING_LUA_SCRIPTING_SERVICE_HH

#include <EASTL/vector.h>
#include <EASTL/shared_ptr.h>

#include <sol/sol.hpp>

#include <Core/Service.hh>
#include <Core/Registry.hh>

#include <Scripting/Script.hh>
#include <Scripting/Lua/LuaBindings.hh>


namespace mikoto::scripting {

    /** @brief Owns the Lua VM, engine bindings and editor script reloads. */
    class LuaScriptingService final : public core::ISubsystem {
    public:
        LuaScriptingService();
        ~LuaScriptingService() override;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( float timeStep ) -> void override;

        MKT_NODISCARD auto CreateScript( const filesystem::Path& path ) -> bool;
        MKT_NODISCARD auto LoadScript( const filesystem::Path& path, scene::Entity* entity, bool watch = true ) -> ScriptHandle;

        /** @brief Revokes editor reloads before an entity or its script is replaced. */
        auto ReleaseEntityScripts( scene::Entity* entity ) -> void;

        DISABLE_COPY_AND_MOVE_FOR( LuaScriptingService );

    private:
        auto InitBindings() -> void;
        auto WatchScript( ScriptHandle handle ) -> void;

        struct WatchedScript;
        // Scripts retain the VM until their sol objects have been destroyed.
        eastl::shared_ptr<sol::state> mLuaState{};
        core::Registry<ScriptingBinding> mBindings{};
        eastl::vector<eastl::shared_ptr<WatchedScript>> mScripts{};
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_LUA_SCRIPTING_SERVICE_HH
