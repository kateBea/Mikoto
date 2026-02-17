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

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

#include <Animation/Skeleton.hh>

namespace  Mikoto {

    // These 2 are vec4 because they need to match the bone influence
    // which is maximum bones influence a vertex ( from what we support now )
    inline constexpr UInt32 MAX_BONE_INFLUENCE{ 4 };
    inline constexpr UInt32 MAX_BONES_PER_MESH{ 100 };
    inline constexpr UInt32 MAX_SKINNED_MESHES{ 1000 };

    class SkinnedAnimation {
    public:
        explicit SkinnedAnimation( std::string_view name, float duration, UInt32 ticksPerSecond );

        auto GetDuration() const -> float;
        auto GetTicksPerSecond() const -> float;

        MKT_NODISCARD auto GetName() const -> const std::string&;

    private:
        // Duration of the animation in ticks
        float m_Duration{};
        UInt32 m_TicksPerSecond{};

        std::string m_Name{};
    };
}

#endif//MIKOTO_ANIMATION_HH
