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

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::filesystem;

    class Script final : public IResource {
    public:
        explicit Script(FileHandle file, sol::state& state, Entity* entity);

        auto Update( float dt ) -> void;

        auto ReloadScript(sol::state& state) -> void;

        auto SetEnable(bool value) -> void;

        MKT_NODISCARD auto IsEnabled() const -> bool;
        MKT_NODISCARD auto GetFile() const -> FileHandle;

        ~Script() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto OnCreate() -> void;
        auto OnUpdate(float dt) -> void;

    private:

        Entity* mEntity{};

        sol::state* mState{};
        FileHandle mFile{};

        bool mEnabled{ false };

        sol::table mObject{};

        sol::function mOnCreate{};
        sol::function mOnUpdate{};
    };

    using ScriptHandle = Ref<Script>;

}

#endif//MIKOTO_SCRIPT_HH
