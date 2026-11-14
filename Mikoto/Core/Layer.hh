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

#ifndef MIKOTO_CORE_LAYER_HH
#define MIKOTO_CORE_LAYER_HH

#include <EASTL/string_view.h>

#include <Core/Core.hh>

#include <Platform/Event.hh>

namespace mikoto::core {

    class ILayer {
    public:
        virtual auto OnUpdate( float deltaTime ) -> void = 0;
        virtual auto OnRender( float deltaTime ) -> void = 0;
        virtual auto OnCreate() -> void = 0;
        virtual auto OnDestroy() -> void = 0;

        // Layers are not subscribers, instead subscribers can propagate
        // events to them through the layer stack. An event could be
        // marked as handled if we do not want to further propagate it down the layer stack
        virtual auto OnEvent( platform::Event& event ) -> void = 0;

        MKT_NODISCARD virtual auto GetName() const -> eastl::string_view = 0;

        virtual ~ILayer() = default;
    };

}// namespace mikoto::core

#endif// MIKOTO_CORE_LAYER_HH
