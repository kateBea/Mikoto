//
// Created by zanet on 10/10/2025.
//

#include <ranges>

#include <Core/Profiler.hh>
#include <Core/LayerStack.hh>


namespace Mikoto {

    auto LayerStack::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( const auto &layerPtr: m_Layers | std::views::values ) {
            layerPtr->OnDestroy();
        }
        m_Layers.Clear();
    }

    auto LayerStack::OnUpdate( const float deltaTime ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( const auto &layerPtr: m_Layers | std::views::values ) {
            layerPtr->OnUpdate( deltaTime );
        }
    }

    auto LayerStack::OnEvent( Event &event ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( const auto &layerPtr: m_Layers | std::views::values ) {
            layerPtr->OnEvent( event );

            if ( event.IsHandled() ) {
                break;
            }
        }
    }
}// namespace Mikoto