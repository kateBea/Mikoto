//    Copyright 2025 ケイト
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

#include <sol/sol.hpp>
#include <ankerl/unordered_dense.h>

#include <Common/Service.hh>
#include <Common/Singleton.hh>

#include <Library/Data/Registry.hh>
#include <Library/Data/ResourcePool.hh>

#include <Scene/Entity.hh>

#include <Scripting/Script.hh>
#include <Scripting/ScriptingBinding.hh>

namespace Mikoto {
    struct ScriptingServiceDescription {
    };

    class ScriptingService final : public IService, public Singleton<ScriptingService> {
    public:
        explicit ScriptingService( const ScriptingServiceDescription& config );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto Update(float timeStep) -> void override;

        auto LoadScript(const Path& path, Entity* entity) -> ScriptHandle;

    private:

        auto InitState() -> void;
        auto InitBindings() -> void;

    private:
        sol::state m_LuaState{};

        Registry<ScriptingBinding> m_Bindings{};
        ResourcePoolTyped<Script> m_ScriptPool{};

        ankerl::unordered_dense::map<std::string, std::vector<ScriptHandle>> m_Scripts{};
    };
}

#endif//MIKOTO_SCRIPTING_SERVICE_HH
