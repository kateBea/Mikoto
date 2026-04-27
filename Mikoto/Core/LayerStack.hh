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

#ifndef MIKOTO_LAYER_STACK_HH
#define MIKOTO_LAYER_STACK_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Event.hh>
#include <Core/Registry.hh>

namespace mikoto::core {

    class ILayer {
    public:
        explicit ILayer( const eastl::string_view name )
            : mName{ name } {}
        virtual ~ILayer() = default;

        // Add deltaTime parameter
        virtual auto OnUpdate( float deltaTime ) -> void = 0;
        virtual auto OnCreate() -> void = 0;
        virtual auto OnDestroy() -> void = 0;

        // Layers are not subscribers, instead subscribers can propagate
        // events to them through the layer stack. An event could be
        // marked as handled if we do not want to further propagate it down the layer stack
        virtual auto OnEvent( IEvent& event ) -> void = 0;

    private:
        eastl::string mName{ "BaseLayer" };
    };

    class LayerStack final {
    public:
        template<typename LayerType, typename... Args>
        auto PushLayer(Args&&... args) -> LayerType* {
            auto* result{ mLayers.Register<LayerType>(std::forward<Args>(args)...) };
            result->OnCreate();
            return result;
        }

        template<typename LayerType>
        auto PopLayer() -> void {
            if ( auto* result{ mLayers.Get<LayerType>() } ) {
                result->OnDestroy();
                mLayers.Unregister<LayerType>();
            }
        }

        template<typename LayerType>
        auto GetLayer() -> LayerType* {
            return mLayers.Get<LayerType>();
        }

        auto Shutdown() -> void;

        // Call OnUpdate on all registered layers
        auto OnUpdate(float deltaTime) -> void;

        auto OnEvent(IEvent& event) -> void;

        // Iteration over registered layers
        MKT_NODISCARD constexpr auto begin() -> decltype(auto) { return mLayers.begin(); }
        MKT_NODISCARD constexpr auto end() -> decltype(auto) { return mLayers.end(); }
        MKT_NODISCARD constexpr auto begin() const -> decltype(auto) { return mLayers.begin(); }
        MKT_NODISCARD constexpr auto end() const -> decltype(auto) { return mLayers.end(); }

    private:
        Registry<ILayer> mLayers{};
    };
}



#endif //LAYERSTACK_H
