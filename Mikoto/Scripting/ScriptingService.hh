//
// Created by zanet on 4/16/2025.
//

#ifndef LUASERVICE_HH
#define LUASERVICE_HH
#include <sol/sol.hpp>
#include <ankerl/unordered_dense.h>

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Data/ResourcePool.hh>
#include <Scripting/Script.hh>
#include <Scene/Entity.hh>
#include "Library/Data/Registry.hh"
#include "ScriptingBinding.hh"

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
}// namespace Mikoto


#endif//LUASERVICE_HH
