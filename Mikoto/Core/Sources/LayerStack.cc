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

#include <ranges>
#include <exception>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/LayerStack.hh>

#include <Platform/Event.hh>

namespace mikoto::core {

    auto LayerStack::OnDestroy() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Every layer gets a cleanup attempt while its services are still alive.
        std::exception_ptr failure{};

        for ( const auto &layerPtr: mLayers | std::views::values ) {
            try {
                layerPtr->OnDestroy();
            } catch ( ... ) {
                if ( !failure ) {
                    failure = std::current_exception();
                }
            }
        }

        mLayers.Clear();

        if ( failure ) {
            std::rethrow_exception( failure );
        }
    }

    auto LayerStack::OnUpdate( f32 deltaTime ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( const auto &layerPtr: mLayers | std::views::values ) {
            layerPtr->OnUpdate( deltaTime );
        }
    }

    auto LayerStack::OnEvent( platform::Event &event ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( const auto &layerPtr: mLayers | std::views::values ) {
            layerPtr->OnEvent( event );

            if ( event.IsHandled() ) {
                break;
            }
        }
    }
}