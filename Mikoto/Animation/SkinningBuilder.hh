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

#ifndef MIKOTO_ANIMATION_BUILDER_HH
#define MIKOTO_ANIMATION_BUILDER_HH

#include <vector>
#include <string>

#include <ozz/base/memory/unique_ptr.h>

#include <ozz/animation/offline/tools/import2ozz.h>

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>

#include <Common/Common.hh>
#include <Assets/Importer.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {


    struct OzzAnimationInfo {
        std::string Name{};
        ozz::unique_ptr<ozz::animation::Animation> Animation{};
    };

    using AnimationList = std::vector<OzzAnimationInfo>;

    class SkinningBuilder final {
    public:
        // This will  be a path to the mikoto asset file in the future for now its just the actual asset file
        explicit SkinningBuilder(const Path& filename);

        MKT_NODISCARD auto Build(ozz::animation::offline::OzzImporter& importer, std::string_view modelFileName) -> bool;

        auto FillModelData( ModelData& data) -> void;
    private:
        // To construct the animations
        ozz::animation::offline::RawAnimation m_RawAnimation{};
        ozz::animation::offline::AnimationBuilder m_AnimationBuilder{};

        std::vector<OzzAnimationInfo> m_Animations{};

        // To construct the skeleton
        ozz::animation::offline::SkeletonBuilder m_Builder{};
        ozz::animation::offline::RawSkeleton m_RawSkeleton{};

        // Run time skeleton
        ozz::animation::Skeleton m_Skeleton{};


        Path m_Filename{};

    };
}

#endif // MIKOTO_ANIMATION_BUILDER_HH
