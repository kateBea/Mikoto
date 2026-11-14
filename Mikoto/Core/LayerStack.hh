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

#ifndef MIKOTO_CORE_LAYER_STACK_HH
#define MIKOTO_CORE_LAYER_STACK_HH

#include <Core/Core.hh>
#include <Core/Layer.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Registry.hh>

#include <Platform/Event.hh>

namespace mikoto::core {

    class LayerStack final {
    public:
        template<typename LayerType, typename... Args>
        auto PushLayer(Args&&... args) -> LayerType* {
            MKT_BEGIN_PROFILER();
            auto* result{ mLayers.Register<LayerType>(eastl::forward<Args>(args)...) };
            result->OnCreate();
            return result;
        }

        template<typename LayerType>
        auto PopLayer() -> void {
            MKT_BEGIN_PROFILER();
            if ( auto* result{ mLayers.Get<LayerType>() } ) {
                result->OnDestroy();
                mLayers.Unregister<LayerType>();
            }
        }

        template<typename LayerType>
        auto GetLayer() -> LayerType* {
            MKT_BEGIN_PROFILER();
            return mLayers.Get<LayerType>();
        }

        auto OnDestroy() -> void;
        auto OnUpdate( f32 deltaTime ) -> void;
        auto OnEvent( platform::Event& event ) -> void;

        MKT_NODISCARD constexpr auto begin() -> decltype(auto) { return mLayers.begin(); }
        MKT_NODISCARD constexpr auto end() -> decltype(auto) { return mLayers.end(); }
        MKT_NODISCARD constexpr auto begin() const -> decltype(auto) { return mLayers.begin(); }
        MKT_NODISCARD constexpr auto end() const -> decltype(auto) { return mLayers.end(); }

    private:
        Registry<ILayer> mLayers{};
    };
}

#endif// MIKOTO_CORE_LAYER_STACK_HH
