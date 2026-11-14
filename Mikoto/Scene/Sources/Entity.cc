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

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

namespace mikoto::scene {
    auto Entity::SetType( EntityType type ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mType == type ) {
            return;
        }

        mType = type;

        // Refresh type filters through the same signal used for entity renames.
        if ( auto* tag{ mRegistry ? mRegistry->try_get<TagComponent>( mHandle ) : nullptr }; tag && tag->mScene ) {
            tag->mScene->NotifyHierarchyChanged( false );
        }
    }


    auto Entity::GetType() const -> EntityType {
        MKT_BEGIN_PROFILER();
        return mType;
    }


    auto Entity::Get() const -> const entt::entity& {
        MKT_BEGIN_PROFILER();
        return ( mHandle );
    }


    auto Entity::IsValid() const -> bool {
        MKT_BEGIN_PROFILER();
        return mRegistry != nullptr && mRegistry->valid( mHandle );
    }


    Entity::~Entity() {
        MKT_BEGIN_PROFILER();
        Invalidate();
    }


    Entity::Entity( entt::registry& registry, EntityType type )
        : mHandle{ registry.create() }, mRegistry{ MKT_ADDRESSOF( registry ) }, mType{ type } {
        /**
         * See: Observe changes section from https://github.com/skypjack/entt/wiki/Entity-Component-System
         * for details on listeners.
         * */
    }


    auto Entity::Invalidate() -> void {
        MKT_BEGIN_PROFILER();
        // This entity does not belong to any scene anymore
        // it can be safely removed from entt structures
        mRegistry = nullptr;
        mHandle = entt::null;
    }


    auto Entity::operator==( const Entity& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mHandle == other.mHandle && mRegistry == other.mRegistry;
    }
}
