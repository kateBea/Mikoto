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

#ifndef MIKOTO_AUDIO_AUDIO_SETTINGS_HH
#define MIKOTO_AUDIO_AUDIO_SETTINGS_HH

#include <EASTL/array.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::audio {
    /**
     * @brief Distance attenuation model used by spatialized sources.
     */
    enum class Attenuation {
        /**
         * @brief Disables distance attenuation.
         */
        eNone,

        /**
         * @brief Applies inverse-distance attenuation.
         */
        eInverse,

        /**
         * @brief Applies linear distance attenuation.
         */
        eLinear,

        /**
         * @brief Applies exponential distance attenuation.
         */
        eExponential
    };

    /**
     * @brief Playback, spatialization, and attenuation parameters for an AudioSource.
     */
    enum class CurveInterpolation { eLinear, eSmooth };

    struct DistanceCurve {
        static constexpr core::u32 kMaxPoints{ 8 };
        eastl::array<core::float2, kMaxPoints> mPoints{
            core::float2{ 0, 1 },
            core::float2{ 1, 1 }
        };

        core::u32 mCount{ 2 };
        bool mEnabled{};

        CurveInterpolation mInterpolation{ CurveInterpolation::eSmooth };

        auto Sanitize() -> void;

        MKT_NODISCARD auto Evaluate( core::f32 distance ) const -> core::f32;
    };

    enum class DistanceParameter {
        eVolume,
        eSpatialBlend,
        eSpread,
        eLowPass,
        eReverb,
        eCount
    };

    struct SourceSettings {
        core::f32 mPan{};
        // Normalized distance (0..max distance) and value (0..1).
        eastl::array<DistanceCurve, static_cast<size_t>( DistanceParameter::eCount )> mCurves{
            DistanceCurve{}, DistanceCurve{},
            DistanceCurve{ .mPoints = { core::float2{ 0, 0 }, core::float2{ 1, 0 } } },
            DistanceCurve{},
            DistanceCurve{ .mPoints = { core::float2{ 0, 0 }, core::float2{ 1, 0 } } }
        };

        /**
         * @brief Linear source gain.
         */
        core::f32 mVolume{ 1.0f };

        /**
         * @brief Playback frequency multiplier.
         */
        core::f32 mPitch{ 1.0f };

        /**
         * @brief Doppler scaling multiplier.
         */
        core::f32 mDoppler{ 1.0f };

        /**
         * @brief Distance at which attenuation begins.
         */
        core::f32 mMinDistance{ 1.0f };

        /**
         * @brief Upper distance limit used by the selected attenuation model.
         *
         * This does not necessarily silence the source beyond this distance.
         */
        core::f32 mMaxDistance{ 100.0f };

        /**
         * @brief Strength of the selected attenuation model.
         */
        core::f32 mRolloff{ 1.0f };

        /**
         * @brief Suppresses source output while retaining playback state.
         */
        bool mMuted{};

        /**
         * @brief Restarts playback when the clip reaches its end.
         */
        bool mLooping{};

        /**
         * @brief Enables listener-relative spatial processing.
         */
        bool mSpatialized{};

        /**
         * @brief Distance attenuation model.
         */
        Attenuation mAttenuation{ Attenuation::eInverse };
    };

}// namespace mikoto::audio

#endif// MIKOTO_AUDIO_AUDIO_SETTINGS_HH