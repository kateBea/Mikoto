//
// Created by zanet on 10/10/2025.
//

#include "Core/LayerStack.hh"

#include <ranges>

namespace Mikoto {

    auto LayerStack::Shutdown() -> void {
        for ( const auto &layerPtr: m_Layers | std::views::values ) {
            layerPtr->OnDestroy();
        }
        m_Layers.Clear();
    }

    auto LayerStack::OnUpdate( const float deltaTime ) -> void {
        for ( const auto &layerPtr: m_Layers | std::views::values ) {
            layerPtr->OnUpdate( deltaTime );
        }
    }

    auto LayerStack::OnEvent( Event &event ) -> void {
        for ( const auto &layerPtr: m_Layers | std::views::values ) {
            layerPtr->OnEvent( event );
        }
    }
}// namespace Mikoto