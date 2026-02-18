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
#include <utility>
#include <list>

#include <ankerl/unordered_dense.h>

#include <Library/Utility/Types.hh>

namespace Mikoto {

    // Maps vertex and the weight contribution of the joint to that vertex
    using JointVertexMap = 
        ankerl::unordered_dense::map<std::string, ankerl::unordered_dense::map<UInt64, float>>;

    inline constexpr Int32 INVALID_JOINT_ID{ -1 };

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

    struct AnimationeProperties {
        std::vector<KeyPosition> Positions{};
        std::vector<KeyRotation> Rotations{};
        std::vector<KeyScale> Scales{};
    };

    class Joint final {
    public:
        Joint( const std::string& name, Int32 ID, Mat4F ModelToBoneTransform );

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

        auto SetParentID( Int32 ID) -> void;
        auto SetParentRelativeTransform( const Mat4F& mat ) -> void;
        auto SetParentRelativeTransform() const -> const Mat4F&;

        auto SetAnimationProperties(AnimationeProperties&& properties ) -> void;

        auto DebugPrintBoneContribution() const -> void;
        auto SetVertexWeights( std::string_view meshName, UInt64 vertex, float weight ) -> void;

        auto GetID() const -> Int32;
        auto GetParentID() const -> Int32;
        auto GetBoneName() const -> const std::string&;
        auto GetLocalTransform() const -> const Mat4F&;
        auto GetModelToBoneTransform() const -> const Mat4F&;

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
        Int32 m_ID{ INVALID_JOINT_ID };
        Int32 m_ParentID{ INVALID_JOINT_ID };
        std::string m_Name{};

        Mat4F m_LocalTransform{}; // Will be moved to the animator and we will have one per joint because this changes every frame depending on the animation being played on a specific mesh
        Mat4F m_ModelToBoneTransform{};
        Mat4F m_ParentRelativeTransform{};

        std::vector<KeyPosition> m_Positions{};
        std::vector<KeyRotation> m_Rotations{};
        std::vector<KeyScale> m_Scales{};

        JointVertexMap m_VertexWeights{};
    };
}

#endif//MIKOTO_BONE_HH