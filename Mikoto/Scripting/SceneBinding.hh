//
// Created by kate on 1/17/26.
//

#ifndef MIKOTO_SCENE_BINDING_HH
#define MIKOTO_SCENE_BINDING_HH

#include <sol/sol.hpp>

#include <Scripting/ScriptingBinding.hh>

namespace Mikoto {
    class SceneBinding final : public ScriptingBinding {
    public:

        auto Init( sol::state &state ) -> void override;
    };
}


#endif // MIKOTO_SCENE_BINDING_HH
