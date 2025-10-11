//
// Created by zanet on 10/10/2025.
//

#ifndef LAYERSTACK_H
#define LAYERSTACK_H

#include <string>
#include <string_view>

#include <Library/Data/Registry.hh>

namespace Mikoto {
    class ILayer {
      public:
        explicit ILayer(std::string_view name)
            : m_LayerName{ name } {}
        virtual ~ILayer() = default;

        // Add deltaTime parameter
        virtual auto OnUpdate(float deltaTime) -> void = 0;
        virtual auto OnCreate() -> void = 0;
        virtual auto OnDestroy() -> void = 0;

    private:
        std::string m_LayerName{ "BaseLayer" };
    };

    class LayerStack final {
    public:
        template<typename LayerType, typename... Args>
        auto PushLayer(Args&&... args) -> LayerType* {
            auto* result{ m_Layers.Register<LayerType>(std::forward<Args>(args)...) };
            result->OnCreate();
            return result;
        }

        template<typename LayerType>
        auto PopLayer() -> void {
            if ( auto* result{ m_Layers.Get<LayerType>() } ) {
                result->OnDestroy();
                m_Layers.Unregister<LayerType>();
            }
        }

        template<typename LayerType>
        auto GetLayer() -> LayerType* {
            return m_Layers.Get<LayerType>();
        }

        auto Clear() -> void;

        // Call OnUpdate on all registered layers
        auto OnUpdate(float deltaTime) -> void;

        // Iteration over registered layers
        constexpr auto begin() -> decltype(auto) { return m_Layers.begin(); }
        constexpr auto end() -> decltype(auto) { return m_Layers.end(); }
        constexpr auto begin() const -> decltype(auto) { return m_Layers.begin(); }
        constexpr auto end() const -> decltype(auto) { return m_Layers.end(); }

    private:
        Registry<ILayer> m_Layers{};
    };
}



#endif //LAYERSTACK_H
