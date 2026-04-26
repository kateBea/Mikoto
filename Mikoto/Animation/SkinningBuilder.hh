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

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/tools/import2ozz.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/memory/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>

namespace mikoto::animation {

    struct SkinningDescription {
        AnimationList mAnimations{};
        eastl::unique_ptr<Skeleton> mSkeleton{};
    };

    class SkinningBuilder final {
    public:
        explicit SkinningBuilder() = default;

        auto SetPath( const filesystem::Path& path ) -> SkinningBuilder&;
        auto SetImporter( ozz::animation::offline::OzzImporter& importer ) -> SkinningBuilder&;

        auto Build() -> eastl::unique_ptr<SkinningDescription>;

    private:
        filesystem::Path mFilename{};
        ozz::animation::offline::OzzImporter* mImporter{};

        // To construct the animations
        ozz::animation::offline::RawAnimation mRawAnimation{};
        ozz::animation::offline::AnimationBuilder mAnimationBuilder{};

        AnimationList mAnimations{};

        // To construct the skeleton
        ozz::animation::offline::SkeletonBuilder mBuilder{};
        ozz::animation::offline::RawSkeleton mRawSkeleton{};

        // Specifies the folder within the asset cache folder where animation files are stored
        static constexpr eastl::string_view kAnimationsCachePath{ "Animations" };
    };
}

#endif // MIKOTO_ANIMATION_BUILDER_HH
