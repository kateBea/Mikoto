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

#ifndef MIKOTO_MATERIAL_BSSRDF_HH
#define MIKOTO_MATERIAL_BSSRDF_HH

#include <EASTL/array.h>

#include <Core/Types.hh>

#include <Material/SurfaceMaterial.hh>

namespace mikoto::material {

    // PBRT photon-beam diffusion, evaluated at the material's exact RGB albedos.
    // Each float4 holds optical radius followed by the three radial profile values.
    // CDFs integrate a nonnegative piecewise-linear profile, shared with the GPU sampler.
    inline constexpr core::u32 kBssrdfRadiusSamples{ 64 };

    struct BssrdfProfile {
        eastl::array<core::float4, kBssrdfRadiusSamples> mProfile{};
        eastl::array<core::float4, kBssrdfRadiusSamples> mCDF{};
    };

    auto BuildBssrdfProfile( const SubsurfaceSettings& settings ) -> BssrdfProfile;
}

#endif// MIKOTO_MATERIAL_BSSRDF_HH
