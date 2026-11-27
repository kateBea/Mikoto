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

#ifndef MIKOTO_SCENE_WIND_HH
#define MIKOTO_SCENE_WIND_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::scene {

    enum class WindAreaShape : core::u32 {
        eDirectional,
        eBox,
        eSphere
    };

    inline constexpr core::u32 kMaxWindAreas{ 64 };

    /** Shared CPU/GPU field record. Velocity is metres per second in world space. */
    struct WindAreaData {
        core::float4x4 mWorldToLocal{ 1.0f };
        core::float4 mVelocityFalloff{};
        core::float4 mWaves{}; // Gust amplitude, frequency (Hz), wavelength (m), lateral turbulence (m/s).
        core::uint4 mControl{};// Shape, seed, reserved.
    };

    struct WindField {
        core::f32 mTime{};
        eastl::vector<WindAreaData> mAreas{};
    };

    /** Samples the same traveling sine-wave field used by the grass shader. */
    MKT_NODISCARD auto SampleWind( const WindField &field, const core::float3 &position ) -> core::float3;

}// namespace mikoto::scene

#endif// MIKOTO_SCENE_WIND_HH
