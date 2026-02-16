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

#ifndef MIKOTO_BONE_HH
#define MIKOTO_BONE_HH

#include <vector>
#include <string>
#include <list>

#include <assimp/scene.h>

#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct KeyPosition {
        Vec3F Position{};
        float TimeStamp{};
    };

    struct KeyRotation {
        Quat Orientation{};
        float TimeStamp{};
    };

    struct KeyScale {
        Vec3F Scale{};
        float TimeStamp{};
    };

    class Joint final {
    public:
        /*reads keyframes from aiNodeAnim*/
        Joint( const std::string& name, Int32 ID, const aiNodeAnim* channel );

        /*interpolates  b/w positions,rotations & scaling keys based on the curren time of
        the animation and prepares the local transformation matrix by combining all keys
        transformations*/
        auto Update( float animationTime ) -> void;

        /* Gets the current index on mKeyPositions to interpolate to based on the current animation time*/
        auto GetPositionIndex( float animationTime ) const -> Int32;

        /* Gets the current index on mKeyRotations to interpolate to based on the current animation time*/
        auto GetRotationIndex( float animationTime ) const -> Int32;

        /* Gets the current index on mKeyScalings to interpolate to based on the current animation time */
        auto GetScaleIndex( float animationTime ) const -> Int32;

        auto GetLocalTransform() const -> const auto& { return m_LocalTransform; }
        auto GetBoneName() const -> auto& { return m_Name; }
        auto GetBoneID() const -> Int32 { return m_ID; }

    private:
        /* Gets normalized value for Lerp & Slerp*/
        auto GetScaleFactor( float lastTimeStamp, float nextTimeStamp, float animationTime ) -> float;

        /*figures out which position keys to interpolate b/w and performs the interpolation and returns the translation matrix*/
        auto InterpolatePosition( float animationTime )-> Mat4F;

        /*figures out which rotations keys to interpolate b/w and performs the interpolation and returns the rotation matrix*/
        auto InterpolateRotation( float animationTime ) -> Mat4F;

        /*figures out which scaling keys to interpolate b/w and performs the interpolation and returns the scale matrix*/
        auto InterpolateScaling( float animationTime ) -> Mat4F;

    private:
        std::vector<KeyPosition> m_Positions{};
        std::vector<KeyRotation> m_Rotations{};
        std::vector<KeyScale> m_Scales{};

        Int32 m_NumPositions{};
        Int32 m_NumRotations{};
        Int32 m_NumScalings{};

        Mat4F m_LocalTransform{};
        std::string m_Name{};
        Int32 m_ID{};
    };
}// namespace Mikoto


#endif//MIKOTO_BONE_HH