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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_RAY_TRACING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_RAY_TRACING_MODULE_HH

#include <EASTL/array.h>
#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Material/SurfaceMaterial.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Denoising/Denoiser.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/RayTracing/RayTracing.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    struct RayTracingInfo {
        DenoiserInputs mDenoiser{};
        FGTextureHandle mDirect{};
        FGTextureHandle mIndirect{};
        FGTextureHandle mAmbientOcclusion{};
        bool mAmbientOcclusionEnabled{};
        bool mShadowsEnabled{};
        bool mReflectionsEnabled{};
        bool mGlobalIlluminationEnabled{};
        bool mTraceOutputsReady{};
    };

    // Shared transport for hybrid lighting and the full path-tracing view.
    // GPU resources and descriptor ownership belong to the frame graph.
    class RayTracingModule final {
    public:
        explicit RayTracingModule( rhi::RenderResolution resolution = rhi::RenderResolution::e1080P );

        // Register in this order so trace consumers see uploaded scene data and a built TLAS.
        auto RegisterUploadPass( FrameGraph &graph ) -> void;
        auto RegisterBuildPass( FrameGraph &graph ) -> void;
        auto RegisterFramePass( FrameGraph &graph ) -> void;
        auto RegisterTracePass( FrameGraph &graph, bool pathTracing = false, bool atmosphere = true ) -> void;
        auto RegisterHistoryPass( FrameGraph &graph ) -> void;

        // Refresh view constants and invalidate the packed scene only when its inputs change.
        auto Prepare( FrameGraph &graph, const scene::Scene *scene, const scene::Camera *camera, const RayTracingConfiguration &settings,
            rhi::TextureHandle environment, float environmentScale, float exposure, float gamma ) -> void;

        auto SetMeshesVisible( bool visible ) -> void;
        auto SetAmbientOcclusionDebug( bool enabled ) -> void;
        auto SetBackground( SceneBackgroundType type, core::f32 blur ) -> void;
        auto SetEnvironmentFaces( const eastl::array<rhi::TextureHandle, 6> &textures ) -> void;
        auto SetAtmosphere( const AtmosphereData *data, const eastl::array<rhi::TextureHandle, 3> &textures ) -> void;

        auto ResetAccumulation() -> void;
        auto SetDenoiserSettings( const DenoiserSettings &settings ) -> void;
        MKT_NODISCARD auto GetLightingRevision() const -> core::u64 {
            return mLightingRevision;
        }
        MKT_NODISCARD auto GetTraceFrameIndex() const -> core::u32 {
            return mTraceFrameIndex;
        }
        MKT_NODISCARD auto GetTopologyRevision() const -> core::u64 {
            return mTopologyRevision;
        }


        // True after frame preparation, until a ray tracing pass reports a failure.
        MKT_NODISCARD auto IsReady() const -> bool;
        MKT_NODISCARD auto GetSampleCount() const -> core::u32;

        // Latest ray tracing exception message, cleared by successful frame preparation.
        MKT_NODISCARD auto GetError() const -> const eastl::string &;

    private:
        // GPU records mirror the declarations in PathTracing_Common.slang.
        struct FrameConstants {
            core::float4x4 mInverseViewProjection{ 1.0f };
            core::float4 mCamera{};
            glm::uvec4 mFrame{};
            core::float2 mDisplay{};
            core::u32 mOutput{};
            core::u32 mSceneIndex{};
            glm::uvec4 mOptions{};
            core::u64 mVertices{};
            core::u64 mIndices{};
            core::u64 mData{};
            core::u64 mViewEffects{};
            core::u32 mHistory{};
            core::u32 mAuxiliary{};
            core::u32 mMaterialSampler{};
            core::u32 mAtmosphereSampler{};
            eastl::array<core::u32, 3> mAtmosphereTextures{};
            core::u32 mLightDistribution{};
            core::u64 mBuildPositions{};
            core::u32 mOcclusionOutput{};
            core::u32 mOcclusionHistory{};
            glm::uvec4 mOcclusion{};// Enabled, samples, accumulated samples, cosine sampling.
            core::f32 mOcclusionMaxDistance{};
            core::u32 mVolumePadding{};
            core::u64 mVolumes{};
            core::u64 mDenoiser{};
        };

        struct Vertex {
            core::float4 mPosition{};
            core::float4 mNormal{};// unit normal, conservative position error
            core::float4 mUV{};
            core::float4 mColor{};
        };

        struct SceneHeader {
            core::u32 mMaterialCount{};
            core::u32 mLightCount{};
            core::u32 mEmitterCount{};
            core::u32 mLightOffset{};
            core::u32 mEmitterOffset{};
            core::i32 mEnvironmentIndex{ kInvalidTextureIndex };
            core::u32 mFaceOffset{};
            core::u32 mHasFaces{};
        };

        struct Emitter {
            core::u32 mTriangle{};
            core::f32 mCDF{};
            core::f32 mArea{};
            core::f32 mWeight{};
        };

        struct ViewEffects {
            AtmosphereData mAtmosphere{};
            core::float4 mControl{};   // atmosphere, reserved, near plane, far plane
            core::float4 mBackground{};// blur level, prefiltered mode, hidden, environment scale
            core::float4 mLensRight{}; // camera right, aperture radius in meters
            core::float4 mLensUp{};    // camera up, focus distance in meters
            core::float4 mLensForward{};
        };

        // Cached per mesh slot. The hash includes world-space shading and packed offsets.
        struct CachedMeshGeometry {
            core::u64 mHash{};
            eastl::vector<Vertex> mVertices{};
            eastl::vector<core::float3> mBuildPositions{};
            eastl::vector<core::u32> mIndices{};
        };

        struct GeometryData {
            eastl::vector<Vertex> mVertices{};
            eastl::vector<core::float3> mBuildPositions{};
            eastl::vector<core::u32> mIndices{};
            eastl::vector<core::ubyte> mSceneData{};
            eastl::vector<rhi::TextureHandle> mTextures{};
            eastl::vector<FGAccelerationGeometry> mMeshes{};
            core::u32 mLightDistribution{};
        };

        static auto HashBytes( core::u64 &hash, const void *data, core::usize bytes ) -> void;

        static auto PackMeshVertices( GeometryData &next, const MeshInstance &instance,
            core::u32 base, core::u32 materialIndex ) -> void;
        static auto AppendRectangleEmitters( GeometryData &next, const SceneInputs &inputs,
            eastl::vector<Material> &materials ) -> void;

        static auto CollectTriangleEmitters( const GeometryData &next,
            eastl::span<const Material> materials,
            core::u32 triangleCount ) -> eastl::vector<Emitter>;

        static auto PackSceneData( GeometryData &next,
            const SceneInputs &inputs,
            eastl::span<Material> materials,
            eastl::span<const Emitter> emitters ) -> void;

        auto AppendLightDistribution( GeometryData &next,
            eastl::span<const Light> lights,
            eastl::span<const Material> materials,
            eastl::span<const Emitter> emitters ) const -> void;

        template<typename T>
        static auto Hash( core::u64 &hash, const T &value ) -> void;

        auto CollectMeshes( const scene::Scene &scene, SceneInputs &inputs ) const -> void;
        auto CollectLights( const scene::Scene &scene, SceneInputs &inputs ) const -> void;
        auto PrepareScene( const scene::Scene *scene, const rhi::TextureHandle &environment ) -> void;
        auto CollectEnvironment( const rhi::TextureHandle &environment, SceneInputs &inputs ) const -> void;
        auto PrepareView( const scene::Camera &camera, core::f32 environmentScale, core::f32 exposure, core::f32 gamma ) -> void;

        auto CreateTracePipeline( FrameGraph &graph ) -> void;
        auto CreateTraceResources( FrameGraph &graph ) -> void;
        auto RegisterFramePreparePass( FrameGraph &graph ) -> void;
        auto RegisterFrameTransferPass( FrameGraph &graph ) -> void;
        auto RegisterGeometryPreparePass( FrameGraph &graph ) -> void;
        auto RegisterGeometryTransferPass( FrameGraph &graph ) -> void;

        auto PrepareFrameUpload( CommandContext &context ) -> void;
        auto PrepareGeometryUpload( CommandContext &context ) -> void;

        auto Trace( CommandContext &context ) -> void;
        auto UpdateHistory( CommandContext &context ) -> void;
        auto BindFrameResources( CommandContext &context ) -> void;

        auto RecordTraceError( const char *message ) -> void;

        auto PackSceneMeshes( GeometryData &next, eastl::span<const MeshInstance> instances,
            eastl::vector<Material> &materials ) -> void;

        MKT_NODISCARD auto IsOcclusionActive() const -> bool;
        MKT_NODISCARD auto PackScene( SceneInputs &inputs ) -> GeometryData;
        MKT_NODISCARD auto ResolveSceneTextures( CommandContext &context ) const -> eastl::vector<core::ubyte>;
        MKT_NODISCARD auto PackMaterial( material::SurfaceMaterial &source,
            SceneInputs &inputs ) const -> Material;

    private:
        // Frame graph resources for mesh data, frame constants, dispatch and history.
        DenoiserSettings mDenoiserSettings{};
        DenoiserInputs mDenoiserInputs{};
        DenoiserRayData mDenoiserData{};
        FGBufferHandle mDenoiserDataBuffer{};
        FGBufferHandle mPreviousPositionsBuffer{};
        eastl::vector<core::float4> mPreviousPositions{};
        core::u64 mTopologyHash{};
        core::u64 mTopologyRevision{};
        const scene::Scene *mDenoiserScene{};
        FGBufferHandle mShadingVertexBuffer{};
        FGBufferHandle mAccelerationBuildPositionBuffer{};
        FGBufferHandle mIndexBuffer{};
        FGBufferHandle mSceneDataBuffer{};
        FGBufferHandle mViewEffectsBuffer{};
        FGBufferHandle mFrameConstantsBuffer{};

        // The main output holds direct lighting in hybrid mode and final radiance in path tracing.
        FGTextureHandle mRadianceOutput{};
        FGTextureHandle mAccumulatedLightingOutput{};
        FGTextureHandle mLightingHistory{};
        FGTextureHandle mOcclusionOutput{};
        FGTextureHandle mOcclusionHistory{};
        FGSamplerHandle mMaterialSampler{};
        FGSamplerHandle mAtmosphereSampler{};
        FGPipelineHandle mTracePipeline{};
        FGShaderTableHandle mTraceShaderTable{};
        FGAccelerationStructureHandle mSceneAccelerationStructure{};

        BufferUploadBatch mGeometryUploads{};
        BufferUploadBatch mFrameUploads{};

        // CPU scene snapshot, retained across frames until its inputs change.
        GeometryData mPackedScene{};
        eastl::vector<CachedMeshGeometry> mPackedMeshCache{};

        FrameConstants mFrameConstants{};
        ViewEffects mViewEffects{};
        RayTracingConfiguration mConfiguration{};
        bool mMeshesVisible{ true };
        eastl::array<rhi::TextureHandle, 3> mAtmosphereTextures{};
        eastl::array<rhi::TextureHandle, 6> mEnvironmentFaces{};

        rhi::RenderResolution mRenderResolution{};

        // Scene changes can require a GPU upload. View changes only invalidate accumulation.
        core::u64 mPackedSceneHash{};
        core::u64 mAccumulationViewHash{};
        core::u32 mAccumulatedRadianceSamples{};
        core::u32 mActivePathSamples{ 1 };
        core::u64 mLightingRevision{};
        core::u32 mAccumulatedOcclusionSamples{};
        bool mAmbientOcclusionDebugEnabled{};
        core::u32 mTraceFrameIndex{};

        // Keep the scene dirty until the acceleration structure build has been recorded.
        bool mGeometryUploadPending{};
        bool mAccelerationBuildPending{};
        bool mTracePassUsesPathTracing{};

        bool mCanTraceFrame{};
        eastl::string mLastTraceError{};
    };
}// namespace mikoto::renderer
#endif// MIKOTO_RENDERER_FORWARD_RENDERER_RAY_TRACING_MODULE_HH
