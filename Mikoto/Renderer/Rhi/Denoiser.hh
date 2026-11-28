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

#ifndef MIKOTO_RENDERER_RHI_DENOISER_HH
#define MIKOTO_RENDERER_RHI_DENOISER_HH

#include <EASTL/array.h>

#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {
    class ITexture;

    enum class ImageDenoiserType {
        eOidn,
        eDlssRayReconstruction,
    };

    enum class ImageDenoiserQuality {
        eFast,
        eBalanced,
        eHigh,
    };

    // Only functional RR presets exposed by the pinned NGX SDK.
    enum class RayReconstructionPreset : core::u32 {
        eDefault = 0,
        eD = 4,
        eE = 5,
        eF = 6,
    };

    enum class DenoiserSupport {
        ePreparing,
        eAvailable,
        eUnavailable,
    };

    struct DenoiserInputSizes {
        core::uint2 mOptimal{};
        core::uint2 mMinimum{};
        core::uint2 mMaximum{};
        DenoiserSupport mSupport{ DenoiserSupport::eUnavailable };
    };

    enum class DenoiserResult {
        eSuccess,
        eBusy,
        eUnavailable,
        eFailed,
    };

    struct DenoiserDescription {
        ImageDenoiserType mType{ ImageDenoiserType::eOidn };
        ImageDenoiserQuality mQuality{ ImageDenoiserQuality::eHigh };
        RayReconstructionPreset mPreset{ RayReconstructionPreset::eDefault };
        core::uint2 mInputSize{};
        core::uint2 mOutputSize{};
    };

    // Matrices are row-major. Motion is in input pixels, depth is hardware depth,
    // and normal/roughness contains world-space XYZ plus perceptual roughness.
    struct DenoiserFrameParameters {
        eastl::array<float, 16> mWorldToView{};
        eastl::array<float, 16> mViewToClip{};
        core::float2 mJitter{};
        float mFrameTimeMilliseconds{ 1000.0f / 60.0f };
        bool mReset{};
    };

    // Images must be in unordered-access layout. Inputs are read-only storage
    // images; the caller owns barriers between initialization and evaluations.
    struct DenoiserDispatch {
        ITexture *mColor{};
        ITexture *mDiffuseAlbedo{};
        ITexture *mSpecularAlbedo{};
        ITexture *mNormalRoughness{};
        ITexture *mDepth{};
        ITexture *mMotion{};
        ITexture *mOutput{};
        DenoiserFrameParameters mFrame{};
    };

    // Owns one view's SDK feature, weights, scratch buffers and temporal history.
    // CreateDenoiser runs on a graph compilation worker and submits no GPU work.
    // Unsupported implementations remain valid resources and report IsReady=false.
    class IDenoiser : public DeviceObject {
    public:
        explicit IDenoiser( const DenoiserDescription &description );

        MKT_NODISCARD auto GetDescription() const -> const DenoiserDescription &;
        MKT_NODISCARD auto GetError() const -> const eastl::string &;
        MKT_NODISCARD auto IsReady() const -> bool;

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        DenoiserDescription mDescription{};
        eastl::string mError{};
        bool mReady{};
    };

    using DenoiserHandle = core::Ref<IDenoiser>;
}

#endif// MIKOTO_RENDERER_RHI_DENOISER_HH
