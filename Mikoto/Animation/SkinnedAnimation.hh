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

#ifndef MIKOTO_ANIMATION_HH
#define MIKOTO_ANIMATION_HH

#include <vector>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>

#include <Animation/Bone.hh>
#include <Library/Utility/Types.hh>

namespace  Mikoto {
    struct NodeHierarchy {
        std::string Name{};

        Mat4F Transformation{};

    
        UInt32 ChildrenCount{};
        std::vector<NodeHierarchy> Children{};
    };

    struct BoneInfo {
        /*id is index in finalBoneMatrices*/
        Int32 ID{};

        /*offset matrix transforms vertex from model space to bone space*/
        Mat4F Offset{};
    };

    class SkinnedAnimation {
    public:
        explicit SkinnedAnimation( NodeHierarchy&& hierarchy, float duration, UInt32 ticksPerSecond );

        MKT_NODISCARD auto FindBone( std::string_view name ) -> Bone*;

        auto GetDuration() -> float;
        auto GetTicksPerSecond() -> float;
        auto GetRootNode() -> NodeHierarchy&;

        auto GetBoneIDMap() -> auto& { return m_BoneInfoMap; } 

    private:

        float m_Duration{};// Duration of the animation in ticks
        UInt32 m_TicksPerSecond{};
        NodeHierarchy m_RootNode{};

        std::vector<Bone> m_Bones{};

        ankerl::unordered_dense::map<std::string, BoneInfo> m_BoneInfoMap{};
    };
}


#endif//MIKOTO_ANIMATION_HH
