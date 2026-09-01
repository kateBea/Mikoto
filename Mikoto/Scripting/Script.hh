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

#ifndef MIKOTO_SCRIPT_HH
#define MIKOTO_SCRIPT_HH

#include <sol/sol.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ResourcePool.hh>
#include <Core/ReferenceCounted.hh>

#include <Filesystem/File.hh>

#include <Scene/Entity.hh>

namespace mikoto::scripting {

    class Script final : public core::IResource {
    public:
        explicit Script(filesystem::FileHandle file, sol::state& state, scene::Entity* entity);

        auto Update( float dt ) -> void;

        auto ReloadScript(sol::state& state) -> void;

        MKT_NODISCARD auto GetFile() const -> filesystem::FileHandle;
        MKT_NODISCARD auto GetEntity() const -> scene::Entity*;

        ~Script() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto OnCreate() -> void;
        auto OnUpdate(float dt) -> void;

    private:

        scene::Entity* mEntity{};
        filesystem::FileHandle mFile{};

        sol::state* mState{};

        sol::table mObject{};

        sol::function mOnCreate{};
        sol::function mOnUpdate{};
    };

    using ScriptHandle = core::Ref<Script>;

}

#endif//MIKOTO_SCRIPT_HH
