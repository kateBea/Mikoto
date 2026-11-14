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

#ifndef MIKOTO_AUDIO_UTILITY_HH
#define MIKOTO_AUDIO_UTILITY_HH

#include <Audio/AudioSettings.hh>

namespace mikoto::audio {

    /**
     * @brief Clamps a scalar to the supported range.
     *
     * @param value Requested value.
     * @param low Inclusive lower bound; must not exceed high.
     * @param high Inclusive upper bound.
     * @return Clamped value.
     */
    MKT_NODISCARD auto AudioValue( core::f32 value, core::f32 low, core::f32 high ) -> core::f32;

    /**
     * @brief Validates playback scalars and the attenuation model.
     *
     * @param value Requested settings, copied for validation.
     * @return Settings with gain [0,20], pitch [0.01,4], Doppler [0,5], rolloff [0,10], and ordered
     * distance limits. Invalid attenuation defaults to inverse; booleans are preserved.
     */
    MKT_NODISCARD auto SanitizeAudio( SourceSettings value ) -> SourceSettings;

    /**
     * @brief Normalizes a usable direction or returns the supplied fallback.
     *
     * @param value Direction to normalize.
     * @param fallback Returned unchanged for a direction with length at most 0.00001.
     * @return Unit direction, or fallback without further normalization.
     */
    MKT_NODISCARD auto AudioDirection( const core::float3& value, const core::float3& fallback ) -> core::float3;

    /**
     * @brief Derives velocity while suppressing discontinuous-motion samples.
     *
     * @param position Current position.
     * @param previous Previous position.
     * @param timeStep Elapsed seconds; valid only in (0,1].
     * @param initialized Whether a previous position sample exists.
     * @return Position delta divided by timeStep, or zero for an initial sample or invalid interval.
     */
    MKT_NODISCARD auto AudioVelocity(
        const core::float3& position,
        const core::float3& previous,
        core::f32 timeStep, bool initialized ) -> core::float3;
}// namespace mikoto::audio

#endif// MIKOTO_AUDIO_UTILITY_HH
