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

#ifndef MIKOTO_ANIMATION_SKINNING_BUILDER_HH
#define MIKOTO_ANIMATION_SKINNING_BUILDER_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <ozz/base/memory/unique_ptr.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/offline/tools/import2ozz.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

#include <Assets/Importer.hh>
#include <Assets/AssetModel.hh>

namespace mikoto::animation {

    struct SkinningDescription {
        asset::AnimationList mAnimations{};
        eastl::unique_ptr<Skeleton> mSkeleton{};
    };

    /** @brief Immutable source/configuration snapshot shared by all rigs of an import. */
    struct SkinningImportSource {
        core::u64 mFingerprint{};
        eastl::string mConfiguration{};
    };

    class SkinningBuilder final {
    public:
        explicit SkinningBuilder() = default;

        auto SetPath( const filesystem::Path& path ) -> SkinningBuilder&;
        auto SetImporter( ozz::animation::offline::OzzImporter& importer ) -> SkinningBuilder&;

        /** @brief Adds importer-specific dependencies, such as rig membership, to the cache identity. */
        auto SetSourceFingerprint( core::u64 fingerprint ) -> SkinningBuilder&;
        auto SetImportAnimations( bool enabled ) -> SkinningBuilder&;

        /** @brief Borrows a source snapshot until Build returns; no ownership is transferred. */
        auto SetSource( const SkinningImportSource& source ) -> SkinningBuilder&;

        auto Build() -> eastl::unique_ptr<SkinningDescription>;

        /** @brief Reads and fingerprints common inputs once, before building individual rigs. */
        MKT_NODISCARD static auto ReadSource( const filesystem::Path& path, core::u64 fingerprint = 0 ) -> SkinningImportSource;

    private:
        filesystem::Path mFilename{};
        ozz::animation::offline::OzzImporter* mImporter{};

        core::u64 mSourceFingerprint{};
        const SkinningImportSource* mSource{};
        bool mImportAnimations{ true };
    };
}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_SKINNING_BUILDER_HH
