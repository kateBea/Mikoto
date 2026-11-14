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

#ifndef MIKOTO_ANIMATION_SKINNING_PALETTE_HH
#define MIKOTO_ANIMATION_SKINNING_PALETTE_HH

#include <EASTL/span.h>
#include <EASTL/vector.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Types.hh>
#include <Core/Profiler.hh>

namespace mikoto::animation {

    // CPU positions and influences for posed editor bounds. Retained only for
    // skinned meshes; physics continues to use its original collision geometry.
    struct SkinningVertex {
        core::float3 mPosition{};
        core::float4 mJoints0{};
        core::float4 mWeights0{};
        core::float4 mJoints1{};
        core::float4 mWeights1{};
    };

    // glTF joint indices belong to a skin;
    // Ozz joint indices belong to the scene skeleton.
    struct SkinJointBinding {
        core::u32 mSkeletonJoint{};
        core::float4x4 mInverseBind{ 1.0f };
    };

    struct MeshSkinBinding {
        // Selects the pose within the model instance's animator. Joint indices
        // below are local to this rig, never a merged scene-wide skeleton.
        core::u32 mRigIndex{};
        core::float4x4 mInverseMeshBind{ 1.0f };
        eastl::vector<SkinJointBinding> mJoints{};

        // Usually identity. A collapsed bind transform is retained here when the
        // editable entity is rebased to model space, so stopped playback keeps it hidden.
        core::float4x4 mBindPoseTransform{ 1.0f };
    };

}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_SKINNING_PALETTE_HH
