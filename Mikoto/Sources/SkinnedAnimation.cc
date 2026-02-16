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

#include <Animation/SkinnedAnimation.hh>

namespace Mikoto {
    SkinnedAnimation::SkinnedAnimation( std::string_view name, float duration, UInt32 ticksPerSecond, Skeleton&& skeleton )
        : m_Duration{ duration }, m_TicksPerSecond{ ticksPerSecond }, m_Skeleton { std::move(skeleton) }, m_Name{ name } {}

    auto SkinnedAnimation::FindBone( std::string_view name ) -> Joint* {
        return nullptr; // TODO
    }

    auto SkinnedAnimation::GetDuration() -> float {
        return m_Duration;
    }

    auto SkinnedAnimation::GetTicksPerSecond() -> float {
        return m_TicksPerSecond;
    }

    auto SkinnedAnimation::GetSkeleton() -> Skeleton& {
        return m_Skeleton;
    }

    auto SkinnedAnimation::GetName() const -> const std::string& {
        return m_Name;
    }

    auto SkinnedAnimation::GetBoneCount() -> UInt32 {
        return m_Skeleton.GetBoneCount();
    }

    auto SkinnedAnimation::SetBoneMapInfo( BoneInfoMap&& info ) -> void {
        m_BoneInfoMap = std::move(info);
    }

    auto SkinnedAnimation::GetBoneMap() const -> const BoneInfoMap& {
        return m_BoneInfoMap;
    }
}
