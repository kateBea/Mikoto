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

#ifndef MIKOTO_ANIMATOR_H
#define MIKOTO_ANIMATOR_H

#include <Library/Utility/Types.hh>

#include <Animation/SkinnedAnimation.hh>

namespace Mikoto {
    class Animator {
    public:
        explicit Animator( SkinnedAnimation& animator );

        auto UpdateAnimation( float deltaTime ) -> void;

        auto PushAnimation( SkinnedAnimation& animation ) -> void;

        auto GetFinalBoneMatrices() -> auto& { return m_FinalBoneMatrices; }

    private:
        auto CalculateBoneTransform( const NodeHierarchy* node, Mat4F parentTransform ) -> void;

    private:
        SkinnedAnimation* m_Animation{};
        UInt64 m_AnimationID{};
        float m_CurrentTime{};

        std::vector<SkinnedAnimation*> m_Animations{};

        std::vector<Mat4F> m_FinalBoneMatrices{};
    };
}

#endif//MIKOTO_ANIMATOR_H
