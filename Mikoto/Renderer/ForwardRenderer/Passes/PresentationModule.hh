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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_PRESENTATION_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_PRESENTATION_MODULE_HH

#include <EASTL/vector.h>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    enum class FinalImageType {
        eGBuffer_Color,
        eGBuffer_Emissive,
        eGBuffer_Normals,
        eGBuffer_Position,
        eWireframe,
        eDepthPrepass,
        ePBRadiance_Output,
        eTonemap_Output,
        eChromaticAberration,
        eClusterHeatmap,
        eClusterHeatmapOverlay,
        ePostProcessBeforeToneMapping,
        ePostProcessAfterToneMapping,
        eSsao,
        eRayTracedAmbientOcclusion,
        eDenoiserValidation,
        eDlssNoisyColor,
        eDlssDiffuseAlbedo,
        eDlssSpecularAlbedo,
        eDlssNormalRoughness,
        eDlssDepth,
        eDlssMotion,
        eCount,
    };

    struct PresentationPassData {
        // Textures we will render the final image into
        eastl::vector<FGTextureHandle> mPresentTextures{};

        FGTextureHandle mColorImage{};
        FGPipelineHandle mPipeline{};
        FGPipelineHandle mClusterHeatmapPipeline{};
    };

    class PresentationModule {
    public:
        explicit PresentationModule( renderer::rhi::RenderResolution resolution );

        // A supplied display-color source bypasses raster debug-view selection.
        auto RegisterFullQuadRender( FrameGraph& graph, FGTextureHandle source = {} ) -> void;
        // Register after editor overlays have finished writing to the final image.
        auto RegisterTransition( FrameGraph& graph, bool rayTracingConsumers = false ) -> void;

        // Set the final image, because there is a resolve if needed when using multisampling
        auto GetFinalImage( FrameGraph& graph ) -> rhi::TextureHandle;
        auto SetFinalImageTarget( FinalImageType type ) -> void;

    private:

        auto GetTargetImage( const core::Blackboard& ctx ) -> FGTextureHandle;

    private:
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        FGTextureHandle mFinalImageHandle{ FGResourceManager::kInvalidResourceHandle };
        FinalImageType mPresentTarget{ FinalImageType::eTonemap_Output };
    };

}// namespace mikoto

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_PRESENTATION_MODULE_HH

