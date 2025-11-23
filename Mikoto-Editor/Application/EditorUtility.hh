//
// Created by kate on 10/31/25.
//

#ifndef EDITOR_UTILITY_HH
#define EDITOR_UTILITY_HH

#include <Scene/Entity.hh>

namespace Mikoto {
    template<typename ComponentType>
    static auto IsPresent( Entity* entity ) -> bool {
        if ( entity == nullptr ) {
            return false;
        }

    return entity->HasComponent<ComponentType>();
    }
}

#endif
