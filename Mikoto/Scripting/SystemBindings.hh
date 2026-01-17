//
// Created by kate on 1/17/26.
//

#ifndef MIKOTO_SYSTEM_BINDING_HH
#define MIKOTO_SYSTEM_BINDING_HH

#include <sol/sol.hpp>
#include <Scripting/ScriptingBinding.hh>

namespace Mikoto {
    class SystemBinding final : public ScriptingBinding {
    public:

        auto Init( sol::state &state ) -> void override;
    };
}

#endif// MIKOTO_SYSTEM_BINDING_HH