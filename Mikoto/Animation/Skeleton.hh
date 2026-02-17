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

#ifndef MIKOTO_SKELETON_HH
#define MIKOTO_SKELETON_HH

#include <string_view>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Animation/Joint.hh>

namespace Mikoto {

    struct BoneInfo {
        /*id is index in finalBoneMatrices*/
        Int32 ID{};

        /*offset matrix transforms vertex from model space to bone space*/
        Mat4F Offset{};
    };

    using BoneMap = ankerl::unordered_dense::map<std::string, Joint>;
    using BoneInfoMap = ankerl::unordered_dense::map<std::string, BoneInfo>;

    struct NodeHierarchy {
        std::string Name{};

        Mat4F Transformation{};

        UInt32 ChildrenCount{};
        std::vector<NodeHierarchy> Children{};
    };

    class Skeleton {
    public:

        explicit Skeleton( NodeHierarchy&& hierarchy = {} );

        auto GetHierarchy() -> NodeHierarchy&;
        auto SetHierarchy(NodeHierarchy&& hierarchy) -> void;

        auto SetBoneMapInfo(BoneInfoMap&& info) -> void;
        auto GetBoneInfoMap() const -> const BoneInfoMap&;
        auto GetBoneInfoMap() -> BoneInfoMap&;

        auto GetBoneMap() -> BoneMap&;
        auto GetBoneMap() const -> const BoneMap&;

        MKT_NODISCARD auto FindBone( std::string_view name ) -> Joint*;

        auto GetBoneCount() const -> UInt32;

        auto SetBoneMap(BoneMap&& boneMap ) -> void;

    private:
        BoneMap m_Joints{};
        BoneInfoMap m_BoneInfoMap{};

        NodeHierarchy m_Hierarchy{};
    };
}// namespace Mikoto

#endif // MIKOTO_SKELETON_HH
