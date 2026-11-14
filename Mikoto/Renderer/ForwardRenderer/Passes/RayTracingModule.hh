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

#include <EASTL/span.h>
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>

namespace mikoto::material {
    class SurfaceMaterial;
}

namespace mikoto::scene {
    class Scene;
    class Camera;
}

namespace mikoto::renderer {

    enum class SceneBackgroundType;

    struct RayTracingInfo {
        FGTextureHandle mDirect{};
        FGTextureHandle mIndirect{};
        FGTextureHandle mAmbientOcclusion{};
        bool mAmbientOcclusionEnabled{};
        bool mShadows{};
        bool mReflect{};
        bool mGlobalIllumination{};
        bool mReady{};
    };

    // Shared transport for hybrid lighting and the full path-tracing view.
    // GPU resources and descriptor ownership belong to the frame graph.
    class RayTracingModule final {
    public:
        // Only transport options belong to this module, independent of a renderer's other passes.
        struct Settings {
            static constexpr core::u32 kMaxPathSamplesPerFrame{ 1024 };

            bool mPathTracing{};
            bool mRayTracedShadows{};
            bool mRayTracedReflections{};
            bool mRayTracedGlobalIllumination{};
            bool mRayTracedAmbientOcclusion{};
            bool mAoCosineSampling{ true };
            core::u32 mAoSamplesPerFrame{ 2 };
            core::f32 mAoMaxDistance{}; // Zero selects PBRT's unlimited-distance default.
            core::u32 mRayGiSamplesPerFrame{ 1 };
            core::u32 mRayGiMaxBounces{ 2 };
            core::u32 mPathSamplesPerFrame{ 2 };
            core::u32 mPathMaxBounces{ 8 };
            core::u32 mPathMaxSamples{ 4096 };

            auto Sanitize() -> void;
        };

        explicit RayTracingModule( rhi::RenderResolution resolution = rhi::RenderResolution::e1080P );

        // Register in this order so trace consumers see uploaded scene data and a built TLAS.
        auto RegisterUploadPass( FrameGraph& graph ) -> void;
        auto RegisterBuildPass( FrameGraph& graph ) -> void;
        auto RegisterFramePass( FrameGraph& graph ) -> void;
        auto RegisterTracePass( FrameGraph& graph, bool pathTracing = false, bool atmosphere = true ) -> void;
        auto RegisterHistoryPass( FrameGraph& graph ) -> void;

        // Refresh view constants and invalidate the packed scene only when its inputs change.
        auto Prepare( FrameGraph& graph, const scene::Scene* scene, const scene::Camera* camera, const Settings& settings,
            rhi::TextureHandle environment, float environmentScale, float exposure, float gamma ) -> void;

        auto SetMeshesVisible( bool visible ) -> void;
        auto SetAmbientOcclusionDebug( bool enabled ) -> void;
        auto SetBackground( SceneBackgroundType type, core::f32 blur ) -> void;
        auto SetEnvironmentFaces( const eastl::array<rhi::TextureHandle, 6>& textures ) -> void;
        auto SetAtmosphere( const AtmosphereData* data, const eastl::array<rhi::TextureHandle, 3>& textures ) -> void;

        auto ResetAccumulation() -> void;

        MKT_NODISCARD auto IsReady() const -> bool;
        MKT_NODISCARD auto GetSampleCount() const -> core::u32;
        MKT_NODISCARD auto GetError() const -> const eastl::string&;

    private:
        struct MeshInstance;
        struct SceneInputs;

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
            glm::uvec4 mOcclusion{}; // Enabled, samples, accumulated samples, cosine sampling.
            core::f32 mOcclusionMaxDistance{};
            core::u32 mVolumePadding{};
            core::u64 mVolumes{};
        };

        struct Vertex {
            core::float4 mPosition{};
            core::float4 mNormal{}; // unit normal, conservative position error
            core::float4 mUV{};
            core::float4 mColor{};
        };

        struct Material {
            core::float4 mBase{};
            core::float4 mEmission{};
            core::float4 mParameters{};
            glm::ivec4 mTextures{ -1 };
            core::float4 mExtra{};
            glm::ivec4 mUVSets{};
            core::float4 mSpecular{};
            core::float4 mUvTransform{ 1, 1, 0, 0 };
            glm::uvec4 mProperties{}; // Shading model, thin wall, BSSRDF byte offset, reserved.
            core::float4 mOptical{}; // IOR, transmission, artist thickness, reserved.
            core::float4 mAbsorption{}; // Inverse world units, phase anisotropy.
            core::float4 mScattering{}; // Inverse world units, reserved.
            core::float4 mHeightMapping{};
            glm::ivec4 mHeightInfo{ -1, 0, 0, 0 };
        };

        struct Light {
            core::float4 mPositionType{};
            core::float4 mDirectionRadius{};
            core::float4 mColorIntensity{};
            core::float4 mSpot{}; // Cone cosines (xy), PT point radius / directional half-angle in radians (z).
        };

        struct SceneHeader {
            core::u32 mMaterialCount{};
            core::u32 mLightCount{};
            core::u32 mEmitterCount{};
            core::u32 mLightOffset{};
            core::u32 mEmitterOffset{};
            core::i32 mEnvironmentIndex{ -1 };
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
            core::float4 mControl{}; // atmosphere, reserved, near plane, far plane
            core::float4 mBackground{}; // blur level, prefiltered mode, hidden, environment scale
            core::float4 mLensRight{}; // camera right, aperture radius in meters
            core::float4 mLensUp{}; // camera up, focus distance in meters
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

