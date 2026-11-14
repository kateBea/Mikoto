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

#ifndef MIKOTO_SCENE_SCENE_INL
#define MIKOTO_SCENE_SCENE_INL

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>

namespace mikoto::scene {

    template<typename EntityFunction>
    auto Scene::ApplyToChildren( Entity* parent, const EntityFunction& callable ) -> void {
        MKT_BEGIN_PROFILER();
        if ( !parent ) {
            return;
        }

        RelationComponent& relation{ parent->GetComponent<RelationComponent>() };
        for ( const auto& childID: relation.GetChildren() ) {

            if ( Entity * child{ FindByID( childID ) } ) {
                callable( child, parent );

                ApplyToChildren( child, callable );
            }
        }
    }

    template<typename Callback, typename... ComponentTypes>
    auto Scene::ForAll(const Callback& c) -> void {
        MKT_BEGIN_PROFILER();
        auto view{ mRegistry.view<ComponentTypes...>() };
        for ( const auto& entity: view ) {
            c(view.template get<ComponentTypes>(entity)...);
        }
    }

}

#endif// MIKOTO_SCENE_SCENE_INL