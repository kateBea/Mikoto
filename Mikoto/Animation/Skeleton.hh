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

#include <Animation/Joint.hh>

namespace Mikoto {

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

        auto GetBoneCount() -> UInt32;

    private:
        NodeHierarchy m_Hierarchy{};
        std::vector<Joint> m_Joints{};
    };
}

#endif // MIKOTO_SKELETON_HH
