//
// Created by zanet on 10/10/2025.
//

#include "Core/LayerStack.hh"

#include <ranges>

namespace Mikoto {

    auto LayerStack::Clear() -> void {
        for ( const auto &layerPtr: m_Layers | std::views::values ) {
            layerPtr->OnDestroy();
        }
        m_Layers.Clear();
    }

    auto LayerStack::OnUpdate( float deltaTime ) -> void {
        for ( const auto &layerPtr: m_Layers | std::views::values ) {
            layerPtr->OnUpdate(deltaTime);
        }
    }
}// namespace Mikoto