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

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

namespace mikoto::renderer {

    struct TerrainGenerationInfo {
        FGBufferHandle mVertices{};
        FGBufferHandle mIndices{};
        FGPipelineHandle mPipeline{};
        FGBufferHandle mBuildParameters{};
        FGBufferHandle mHeightmapPixels{};
        FGPipelineHandle mGeometryPipeline{};
        FGTextureHandle mNoiseTarget{};
        FGBufferHandle mNoiseReadback{};
        FGPipelineHandle mNoisePipeline{};
        FGBufferHandle mNoiseParameters{};
        FGBufferHandle mElevationColors{};
    };

    class TerrainGenerationModule {
    public:
        explicit TerrainGenerationModule( rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        auto RegisterTerrainGeneration( FrameGraph& graph ) -> void;
        auto RegisterTerrainRendering( FrameGraph& graph, bool hdr = true ) -> void;

    private:
        auto FilterTerrains() -> void;
        auto PrepareGeometryInputs( CommandContext& context, core::Blackboard& blackboard ) -> void;
        auto RegisterNoiseGeneration( FrameGraph& graph ) -> void;
        auto GenerateGeometry( CommandContext& context, core::Blackboard& blackboard ) -> void;

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

        BufferUploadBatch mUploads{};
        eastl::vector<Terrain> mTerrains{};

        // The landscape owns the request and a queued callback keeps it alive through readback.
        eastl::weak_ptr<TerrainNoiseGeneration> mPendingNoise{};
        eastl::weak_ptr<TerrainNoiseGeneration> mLastGpuNoise{};

        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        bool mBudgetWarning{};
        bool mNoiseSubmitted{};
        bool mNoiseReadbackComplete{};
        bool mColorsDirty{ true };
        bool mGeometryDirty{ true };

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };

}

#endif//MIKOTO_RENDERER_FORWARD_RENDERER_LANDSCAPE_MODULE_HH
