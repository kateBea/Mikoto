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

#include <Core/Core.hh>
#include <Core/Types.hh>

 #include <Audio/AudioListener.hh>
 #include <Audio/AudioDevice.hh>
 #include <Audio/AudioService.hh>

 namespace mikoto::audio {

     using namespace mikoto::core;

     AudioListener::AudioListener( i32 index, float x, float y, float z )
         : mIndex{ index }, mPosition{ x, y, z } {}

     auto AudioListener::SetPosition(float x, float y, float z) -> void {
         mPosition = { x, y, z };
     }

     auto AudioListener::SetPosition( const float3& pos ) -> void {
         mPosition = pos;
     }

     auto AudioListener::GetPosition() const -> const float3& {
         return mPosition;
     }

     auto AudioListener::GetUp() const -> const float3& {
         return mUp;;
     }

     auto AudioListener::GetForward() const -> const float3& {
         return mForward;
     }

     auto AudioListener::GetVelocity() const -> const float3& {
         return mVelocity;
     }

     MKT_NODISCARD auto AudioListener::GetIndex() const -> i32 {
         return mIndex;
     }

     auto AudioListener::SetOrientation(float forwardX, float forwardY, float forwardZ,
                                               float upX, float upY, float upZ) -> void {
         SetForward({ forwardX, forwardY, forwardZ });
         SetUp({ upX, upY, upZ });
     }

     auto AudioListener::SetVelocity(float x, float y, float z) -> void {
         mVelocity = { x, y, z };
     }

     auto AudioListener::SetVelocity(const float3& vel) -> void {
         SetVelocity( vel.x, vel.y, vel.z );
      }

     auto AudioListener::SetUp( float x, float y, float z ) -> void {
          mUp = glm::normalize(float3{ x, y, z });
      }

      auto AudioListener::SetUp( const float3& up ) -> void {
          mUp = glm::normalize(float3{ up });
      }

      auto AudioListener::SetForward( float x, float y, float z ) -> void {
          mForward = glm::normalize(float3{ x, y, z });
      }

      auto AudioListener::SetForward( const float3& forward ) -> void {
          mForward = glm::normalize(float3{ forward });
      }

     auto AudioListener::Apply() const -> void {
         AudioDevice* engine{ AudioService::Get()->GetDevice() };

         ma_engine_listener_set_position(&engine->mAudioEngine, mIndex, mPosition.x, mPosition.y, mPosition.z);
         ma_engine_listener_set_direction(&engine->mAudioEngine, mIndex, mForward.x, mForward.y, mForward.z);
         ma_engine_listener_set_velocity(&engine->mAudioEngine, mIndex, mVelocity.x, mVelocity.y, mVelocity.z);
     }
}