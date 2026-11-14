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

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Profiler.hh>

#include <Audio/Utility.hh>
#include "../AudioBackend.hh"
#include <Audio/AudioDevice.hh>
#include <Audio/AudioListener.hh>
#include <Audio/AudioSettings.hh>

namespace mikoto::audio {

    using namespace mikoto::core;

    AudioListener::AudioListener( core::i32 index, float x, float y, float z, AudioDevice* device )
        : mIndex{ index }, mDevice{ device }, mPosition{ x, y, z } {
    }

    auto AudioListener::SetPosition( float x, float y, float z ) -> void {
        MKT_BEGIN_PROFILER();
        SetPosition( core::float3{ x, y, z } );
    }

    auto AudioListener::SetPosition( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();
        mPosition = value;
        mHasPosition = true;
    }

    auto AudioListener::SetVelocity( float x, float y, float z ) -> void {
        MKT_BEGIN_PROFILER();
        SetVelocity( core::float3{ x, y, z } );
    }

    auto AudioListener::SetVelocity( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();
        mVelocity = value;
    }

    auto AudioListener::UpdatePosition( const core::float3& value, float dt ) -> void {
        MKT_BEGIN_PROFILER();

        SetVelocity( AudioVelocity( value, mPosition, dt, mHasPosition ) );
        SetPosition( value );
    }

    auto AudioListener::Detach() -> void {
        MKT_BEGIN_PROFILER();
        mDevice = nullptr;
    }

    auto AudioListener::SetForward( float x, float y, float z ) -> void {
        MKT_BEGIN_PROFILER();
        SetForward( core::float3{ x, y, z } );
    }

    auto AudioListener::SetForward( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();
        mForward = AudioDirection( value, mForward );
    }

    auto AudioListener::SetUp( float x, float y, float z ) -> void {
        MKT_BEGIN_PROFILER();
        SetUp( core::float3{ x, y, z } );
    }

    auto AudioListener::SetUp( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();
        mUp = AudioDirection( value, mUp );
    }

    auto AudioListener::SetOrientation( float fx, float fy, float fz, float ux, float uy, float uz ) -> void {
        MKT_BEGIN_PROFILER();

        SetForward( fx, fy, fz );
        SetUp( ux, uy, uz );

        // Remove scale/shear and keep a valid orthogonal listener basis.
        auto up = mUp - mForward * glm::dot( mUp, mForward );

        if ( glm::length( up ) < 0.00001f ) {
            const core::float3 fallback = glm::abs( mForward.y ) < 0.9f ? core::float3{ 0, 1, 0 } : core::float3{ 1, 0, 0 };
            up = fallback - mForward * glm::dot( fallback, mForward );
        }

        mUp = AudioDirection( up, core::float3{ 0, 1, 0 } );
    }

    auto AudioListener::GetPosition() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mPosition;
    }

    auto AudioListener::GetUp() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mUp;
    }

    auto AudioListener::GetForward() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mForward;
    }

    auto AudioListener::GetVelocity() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mVelocity;
    }

    auto AudioListener::GetIndex() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mIndex;
    }

    auto AudioListener::SetEnabled( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mEnabled = value;
        Apply();
    }

    auto AudioListener::Apply() const -> void {
        MKT_BEGIN_PROFILER();

        if ( !mDevice || !mDevice->IsInitialized() || mIndex < 0 || core::u32( mIndex ) >= mDevice->GetListenerCount() ) {
            return;
        }

        auto* engine = &mDevice->mImpl->mAudioEngine;
        ma_engine_listener_set_enabled( engine, core::u32( mIndex ), mEnabled ? MA_TRUE : MA_FALSE );
        ma_engine_listener_set_position( engine, core::u32( mIndex ), mPosition.x, mPosition.y, mPosition.z );
        ma_engine_listener_set_direction( engine, core::u32( mIndex ), mForward.x, mForward.y, mForward.z );
        ma_engine_listener_set_world_up( engine, core::u32( mIndex ), mUp.x, mUp.y, mUp.z );
        ma_engine_listener_set_velocity( engine, core::u32( mIndex ), mVelocity.x, mVelocity.y, mVelocity.z );
    }
}// namespace mikoto::audio
