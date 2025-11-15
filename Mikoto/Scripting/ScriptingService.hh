//
// Created by zanet on 4/16/2025.
//

#ifndef LUASERVICE_HH
#define LUASERVICE_HH

// Will temporarily disable lua on windows
// until the build works for this platform properly
#if defined( __linux__ )
#include <sol/sol.hpp>
#endif

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
#if defined( __linux__ )
        sol::state m_LuaState{};
#endif
    };
}// namespace Mikoto


#endif//LUASERVICE_HH
