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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_CLOUDS_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_CLOUDS_MODULE_HH

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <Renderer/Clouds/Clouds.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    class GeometryCullModule;

    struct CloudVolumeTextures {
        FGTextureHandle mModel{};
        FGTextureHandle mLighting{};
        FGTextureHandle mLocalLighting{};
    };

    struct CloudsModuleInfo {
        FGBufferHandle mFrame{};
        FGBufferHandle mModels{};
        FGBufferHandle mInteractions{};
        FGBufferHandle mLocalLights{};
        FGBufferHandle mMeshShadows{};
        FGBufferHandle mShadowCommands{};
        FGBufferHandle mBolts{};
        eastl::array<FGTextureHandle, kMaxCloudVolumes> mSunMaps{};
        eastl::array<eastl::array<FGTextureHandle, 6>, kMaxCloudLocalLights> mLocalMaps{};
        eastl::vector<CloudVolumeTextures> mVolumes{};
        FGTextureHandle mNoise{};
        FGTextureHandle mBaseShapeNoise{};
        FGTextureHandle mWeatherMap{};
        eastl::array<FGTextureHandle, 4> mNoiseMips{};
        FGBufferHandle mDistanceFields{};
        FGBufferHandle mDistanceScratchA{};
        FGBufferHandle mDistanceScratchB{};
        FGTextureHandle mRawRadiance{};
        FGTextureHandle mRawTransmittance{};
        FGTextureHandle mRadiance{};
        FGTextureHandle mTransmittance{};
        FGTextureHandle mRadianceHistory{};
        FGTextureHandle mTransmittanceHistory{};
        FGTextureHandle mSceneCopy{};
        FGTextureHandle mComposite{};
        FGSamplerHandle mClampSampler{};
        FGSamplerHandle mRepeatSampler{};
    };

    /** Nubis³-inspired voxel fields, distance-guided marching and cached cloud lighting.
     * https://www.guerrilla-games.com/read/nubis-cubed
     * The model pass authors procedural fields or resamples an imported density grid.
     * It does not require a fluid simulation or hardware ray-tracing API.
     */
    // The renderer uses Mikoto's RHI/frame graph, atmosphere, depth, temporal history
    // and scene-light visibility. No hardware RT or backend extension is needed.
    // Procedural tapered/lobed formations use cached derivative-warped PSRD profiles;
    // Alligator/Curly-Alligator detail erodes the profile during view sampling. Noise
    // is generated once, independent of camera motion. Wind translates the volume
    // without rebaking it. Nonzero material seeds reproduce formation placement.
    // Cumulus and Frostnova are references for an independent implementation:
    // https://github.com/rubenaryo/Cumulus (fd97783db77914e64d77f9b5632a790fe4638fa5)
    // https://github.com/YueZhang1027/CIS5650-Final-Project-Frostnova (1f67c2759d38c1d3724060843f83e24643e34fa3)
    // Only separately licensed PSRD code is adapted; its MIT notice lives beside the
    // shader. This renderer is not the complete Guerrilla production implementation.
    class CloudsModule final {
    public:
        explicit CloudsModule( rhi::RenderResolution resolution );

        auto SetSettings( const CloudsSettings &settings ) -> void;
        auto SetGeometryManager( GeometryCullModule &geometry ) -> void;
        auto RegisterMeshLighting( FrameGraph &graph ) -> void;
        auto RegisterResources( FrameGraph &graph, core::u32 volumeCapacity ) -> void;
        auto RegisterModelPasses( FrameGraph &graph ) -> void;
        auto RegisterLightingPasses( FrameGraph &graph ) -> void;
        auto RegisterViewPasses( FrameGraph &graph ) -> void;
        auto Prepare( const scene::Scene *scene, const scene::Camera *camera,
            const AtmosphereData *atmosphere, bool aerialPerspective ) -> void;

        MKT_NODISCARD auto GetSettings() const -> const CloudsSettings &;

    private:
        auto PrepareLocalLighting() -> void;
        auto PrepareInteractions() -> void;
        auto PrepareBolt( const CloudLightningSettings &settings, const CloudLightningState &state ) -> void;
        auto UploadMeshLighting( CommandContext &ctx, core::Blackboard &blackboard ) -> void;
        auto RegisterLocalLighting( FrameGraph &graph ) -> void;
        auto RegisterBolts( FrameGraph &graph ) -> void;
        auto RegisterDistancePasses( FrameGraph &graph ) -> void;
        auto RegisterTrace( FrameGraph &graph ) -> void;
        auto RegisterTemporal( FrameGraph &graph ) -> void;
        auto RegisterHistory( FrameGraph &graph ) -> void;
        auto RegisterComposite( FrameGraph &graph ) -> void;

    private:
        struct ShadowView {
            FGTextureHandle mTarget{};
            core::u32 mRecord{};
            core::u32 mFace{};
            core::u32 mSize{};
            core::u32 mFirstDraw{};
            core::u32 mDrawCount{};
            bool mUseUnculled{};
        };

        GeometryCullModule *mGeometryManagement{};
        eastl::vector<CloudLocalLight> mLocalLights{};
        eastl::vector<CloudInteractionRecord> mInteractions{};
        core::u64 mInteractionHash{};
        eastl::vector<CloudBoltSegment> mBolts{};
        eastl::vector<CloudMeshShadow> mMeshShadows{};
        eastl::vector<DrawIndirectCommand> mShadowCommands{};
        eastl::vector<ShadowView> mShadowViews{};
        core::u64 mCasterRevision{};
        core::u64 mLocalLightHash{};
        rhi::RenderResolution mResolution{};
        CloudsSettings mSettings{};
        CloudsFrameData mFrame{};
        core::float4x4 mViewProjection{ 1.0f };
        core::float3 mViewDirection{ 0.0f, 0.0f, -1.0f };
        eastl::vector<CloudRecord> mRecords{};
        eastl::vector<core::ubyte> mModelData{};
        eastl::vector<core::u32> mGridOffsets{};
        eastl::vector<core::u64> mFieldHashes{};
        eastl::vector<bool> mModelDirty{};
        eastl::vector<bool> mDistanceDirty{};
        const scene::Scene *mScene{};
        const scene::Camera *mCamera{};
        core::u64 mHistoryHash{};
        core::u64 mTerrainHash{};
        core::u64 mGridDataHash{};
        bool mSourceDataDirty{ true };
        core::u32 mCapacity{};
        bool mNoiseDirty{ true };
        bool mHistoryValid{};
        bool mResetLighting{ true };
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_CLOUDS_MODULE_HH
