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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_LANDSCAPE_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_LANDSCAPE_MODULE_HH

#include <EASTL/shared_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    struct TerrainGenerationInfo {
        FGBufferHandle mVertices{};
        FGBufferHandle mIndices{};
        FGPipelineHandle mPipeline{};
        FGBufferHandle mBuildParameters{};
        FGBufferHandle mSamplingRecords{};
        FGBufferHandle mHeightBounds{};
        FGBufferHandle mBoundsScratch{};
        FGBufferHandle mHeightmapPixels{};
        FGPipelineHandle mGeometryPipeline{};
        FGTextureHandle mNoiseTarget{};
        FGBufferHandle mNoiseReadback{};
        FGPipelineHandle mNoisePipeline{};
        FGBufferHandle mNoiseParameters{};
        FGBufferHandle mElevationColors{};
        FGBufferHandle mSurfaces{};
        FGBufferHandle mPaintWeights{};
        FGSamplerHandle mLayerSampler{};

        FGPipelineHandle mTerrainDepthPipeline{};
        FGTextureHandle mGrassContactDepth{};
        FGTextureHandle mGrassContactMask{};
        FGPipelineHandle mGrassContactDepthPipeline{};
        FGPipelineHandle mGrassContactMeshDepthPipeline{};
        FGPipelineHandle mGrassShadowPipeline{};
        FGPipelineHandle mGrassContactPipeline{};
        FGBufferHandle mGrassDescriptions{};
        FGBufferHandle mGrassTiles{};
        FGBufferHandle mGrassBlades{};
        FGBufferHandle mGrassArguments{};
        FGBufferHandle mGrassMeshes{};
        FGBufferHandle mGrassHiZ{};
        FGPipelineHandle mGrassDepthPipeline{};
        FGPipelineHandle mGrassGeneratePipeline{};
        FGPipelineHandle mGrassRenderPipeline{};
        FGPipelineHandle mGrassMeshPipeline{};
    };

    class TerrainGenerationModule {
    public:
        explicit TerrainGenerationModule( rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene *scene ) -> void;
        auto SetCamera( const scene::Camera *camera ) -> void;

        auto RegisterTerrainGeneration( FrameGraph &graph ) -> void;
        auto RegisterGrassPreparation( FrameGraph &graph ) -> void;
        auto RegisterGrassShadows( FrameGraph &graph ) -> void;
        auto RegisterTerrainRendering( FrameGraph &graph, bool hdr = true ) -> void;

    private:
        auto FilterTerrains() -> void;

        auto RegisterGrass( FrameGraph &graph ) -> void;
        auto RegisterHeightBounds( FrameGraph &graph ) -> void;
        auto RegisterNoiseGeneration( FrameGraph &graph ) -> void;

        auto UploadGrass( CommandContext &context, core::Blackboard &blackboard ) -> void;

        auto PrepareGeometryInputs( CommandContext &context, core::Blackboard &blackboard ) -> void;

        auto GenerateGeometry( CommandContext &context, core::Blackboard &blackboard ) -> void;
        auto UploadSurfaces( CommandContext &context, core::Blackboard &blackboard, bool hdr ) -> void;

        struct Terrain {
            entt::entity mEntity{ entt::null };
            core::RevisionTracked::Revision mRevision{};
            core::u32 mFirstVertex{};
            core::u32 mFirstIndex{};
            core::u32 mIndexCount{};
            eastl::shared_ptr<TerrainNoiseGeneration> mNoise{};
            WorldDescription mWorld{};
            eastl::shared_ptr<const TerrainHeightmap> mHeightmap{};
        };

        struct LayerRequest {
            asset::AssetID mID{};
            asset::AssetHandle<asset::Texture> mHandle{};
        };

        struct GrassDraw {
            core::u32 mCandidateCount{};
            core::u32 mFirstBlade{};
            core::u32 mCapacity{};
            bool mCustomMesh{};
            bool mCastShadows{};
            bool mContactShadows{};
            core::u32 mProxyVertices{};
        };

    private:
        const scene::Scene *mScene{};
        const scene::Camera *mCamera{};

        BufferUploadBatch mUploads{};

        eastl::vector<Terrain> mTerrains{};
        eastl::vector<GrassDraw> mGrassDraws{};

        struct GrassMeshVertex {
            core::float4 mPosition{};
            core::float4 mNormal{};
        };
        struct GrassMesh {
            asset::AssetHandle<asset::Model> mRequest{};
            asset::AssetHandle<asset::Texture> mTexture{};
            rhi::TextureHandle mInlineTexture{};
            const MeshRenderData *mSource{};
            core::RevisionTracked::Revision mRevision{};
            core::u32 mMeshIndex{};
            eastl::vector<GrassMeshVertex> mVertices{};
        };
        eastl::vector<GrassMesh> mGrassMeshes{};

        eastl::vector<LayerRequest> mLayerRequests{};
        eastl::vector<eastl::shared_ptr<const TerrainPaintMap>> mPaintSources{};

        // The landscape owns the request and a
        // queued callback keeps it alive through readback.
        eastl::weak_ptr<TerrainNoiseGeneration> mPendingNoise{};
        eastl::weak_ptr<TerrainNoiseGeneration> mLastGpuNoise{};

        bool mBudgetWarning{};
        bool mNoiseSubmitted{};
        bool mNoiseReadbackComplete{};
        bool mColorsDirty{ true };
        bool mGeometryDirty{ true };
        bool mBoundsDirty{ true };

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };

}// namespace mikoto::renderer

#endif//MIKOTO_RENDERER_FORWARD_RENDERER_LANDSCAPE_MODULE_HH
