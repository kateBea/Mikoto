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

#ifndef MIKOTO_SCRIPTING_LUA_SCRIPT_HH
#define MIKOTO_SCRIPTING_LUA_SCRIPT_HH

#include <EASTL/shared_ptr.h>
#include <EASTL/unique_ptr.h>

#include <sol/sol.hpp>

#include <Scripting/Script.hh>

namespace mikoto::scripting {

    /** @brief Owns one Lua environment and retains its service VM. */
    class LuaScript final : public IScript {
    public:
        explicit LuaScript( filesystem::FileHandle file, eastl::shared_ptr<sol::state> state, scene::Entity *entity );

        ~LuaScript() override;

        auto Start( scene::Scene &scene ) -> void override;
        auto Stop() -> void override;
        auto Update( float timeStep ) -> void override;

        auto ReloadScript() -> void override;

        MKT_NODISCARD auto GetFile() const -> filesystem::FileHandle override;
        MKT_NODISCARD auto GetEntity() const -> scene::Entity * override;
        MKT_NODISCARD auto GetLanguage() const -> ScriptLanguage override;

        MKT_NODISCARD auto GetMetaData() const -> const ScriptClassMetadata& override;

    private:
        auto Initialize() -> void;
        auto ApplyProperties() -> bool;

    private:
        // The VM and its registry references are released together.
        struct LuaInstance;
        eastl::unique_ptr<LuaInstance> mLua{};

        ScriptClassMetadata mScriptMetadata{};

        // File ownership is independent of the borrowed scene entity.
        filesystem::FileHandle mFile{};
        scene::Entity *mEntity{};
        scene::Scene* mScene{};

        bool mRunning{};
        bool mFailed{};
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_LUA_SCRIPT_HH
