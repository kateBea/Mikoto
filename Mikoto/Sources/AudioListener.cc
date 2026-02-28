//    Copyright 2025 ケイト
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

#include <miniaudio.h>

#include <Audio/AudioListener.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioService.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    AudioListener::AudioListener( Int32 index, float x, float y, float z )
        : m_Index{ index }, m_Position{ x, y, z } {}

    auto AudioListener::SetPosition(float x, float y, float z) -> void {
        m_Position = { x, y, z };
    }

    auto AudioListener::SetPosition( const Vec3F& pos ) -> void {
        m_Position = pos;
    }

    auto AudioListener::GetPosition() const -> const Vec3F& {
        return m_Position;
    }

    auto AudioListener::GetUp() const -> const Vec3F& {
        return m_Up;;
    }

    auto AudioListener::GetForward() const -> const Vec3F& {
        return m_Forward;
    }

    auto AudioListener::GetVelocity() const -> const Vec3F& {
        return m_Velocity;
    }

    MKT_NODISCARD auto AudioListener::GetIndex() const -> Int32 {
        return m_Index;
    }

    auto AudioListener::SetOrientation(float forwardX, float forwardY, float forwardZ,
                                              float upX, float upY, float upZ) -> void {
        SetForward({ forwardX, forwardY, forwardZ });
        SetUp({ upX, upY, upZ });
    }

    auto AudioListener::SetVelocity(float x, float y, float z) -> void {
        m_Velocity = { x, y, z };
    }

    auto AudioListener::SetVelocity(const Vec3F& vel) -> void {
        SetVelocity( vel.x, vel.y, vel.z );
     }

    auto AudioListener::SetUp( float x, float y, float z ) -> void {
         m_Up = glm::normalize(Vec3F{ x, y, z });
     }

     auto AudioListener::SetUp( const Vec3F& up ) -> void {
         m_Up = glm::normalize(Vec3F{ up });
     }

     auto AudioListener::SetForward( float x, float y, float z ) -> void {
         m_Forward = glm::normalize(Vec3F{ x, y, z });
     }

     auto AudioListener::SetForward( const Vec3F& forward ) -> void {
         m_Forward = glm::normalize(Vec3F{ forward });
     }

    auto AudioListener::Apply() const -> void {
        AudioDevice* engine{ AudioService::Get()->GetDevice() };

        ma_engine_listener_set_position(&engine->m_AudioEngine, 0, m_Position.x, m_Position.y, m_Position.z);
        ma_engine_listener_set_direction(&engine->m_AudioEngine, 0, m_Forward.x, m_Forward.y, m_Forward.z);
        ma_engine_listener_set_velocity(&engine->m_AudioEngine, 0, m_Velocity.x, m_Velocity.y, m_Velocity.z);
    }
}