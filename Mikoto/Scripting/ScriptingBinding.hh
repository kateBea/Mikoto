//
// Created by kate on 1/17/26.
//

#ifndef MIKOTO_SCRIPTING_BINDING_HH
#define MIKOTO_SCRIPTING_BINDING_HH

namespace Mikoto {
    class ScriptingBinding {
    public:
        virtual ~ScriptingBinding() = default;

        virtual auto Init(sol::state& state) -> void = 0;

    };
}


#endif//MIKOTO_SCRIPTING_BINDING_HH
