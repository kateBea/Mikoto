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

#ifndef MIKOTO_RENDERER_PASSES_THUMBNAIL_PASSES_HH
#define MIKOTO_RENDERER_PASSES_THUMBNAIL_PASSES_HH

#include <EASTL/array.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/AssetModel.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>

namespace mikoto::renderer {

    struct ThumbnailPreviewSettings {
        core::f32 mYaw{ 35.0f };
        core::f32 mPitch{ 18.0f };
        core::f32 mDistance{ 4.5f };

        // Browser thumbnails fit the model;
        // interactive previews retain manual zoom.
        bool mFrameModelBounds{};
        bool mFitMaterialSphere{};
        core::f32 mExposure{ 1.0f };
        core::f32 mEnvironmentIntensity{ 1.0f };

        // Perceptual roughness of the prefiltered background (0 = sharp, 1 = diffuse).
        core::f32 mBackgroundBlur{ 0.0f };
        core::f32 mLightYaw{ -35.0f };
        core::f32 mLightElevation{ 55.0f };
        core::f32 mLightIntensity{ 3.0f };
        bool mGround{ true };
        bool mInfiniteGrid{}; // Interactive previews opt in; browser tiles stay uncluttered.
        bool mShadows{ true };
        rhi::TextureHandle mEnvironment{};
        BloomSettings mBloom{};
    };

    // A compact preview scene: no editor entities, physics world, selection,
    // clustered-light buffers, or full-scene geometry arena. All preview graph
    // resources and passes are declared here and in ThumbnailPasses.cc.
    class ThumbnailPasses final {
    public:
        ThumbnailPasses( asset::ModelHandle model, core::u32 width, core::u32 height );

        auto RegisterUpload( FrameGraph& graph ) -> void;
        auto RegisterShadows( FrameGraph& graph ) -> void;
        auto RegisterAudioFrame( FrameGraph& graph ) -> void;
        auto RegisterPrefilter( FrameGraph& graph ) -> void;
        auto RegisterEnvironment( FrameGraph& graph ) -> void;
        auto RegisterMaterials( FrameGraph& graph ) -> void;
        auto RegisterBloom( FrameGraph& graph ) -> void;
        auto RegisterTonemap( FrameGraph& graph ) -> void;
        auto RegisterReadback( FrameGraph& graph ) -> void;
        auto RegisterSampledOutput( FrameGraph& graph ) -> void;

        auto Prepare( const ThumbnailPreviewSettings& settings, material::MaterialHandle material, bool readback ) -> void;
        auto ReadImage( const FGResourceManager& resources ) const -> asset::ImageHandle;
        auto GetOutput() const -> FGTextureHandle;

    private:
        // Match ForwardRenderer: 512 cubemap, complete roughness mip chain.
        static constexpr core::u32 kPrefilterSize{ 512 };
        static constexpr core::u32 kPrefilterMipCount{ 10 };

        struct alignas( 16 ) SceneData {
            core::float4x4 mViewProjection{ 1.0f };
            core::float4x4 mInverseViewProjection{ 1.0f };
            core::float4x4 mLightViewProjection{ 1.0f };
            core::float4 mCamera{};
            core::float4 mLightDirection{};
            core::float4 mLightColor{};
            core::float4 mSettings{};
            core::float4 mBloom{};
            core::float4 mPreviewOptions{};
        };
        struct alignas( 16 ) DrawData {
            core::float4x4 mModel{ 1.0f };
            core::float4x4 mNormal{ 1.0f };
            core::float4 mBaseColor{ 1.0f };
            core::float4 mEmission{};
            core::float4 mFactors{};
            core::float4 mFlags{};
            eastl::array<core::i32, 4> mTextures0{ -1, -1, -1, -1 };
            eastl::array<core::i32, 4> mTextures1{ -1, -1, -1, -1 };
            eastl::array<core::i32, 4> mUv0{};
            eastl::array<core::i32, 4> mUv1{};
            core::u64 mVertices{};
            core::u64 mIndices{};
            core::u64 mPalette{};
            core::u32 mJointCount{};
            core::u32 mPadding{};
            core::float4 mSurfaceShading{};
            core::float4 mShadingModel{};
            core::i32 mClearCoatNormalIndex{ -1 };
            core::i32 mClearCoatNormalTextureSet{};
            core::f32 mClearCoatNormalScale{ 1.0f };
            core::f32 mCoatPadding{};
            core::float4 mUvTransform{ 1, 1, 0, 0 };
            core::float4 mDielectricAttenuation{ 1, 1, 1, 10 };
        };
        struct Mesh {
            const asset::MeshNode* mMesh{};
            FGBufferHandle mVertices{};
            FGBufferHandle mIndices{};
            core::Ref<material::SurfaceMaterial> mMaterial{};
            core::usize mPaletteOffset{};
            core::u32 mJointCount{};
            core::float3 mCenter{};
        };
        struct Parameters {
            core::u64 mScene{};
            core::u64 mDraws{};
            core::u32 mDrawIndex{};
            core::u32 mSampler{};
            core::u32 mShadow{};
            core::i32 mEnvironment{ -1 };
            core::u32 mMode{};
            core::u32 mColor{};
            core::u32 mClampSampler{};
            core::u32 mBloom{};
            core::u32 mPrefilter{};
            core::u32 mPrefilterMipCount{};
        };
        auto Upload( CommandContext& context ) -> void;
        auto DeclareGeometryReads( FGNodeBuilder& builder ) const -> void;
        auto DrawMeshes( CommandContext& context, bool shadow ) -> void;
        auto ParametersFor( CommandContext& context, core::u32 mode ) const -> Parameters;
        auto SetViewport( CommandContext& context, core::u32 width, core::u32 height ) const -> void;

        core::u32 mWidth{};
        core::u32 mHeight{};
        asset::ModelHandle mModel{};

        core::float4x4 mNormalization{ 1.0f };
        core::float3 mNormalizedHalfExtent{};

        ThumbnailPreviewSettings mSettings{};
        material::MaterialHandle mOverride{};

        SceneData mSceneData{};

        eastl::vector<Mesh> mMeshes{};
        eastl::vector<DrawData> mDrawData{};
        eastl::vector<core::float4x4> mPalettes{};

        bool mReadback{};

        FGBufferHandle mSceneBuffer{};
        FGBufferHandle mDrawBuffer{};
        FGBufferHandle mReadbackBuffer{};
        FGBufferHandle mPaletteBuffer{};
        FGTextureHandle mShadow{};
        FGTextureHandle mDepth{};
        FGTextureHandle mHdr{};
        FGTextureHandle mOutput{};
        FGTextureHandle mBloomSeed{};
        FGTextureHandle mPrefilter{};
        FGPipelineHandle mPrefilterPipeline{};
        eastl::array<FGTextureHandle, 5> mBloomDown{};
        eastl::array<FGTextureHandle, 4> mBloomUp{};
        FGSamplerHandle mSampler{};
        FGSamplerHandle mClampSampler{};
        FGPipelineHandle mShadowPipeline{};
        FGPipelineHandle mOpaquePipeline{};
        FGPipelineHandle mTransparentPipeline{};
        FGPipelineHandle mEnvironmentPipeline{};
        FGPipelineHandle mTonemapPipeline{};
        FGPipelineHandle mBloomPipeline{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_PASSES_THUMBNAIL_PASSES_HH

