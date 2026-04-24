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

#ifndef MIKOTO_AUDIO_LISTENER_HH
#define MIKOTO_AUDIO_LISTENER_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::audio {

    using namespace mikoto::core;

    inline constexpr i32 kInvalidListenerIndex{ -1 };

    class AudioListener final {
    public:
        explicit AudioListener( i32 index, float x, float y, float z );

        auto SetPosition( float x, float y, float z ) -> void;
        auto SetPosition( const float3& pos ) -> void;

        auto SetUp( float x, float y, float z ) -> void;
        auto SetUp( const float3& up ) -> void;

        auto SetForward( float x, float y, float z ) -> void;
        auto SetForward( const float3& forward ) -> void;

        MKT_NODISCARD auto GetPosition() const -> const float3&;
        MKT_NODISCARD auto GetUp() const -> const float3&;
        MKT_NODISCARD auto GetForward() const -> const float3&;
        MKT_NODISCARD auto GetVelocity() const -> const float3&;

        MKT_NODISCARD auto GetIndex() const -> i32;

        auto SetOrientation( float forwardX, float forwardY, float forwardZ,
                             float upX, float upY, float upZ ) -> void;

        /**
        * @brief Sets the velocity of the listener (used for Doppler effect).
        *
        * @param x Velocity in X.
        * @param y Velocity in Y.
        * @param z Velocity in Z.
        */
        auto SetVelocity( float x, float y, float z ) -> void;
        auto SetVelocity( const float3& vel ) -> void;

        /**
        * @brief Updates the underlying audio backend with the current listener state.
        * Should be called after setting position/orientation/velocity.
        */
        auto Apply() const -> void;

    private:
        i32 mIndex{ kInvalidListenerIndex };

        float3 mUp{ 0.0f, 1.0f, 0.0f };
        float3 mPosition{ 0.0f, 0.0f, 0.0f };
        float3 mForward{ 0.0f, 0.0f, -1.0f };
        float3 mVelocity{ 0.0f, 0.0f, 0.0f };
    };
}

#endif // MIKOTO_AUDIO_LISTENER_HH
