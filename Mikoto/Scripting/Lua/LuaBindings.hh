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

#ifndef MIKOTO_SCRIPTING_SCRIPTING_BINDING_HH
#define MIKOTO_SCRIPTING_SCRIPTING_BINDING_HH

#include <sol/sol.hpp>

namespace mikoto::scripting {

    class ScriptingBinding {
    public:
        virtual ~ScriptingBinding() = default;

        virtual auto Init(sol::state& state) -> void = 0;

    };

    class SceneBinding final : public ScriptingBinding {
    public:

        auto Init( sol::state &state ) -> void override;
    };

    class InputBinding final : public ScriptingBinding {
    public:

        auto Init( sol::state &state ) -> void override;
    };

    class MathBinding final : public ScriptingBinding {
    public:

        auto Init( sol::state &state ) -> void override;

    private:
        auto SetupMathTypes( sol::state& state ) -> void;
        auto SetupMathFunctions( sol::state& state ) -> void;
    };

    class SystemBinding final : public ScriptingBinding {
    public:
        auto Init( sol::state &state ) -> void override;
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_SCRIPTING_BINDING_HH
