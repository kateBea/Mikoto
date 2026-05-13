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

#ifndef MIKOTOROOT_SWAPCHAIN_RENDER_HH
#define MIKOTOROOT_SWAPCHAIN_RENDER_HH

#include <EASTL/vector.h>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    struct PresentationPassData {
        // Textures we will render the final image into
        eastl::vector<FGTextureHandle> mPresentTextures{};

        FGSamplerHandle mSampler{};
        FGPipelineHandle mPipeline{};
    };

    class PresentationModule {
    public:

        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto AddPresentTexture( TextureHandle texture ) -> void;

    private:

        auto RegisterTransition( FrameGraph& graph ) -> void;
        auto RegisterFullQuadRender( FrameGraph& graph ) -> void;
        auto RegisterRenderToSwapchain( FrameGraph& graph ) -> void;
    private:
        eastl::vector<TextureHandle> mPresentTextures{};
    };

}// namespace mikoto

#endif//MIKOTOROOT_SWAPCHAIN_RENDER_HH
