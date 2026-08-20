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

#ifndef MIKOTO_SWAPCHAIN_RENDER_HH
#define MIKOTO_SWAPCHAIN_RENDER_HH

#include <EASTL/vector.h>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    enum class PresentTarget {
        eGBuffer_Color,
        eGBuffer_Emissive,
        eGBuffer_Normals,
        eGBuffer_Position,

        eWireframe,
        eDepthPrepass,

        ePBRadiance_Output,

        eTonemap_Output,

        eCount,
    };

    struct PresentationPassData {
        // Textures we will render the final image into
        eastl::vector<FGTextureHandle> mPresentTextures{};

        FGPipelineHandle mPipeline{};
    };

    class PresentationModule {
    public:
        explicit PresentationModule( RenderResolution resolution );

        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetPresentType( PresentTarget type ) -> void;

        auto RegisterPresentImage( FrameGraph& graph, rhi::TextureHandle texture ) -> void;

    private:

        auto RegisterTransition( FrameGraph& graph ) -> void;

        auto GetTargetImage( Blackboard& ctx ) -> FGTextureHandle;
        auto RegisterFullQuadRender( FrameGraph& graph ) -> void;

    private:
        FGTextureHandle mPresentTexture{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        PresentTarget mPresentTarget{ PresentTarget::ePBRadiance_Output };
    };

}// namespace mikoto

#endif//MIKOTO_SWAPCHAIN_RENDER_HH
