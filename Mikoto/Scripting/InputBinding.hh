//
// Created by kate on 1/17/26.
//

#ifndef MIKOTO_INPUTBINDING_HH
#define MIKOTO_INPUTBINDING_HH

#include <sol/sol.hpp>

#include <Scripting/ScriptingBinding.hh>

namespace Mikoto {
    class InputBinding final : public ScriptingBinding {
    public:

        auto Init( sol::state &state ) -> void override;
    };
}



#endif//MIKOTO_INPUTBINDING_HH
