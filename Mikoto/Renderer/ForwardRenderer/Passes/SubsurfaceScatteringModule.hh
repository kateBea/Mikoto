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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_SUBSURFACE_SCATTERING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_SUBSURFACE_SCATTERING_MODULE_HH

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    class GeometryShadingModule;
    class GeometryCullModule;

    struct SubsurfaceScatteringInfo {
        FGTextureHandle mDiffuse{};
        FGTextureHandle mSpecular{};
        FGTextureHandle mAmbient{};
        FGTextureHandle mIdentityDepth{};
        FGTextureHandle mFiltered[2]{};
        FGTextureHandle mComposite{};
        FGPipelineHandle mBlurPipeline{};
    };

    /** @brief Separate diffuse/specular/ambient capture, two diffuse convolutions, and explicit composition.
     * Discrete RGB Gaussian kernels are normalized on CPU and uploaded on edits.
     * Raster opaque/masked surfaces, including MSAA; path tracing remains separate.
     * https://github.com/iryoku/separable-sss/tree/master
     * https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/
     * https://github.com/bentoBAUX/Separable-Subsurface-Scattering-in-Unity-6
     * https://users.cg.tuwien.ac.at/zsolnai/gfx/separable-subsurface-scattering-with-activision-blizzard/
     * https://www.cg.tuwien.ac.at/research/publications/2015/Jimenez_SSS_2015/Jimenez_SSS_2015-paper.pdf
     * https://bentobaux.github.io/assets/research/realtime-subsurface-scattering/paper.pdf
     */
    class SubsurfaceScatteringModule {
    public:
        // Register lighting first, then axis 0 (horizontal), axis 1 (vertical), and composition.
        auto RegisterBlurPass( FrameGraph& graph, core::u32 axis, rhi::RenderResolution resolution ) -> void;
        auto RegisterCompositePass( FrameGraph& graph, GeometryCullModule& geometry,
            rhi::RenderResolution resolution, rhi::Multisampling multisampling ) -> void;
        auto RegisterStorePass( FrameGraph& graph, rhi::RenderResolution resolution,
            rhi::Multisampling multisampling ) -> void;

    };
}// namespace mikoto::renderer
#endif

