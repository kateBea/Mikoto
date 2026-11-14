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

#ifndef MIKOTO_ANIMATION_SKINNING_BOUNDS_HH
#define MIKOTO_ANIMATION_SKINNING_BOUNDS_HH

#include <EASTL/span.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Math/Bounds.hh>

#include <Animation/SkinningPalette.hh>

namespace mikoto::animation {

    // Immutable per-mesh influence bounds. Updating a pose visits joints instead
    // of skinning every vertex on the editor thread.
    class SkinningBounds {
    public:
        auto Build( eastl::span<const SkinningVertex> vertices, core::usize paletteSize ) -> void;

        MKT_NODISCARD auto Transform(
            eastl::span<const core::float4x4> palette,
            const core::float4x4& transform ) const -> math::Bounds;

    private:
        struct JointBounds {
            core::u32 mJoint{};
            core::float3 mCenter{};
            core::float3 mExtent{};
        };

    private:
        math::Bounds mUnweighted{};
        eastl::vector<JointBounds> mJoints{};
    };
}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_SKINNING_BOUNDS_HH
