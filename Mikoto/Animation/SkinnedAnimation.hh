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

#include <Animation/Bone.hh>
#include <Library/Utility/Types.hh>

namespace  Mikoto {
    struct NodeHierarchy {
        std::string Name{};

        Mat4F Transformation{};

    
        UInt32 ChildrenCount{};
        std::vector<NodeHierarchy> Children{};
    };

    class SkinnedAnimation {
    public:
        explicit SkinnedAnimation( NodeHierarchy&& hierarchy, float duration, UInt32 ticksPerSecond );


    private:

        float m_Duration{};// Duration of the animation in ticks
        UInt32 m_TicksPerSecond{};
        NodeHierarchy m_RootNode{};

        std::vector<Bone> m_Bones{};
    };
}


#endif//MIKOTO_ANIMATION_HH
