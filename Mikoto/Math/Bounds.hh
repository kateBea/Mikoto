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

#ifndef MIKOTO_MATH_BOUNDS_HH
#define MIKOTO_MATH_BOUNDS_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

namespace mikoto::math {

    struct Bounds {
        core::float3 mMinimum{};
        core::float3 mMaximum{};
        bool mIsValid{};

        /**
         * @brief Expands the bounding box to encapsulate the given 3D point.
         *
         * If the box is currently invalid, this point will set both the minimum and maximum.
         * If it is valid, the boundaries will dynamically stretch to fit the point.
         *
         * @param point The 3D coordinate vector to include within the bounds.
         */
        auto Include( const core::float3& point ) -> void;

        /**
         * @brief Checks if the bounding box has a non-zero volume or positive size.
         *
         * @return true If the bounds have a valid, measurable size across the axes.
         * @return false If the box is collapsed into a single point, inverted, or flat.
         */
        MKT_NODISCARD auto HasExtent() const -> bool;
    };
}// namespace mikoto::math

#endif// MIKOTO_MATH_BOUNDS_HH
