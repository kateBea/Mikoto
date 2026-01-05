//
// Created by kate on 10/31/25.
//

#ifndef EDITOR_UTILITY_HH
#define EDITOR_UTILITY_HH

#include <glm/glm.hpp>

#include <Scene/Entity.hh>
#include <Library/Random/Random.hh>

namespace Mikoto {
    template<typename ComponentType>
    static auto IsPresent( Entity* entity ) -> bool {
        if ( entity == nullptr ) {
            return false;
        }

    return entity->HasComponent<ComponentType>();
    }

    MKT_NODISCARD inline auto GetRandomizedVec3F(float lowerBound, float upperBound) -> glm::vec3 {
        const float x{ static_cast<float>( GetRandomReal( lowerBound, upperBound ) ) };
        const float y{ static_cast<float>( GetRandomReal( lowerBound, upperBound ) ) };
        const float z{ static_cast<float>( GetRandomReal( lowerBound, upperBound ) ) };

        return glm::vec3{ x, y, z };
    }

}

#endif
