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

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Audio/Utility.hh>
#include <Audio/AudioSettings.hh>

namespace mikoto::audio {

    using namespace mikoto::core;

    auto DistanceCurve::Sanitize() -> void {
        mCount = eastl::clamp( mCount, 2u, kMaxPoints );

        for ( core::u32 i{}; i < mCount; ++i ) {
            mPoints[i].x = AudioValue( mPoints[i].x, 0, 1 );
            mPoints[i].y = AudioValue( mPoints[i].y, 0, 1 );
        }

        eastl::sort( mPoints.begin(), mPoints.begin() + mCount, []( auto a, auto b ) {
            return a.x < b.x;
        } );
        mPoints[0].x = 0;
        mPoints[mCount - 1].x = 1;
    }

    auto DistanceCurve::Evaluate( core::f32 distance ) const -> core::f32 {
        // Flat endpoint tangents join the constant values outside the distance range.
        // Interior tangents use a weighted harmonic mean: extrema stay flat and
        // monotone segments cannot overshoot their control point values.
        const auto tangent = [&]( core::u32 point ) -> float {
            if ( point == 0 || point + 1 == mCount ) {
                return 0.f;
            }

            const float leftWidth{ mPoints[point].x - mPoints[point - 1].x };
            const float rightWidth{ mPoints[point + 1].x - mPoints[point].x };

            if ( leftWidth <= 0.f || rightWidth <= 0.f ) {
                return 0.f;
            }

            const float left{ ( mPoints[point].y - mPoints[point - 1].y ) / leftWidth };
            const float right{ ( mPoints[point + 1].y - mPoints[point].y ) / rightWidth };

            if ( left * right <= 0.f ) {
                return 0.f;
            }

            const float a{ 2.f * rightWidth + leftWidth };
            const float b{ rightWidth + 2.f * leftWidth };
            return ( a + b ) / ( a / left + b / right );
        };

        for ( core::u32 i{ 1 }; i < mCount; ++i ) {
            if ( distance <= mPoints[i].x ) {
                const auto& left{ mPoints[i - 1] };
                const auto& right{ mPoints[i] };
                const float width{ right.x - left.x };

                if ( width <= 0.f ) {
                    return right.y;
                }

                const float t{ glm::clamp( ( distance - left.x ) / width, 0.f, 1.f ) };

                if ( mInterpolation == CurveInterpolation::eLinear ) {
                    return glm::mix( left.y, right.y, t );
                }

                const float t2{ t * t }, t3{ t2 * t };
                const float value{ ( 2.f * t3 - 3.f * t2 + 1.f ) * left.y +
                                   ( t3 - 2.f * t2 + t ) * width * tangent( i - 1 ) +
                                   ( -2.f * t3 + 3.f * t2 ) * right.y +
                                   ( t3 - t2 ) * width * tangent( i ) };
                return glm::clamp( value, glm::min( left.y, right.y ), glm::max( left.y, right.y ) );
            }
        }

        return mPoints[mCount - 1].y;
    }

}// namespace mikoto::audio
