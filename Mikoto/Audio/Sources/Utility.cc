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

#include <EASTL/algorithm.h>

#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Audio/Utility.hh>

namespace mikoto::audio {

    auto AudioValue( core::f32 value, core::f32 low, core::f32 high ) -> core::f32 {
        MKT_BEGIN_PROFILER();
        return eastl::clamp( value, low, high );
    }

    auto SanitizeAudio( SourceSettings value ) -> SourceSettings {
        MKT_BEGIN_PROFILER();
        value.mVolume = AudioValue( value.mVolume, 0.0f, 20.0f );
        value.mPan = AudioValue( value.mPan, -1, 1 );

        for ( auto& curve: value.mCurves ) {
            curve.Sanitize();
        }

        value.mPitch = AudioValue( value.mPitch, 0.01f, 4.0f );
        value.mDoppler = AudioValue( value.mDoppler, 0.0f, 5.0f );
        value.mMinDistance = AudioValue( value.mMinDistance, 0.01f, 9999.0f );

        // Keep a nonzero attenuation range even when the minimum is edited
        // beyond the default maximum or a serialized value is invalid.
        const core::f32 minimumMaxDistance{ value.mMinDistance + 0.01f };
        value.mMaxDistance = AudioValue( value.mMaxDistance, minimumMaxDistance, 10000.0f );
        value.mRolloff = AudioValue( value.mRolloff, 0.0f, 10.0f );

        switch ( value.mAttenuation ) {
            case Attenuation::eNone:
            case Attenuation::eInverse:
            case Attenuation::eLinear:
            case Attenuation::eExponential:
                break;
            default:
                value.mAttenuation = Attenuation::eInverse;
                break;
        }

        return value;
    }

    auto AudioDirection( const core::float3& value, const core::float3& fallback ) -> core::float3 {
        MKT_BEGIN_PROFILER();
        const core::f32 length{ glm::length( value ) };
        return length > 0.00001f ? value / length : fallback;
    }

    auto AudioVelocity( const core::float3& position, const core::float3& previous, core::f32 timeStep, bool initialized ) -> core::float3 {
        MKT_BEGIN_PROFILER();
        // First updates and long pauses do not represent continuous motion;
        // suppress their velocity so they cannot produce a Doppler spike.
        if ( !initialized || timeStep <= 0.0f || timeStep > 1.0f ) {
            return {};
        }

        const core::float3 velocity{ ( position - previous ) / timeStep };
        return velocity;
    }
}// namespace mikoto::audio
