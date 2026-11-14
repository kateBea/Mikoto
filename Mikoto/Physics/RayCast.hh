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

#ifndef MIKOTO_PHYSICS_RAY_CAST_HH
#define MIKOTO_PHYSICS_RAY_CAST_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::physics {

    /** @brief Stores a ray origin and direction for physics queries. */
    class RayCast {
    public:

        /**
         * @brief Stores a ray without normalizing its direction.
         * @param point Ray origin in world coordinates.
         * @param direction Ray direction, including its supplied magnitude.
         */
        RayCast( const core::float3& point, const core::float3& direction );

        /**
         * @brief Retrieves the ray's origin.
         * @returns Origin in world coordinates.
         */
        MKT_NODISCARD auto GetOrigin() const -> core::float3;

        /**
         * @brief Retrieves the stored direction.
         * @returns Direction with its original magnitude.
         */
        MKT_NODISCARD auto GetDirection() const -> core::float3;

    private:
        core::f32 mTime{};
        core::float3 mOrigin{};
        core::float3 mDirection{};
    };

}// namespace mikoto::physics

#endif// MIKOTO_PHYSICS_RAY_CAST_HH
