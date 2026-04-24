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

#ifndef MIKOTO_SCRIPTING_SERVICE_HH
#define MIKOTO_SCRIPTING_SERVICE_HH

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <sol/sol.hpp>

#include <Core/Registry.hh>
#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>
#include <Core/ResourcePool.hh>

#include <Scene/Entity.hh>

#include <Scripting/Script.hh>
#include <Scripting/ScriptingBinding.hh>

namespace mikoto::scripting {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::filesystem;

    struct ScriptingServiceDescription {
        Path mScriptBasePath{};
    };

    class ScriptingService final : public ISubsystem, public Singleton<ScriptingService> {
    public:
        explicit ScriptingService( const ScriptingServiceDescription& config );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Update(float timeStep) -> void override;

        auto CreateScript( Entity* entity ) -> ScriptHandle;
        auto LoadScript(const Path& path, Entity* entity) -> ScriptHandle;

    private:

        auto InitState() -> void;
        auto InitBindings() -> void;

    private:
        sol::state mLuaState{};

        Path mBasePath{};

        Registry<ScriptingBinding> mBindings{};
        ResourcePoolTyped<Script> mScriptPool{};
    };
}

#endif//MIKOTO_SCRIPTING_SERVICE_HH
