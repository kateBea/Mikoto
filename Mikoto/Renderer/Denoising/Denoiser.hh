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

#ifndef MIKOTO_RENDERER_DENOISING_DENOISER_HH
#define MIKOTO_RENDERER_DENOISING_DENOISER_HH

#include <EASTL/array.h>

#include <Core/Types.hh>

#include <Renderer/Core/FrameGraphTypes.hh>

namespace mikoto::renderer {
    struct DenoiserInputs {
        FGTextureHandle mDiffuse{};
        FGTextureHandle mSpecular{};
        FGTextureHandle mNormalRoughness{};
        FGTextureHandle mViewZ{};
        FGTextureHandle mMotion{};
        FGTextureHandle mDiffuseFactor{};
        FGTextureHandle mSpecularFactor{};
        FGTextureHandle mBase{};

        MKT_NODISCARD auto Textures() const -> eastl::array<FGTextureHandle, 8> {
            return { mDiffuse, mSpecular, mNormalRoughness, mViewZ, mMotion, mDiffuseFactor, mSpecularFactor, mBase };
        }
    };

    // Debug attachments are selectable viewport outputs, independently of beauty composition.
    struct DenoiserModuleInfo {
        FGTextureHandle mValidation{};
        bool mValidationReady{};
    };

    struct DenoiserRayData {
        core::float4x4 mWorldToView{ 1.0f };
        core::uint4 mImages0{};
        core::uint4 mImages1{};
        core::u64 mPreviousPositions{};
        core::u32 mEnabled{};
        core::u32 mMethod{};
        core::float4 mJitter{};
    };

    enum class DenoiserType : core::u32 {
        eReblur,
        eRelax
    };

    enum class DenoiserBackend : core::u32 {
        eNrd,
        eOidn,
        eDlss
    };

    enum class OidnQuality : core::u32 {
        eFast,
        eBalanced,
        eHigh
    };

    enum class DlssInputResolution : core::u32 {
        eNative,
        eOptimal,
        eMinimum,
        eMaximum,
    };

    struct DenoiserSettings {
        bool mEnabled{ true };
        DenoiserBackend mBackend{ DenoiserBackend::eOidn };
        OidnQuality mOidnQuality{ OidnQuality::eHigh };
        DenoiserType mType{ DenoiserType::eReblur };
        DlssInputResolution mDlssInputResolution{ DlssInputResolution::eNative };
        rhi::RayReconstructionPreset mDlssPreset{ rhi::RayReconstructionPreset::eDefault };
        bool mDlssClearGlass{ true };
        core::u32 mHistoryFrames{ 30 };
        core::u32 mFastHistoryFrames{ 6 };
        core::u32 mAtrousIterations{ 5 };
        core::f32 mDisocclusionThreshold{ .01f };
        core::f32 mSplitScreen{};
        bool mAdaptiveSampling{ true };
        bool mResponsiveHistory{ true };
        bool mAntiFirefly{ true };
        bool mReconstructionGuides{};// Runtime-only RR input preview; not serialized.
        bool mValidation{};// Runtime-only viewport selection; not serialized.
        core::u32 mResetRevision{};// Runtime-only reset command; not serialized.

        auto Sanitize() -> void;
        auto operator==( const DenoiserSettings & ) const -> bool = default;
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_DENOISING_DENOISER_HH
