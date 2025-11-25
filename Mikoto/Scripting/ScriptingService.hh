//
// Created by zanet on 4/16/2025.
//

#ifndef LUASERVICE_HH
#define LUASERVICE_HH

#include <sol/sol.hpp>

#include <Common/Service.hh>
#include <Common/Singleton.hh>

namespace Mikoto {
    struct ScriptingServiceDescription {
    };

    class ScriptingService final : public IService, public Singleton<ScriptingService> {
    public:
        explicit ScriptingService( const ScriptingServiceDescription& config );

        auto Init() -> void override;
        auto Shutdown() -> void override;

    private:
        sol::state m_LuaState{};
    };
}// namespace Mikoto


#endif//LUASERVICE_HH
