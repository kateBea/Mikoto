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

#include <Physics/RayCast.hh>

namespace mikoto::physics {

    auto RayCastHit::SetPosition( const core::float3& position ) -> void {
        mPosition = position;
    }

    auto RayCastHit::SetNormal( const core::float3& normal ) -> void {
        mNormal = normal;
    }

    auto RayCastHit::SetFraction( core::f32 fraction ) -> void {
        mFraction = fraction;
    }

    auto RayCastHit::SetHit( bool hit ) -> void {
        mHit = hit;
    }

    auto RayCastHit::GetPosition() const -> core::float3 {
        return mPosition;
    }

    auto RayCastHit::GetNormal() const -> core::float3 {
        return mNormal;
    }

    auto RayCastHit::GetFraction() const -> core::f32 {
        return mFraction;
    }

    auto RayCastHit::HasHit() const -> bool {
        return mHit;
    }

    RayCast::RayCast( const core::float3& point, const core::float3& direction )
        : mOrigin{ point }, mDirection{ direction } {
    }

    auto RayCast::GetOrigin() const -> core::float3 {
        return mOrigin;
    }

    auto RayCast::GetDirection() const -> core::float3 {
        return mDirection;
    }

}// namespace mikoto::physics
