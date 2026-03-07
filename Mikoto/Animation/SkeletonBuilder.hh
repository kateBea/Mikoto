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

#ifndef MIKOTO_SKELETON_BUILDER_HH
#define MIKOTO_SKELETON_BUILDER_HH

#include <ozz/base/memory/unique_ptr.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>

namespace Mikoto {
    using OzzSkeleton = ozz::unique_ptr<ozz::animation::Skeleton>;

    class SkeletonBuilder {
    public:

        virtual ~SkeletonBuilder() = default;

        auto GetSkeleton() -> OzzSkeleton&;

        virtual auto Build() -> bool = 0;

    private:
        // To construct the skeleton
        ozz::animation::offline::SkeletonBuilder m_Builder{};
        ozz::animation::offline::RawSkeleton m_RawSkeleton{};

        // Run time skeleton
        ozz::unique_ptr<ozz::animation::Skeleton> m_Skeleton{};
    };
}


#endif // MIKOTO_SKELETON_BUILDER_HH