    private:
        static auto HashBytes( core::u64& hash, const void* data, core::usize bytes ) -> void;

        template<typename T>
        static auto Hash( core::u64& hash, const T& value ) -> void;

        auto PrepareView( const scene::Camera& camera, core::f32 environmentScale, core::f32 exposure, core::f32 gamma ) -> void;
        auto PrepareScene( const scene::Scene* scene, const rhi::TextureHandle& environment ) -> void;
        auto CollectEnvironment( const rhi::TextureHandle& environment, SceneInputs& inputs ) const -> void;
        auto CollectMeshes( const scene::Scene& scene, SceneInputs& inputs ) const -> void;
        MKT_NODISCARD auto PackMaterial( material::SurfaceMaterial& source, SceneInputs& inputs ) const -> Material;
        auto CollectLights( const scene::Scene& scene, SceneInputs& inputs ) const -> void;

        MKT_NODISCARD auto PackScene( SceneInputs& inputs ) -> GeometryData;
        auto PackSceneMeshes( GeometryData& next, eastl::span<const MeshInstance> instances, eastl::vector<Material>& materials ) -> void;
        static auto PackMeshVertices( GeometryData& next, const MeshInstance& instance, core::u32 base, core::u32 materialIndex ) -> void;
        static auto AppendRectangleEmitters( GeometryData& next, const SceneInputs& inputs, eastl::vector<Material>& materials ) -> void;
        static auto CollectTriangleEmitters( const GeometryData& next, eastl::span<const Material> materials,
            core::u32 triangleCount ) -> eastl::vector<Emitter>;
        static auto PackSceneData( GeometryData& next, const SceneInputs& inputs, eastl::span<Material> materials,
            eastl::span<const Emitter> emitters ) -> void;
        auto AppendLightDistribution( GeometryData& next, eastl::span<const Light> lights, eastl::span<const Material> materials,
            eastl::span<const Emitter> emitters ) const -> void;

        auto RegisterGeometryPreparePass( FrameGraph& graph ) -> void;
        auto RegisterGeometryTransferPass( FrameGraph& graph ) -> void;
        auto RegisterFramePreparePass( FrameGraph& graph ) -> void;
        auto RegisterFrameTransferPass( FrameGraph& graph ) -> void;
        auto CreateTraceResources( FrameGraph& graph ) -> void;
        auto CreateTracePipeline( FrameGraph& graph ) -> void;

        auto PrepareGeometryUpload( CommandContext& context ) -> void;
        auto PrepareFrameUpload( CommandContext& context ) -> void;

        MKT_NODISCARD auto ResolveSceneTextures( CommandContext& context ) const -> eastl::vector<core::ubyte>;
        auto BindFrameResources( CommandContext& context ) -> void;
        auto Trace( CommandContext& context ) -> void;
        auto UpdateHistory( CommandContext& context ) -> void;
        MKT_NODISCARD auto IsOcclusionActive() const -> bool;

        auto SetError( const char* message ) -> void;

    private:

        // Frame graph resources for mesh data, frame constants, dispatch and history.
        FGBufferHandle mVertices{};
        FGBufferHandle mBuildPositions{};
        FGBufferHandle mIndices{};
        FGBufferHandle mData{};
        FGBufferHandle mEffectsBuffer{};
        FGBufferHandle mFrameBuffer{};
        FGTextureHandle mOutput{};
        FGTextureHandle mIndirect{};
        FGTextureHandle mHistory{};
        FGTextureHandle mOcclusionOutput{};
        FGTextureHandle mOcclusionHistory{};
        FGSamplerHandle mSampler{};
        FGSamplerHandle mAtmosphereSampler{};
        FGPipelineHandle mPipeline{};
        FGShaderTableHandle mShaderTable{};
        FGAccelerationStructureHandle mAccelerationStructure{};

        BufferUploadBatch mGeometryUploads{};
        BufferUploadBatch mFrameUploads{};

        // CPU scene snapshot, retained across frames until its inputs change.
        GeometryData mGeometry{};
        eastl::vector<CachedMeshGeometry> mMeshCache{};

        FrameConstants mConstants{};
        ViewEffects mEffects{};
        Settings mSettings{};
        bool mMeshesVisible{ true };
        eastl::array<rhi::TextureHandle, 3> mAtmosphereTextures{};
        eastl::array<rhi::TextureHandle, 6> mEnvironmentFaces{};

        rhi::RenderResolution mResolution{};

        // Scene changes can require a GPU upload. View changes only invalidate accumulation.
        core::u64 mSceneHash{};
        core::u64 mViewHash{};
        core::u32 mSamples{};
        core::u32 mOcclusionSamples{};
        bool mOcclusionDebug{};
        core::u32 mFrame{};
        bool mGeometryDirty{};
        bool mBuildPending{};
        bool mPathTracing{};
        bool mReady{};
        eastl::string mError{};
    };
}
#endif// MIKOTO_RENDERER_FORWARD_RENDERER_RAY_TRACING_MODULE_HH
