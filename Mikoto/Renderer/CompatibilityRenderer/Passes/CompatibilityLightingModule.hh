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

#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_LIGHTING_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_LIGHTING_MODULE_HH

#include <EASTL/array.h>

#include <Core/Types.hh>

namespace mikoto::scene {
    class Scene;
}

namespace mikoto::renderer {
    struct CompatibilityLight {
        core::float4 mPositionType{};
        core::float4 mDirectionRadius{};
        core::float4 mRadiance{};
        core::float4 mSpot{};
    };

    /** Collects active scene lights into the compatibility shader's constant buffer. */
    class CompatibilityLightingModule {
    public:
        // Match the fixed-size array in CompatibilityFrame.slang.
        static constexpr core::usize kMaxLights{ 32 };
        using Lights = eastl::array<CompatibilityLight, kMaxLights>;

        MKT_NODISCARD auto Gather( const scene::Scene* scene, Lights& lights ) const -> core::usize;
    };
}

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_LIGHTING_MODULE_HH
