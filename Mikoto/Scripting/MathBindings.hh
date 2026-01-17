//
// Created by kate on 1/17/26.
//

#ifndef MIKOTO_MATH_BINDING_HH
#define MIKOTO_MATH_BINDING_HH

#include <sol/sol.hpp>
#include <Scripting/ScriptingBinding.hh>

namespace Mikoto {
    class MathBinding final : public ScriptingBinding {
    public:

        auto Init( sol::state &state ) -> void override;

    private:
        auto SetupMathTypes( sol::state& state ) -> void;
        auto SetupMathFunctions( sol::state& state ) -> void;
    };
}

#endif// MIKOTO_MATH_BINDING_HH
