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

#include <ankerl/unordered_dense.h>

#include <Library/Utility/Types.hh>

#include <Assets/Model.hh>
#include <Animation/SkinnedAnimation.hh>

namespace Mikoto {

    class Animator {
    public:
        explicit Animator( ModelHandle handle );

        auto UpdateAnimation( float deltaTime ) -> void;

        auto SetCurrentAnimation( std::string_view name ) -> void;
        MKT_NODISCARD auto GetCurrentAnimation() const -> const SkinnedAnimation*;

        auto GetFinalBoneMatrices() -> auto& { return m_FinalMatrices; }
        auto GetAnimationList() const -> const auto& { return m_Model->GetAnimations(); }

        auto StopCurrentAnimation() -> void;

        auto PlayCurrentAnimation() -> void;
        auto PlayAnimation( std::string_view name ) -> void;

        MKT_NODISCARD auto IsPlaying() const -> bool;

    private:

        auto UpdateLocalTransform( const Joint& joint, float animationTime, Mat4F& localTransform ) -> void;
        auto CalculateTransform( const Node& node, glm::mat4 parentTransform, float animationTime, const Skeleton& skeleton ) -> void;

    private:
        UInt64 m_AnimationID{};
        float m_CurrentTime{};

        ModelHandle m_Model{};

        bool m_IsPlaying{ false };
        SkinnedAnimation* m_CurrentAnimation{};

        std::vector<Mat4F> m_LocalTransform{};
        std::vector<Mat4F> m_GlobalTransform{};
        std::vector<Mat4F> m_FinalMatrices{};
    };
}

#endif//MIKOTO_ANIMATOR_H
