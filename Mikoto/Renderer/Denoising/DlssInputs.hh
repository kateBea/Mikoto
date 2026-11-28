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

#ifndef MIKOTO_RENDERER_DENOISING_DLSS_INPUTS_HH
#define MIKOTO_RENDERER_DENOISING_DLSS_INPUTS_HH

#include <EASTL/array.h>

#include <Core/Types.hh>

#include <Renderer/Core/FrameGraphTypes.hh>

namespace mikoto::renderer {
    // Scene-linear, unfiltered RR inputs. Normals are world-space, roughness is
    // perceptual, depth is unjittered NDC [0,1], and motion is previous-current
    // in render pixels with a top-left origin. Specular hit distance is optional.
    struct DlssInputs {
        FGTextureHandle mNoisyColor{};
        FGTextureHandle mDiffuseAlbedo{};
        FGTextureHandle mSpecularAlbedo{};
        FGTextureHandle mNormalRoughness{};
        FGTextureHandle mDepth{};
        FGTextureHandle mMotion{};

        MKT_NODISCARD auto GetTextures() const -> eastl::array<FGTextureHandle, 6> {
            return { mNoisyColor, mDiffuseAlbedo, mSpecularAlbedo, mNormalRoughness, mDepth, mMotion };
        }
    };

    // Kept separate from NRD's packed surface/lobe contract.
    struct DlssRayData {
        core::float4x4 mViewProjection{ 1.0f };
        core::float4x4 mPreviousViewProjection{ 1.0f };
        core::uint4 mImages0{};
        core::uint4 mImages1{};
        core::u64 mPreviousPositions{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::float4 mJitter{};// Film offset xy, previous-frame validity z, reserved w.
        core::uint4 mGuideOptions{};// Clear-glass replacement enabled in x; reserved yzw.
    };
    static_assert( sizeof( DlssRayData ) == 208 );
}

#endif// MIKOTO_RENDERER_DENOISING_DLSS_INPUTS_HH
