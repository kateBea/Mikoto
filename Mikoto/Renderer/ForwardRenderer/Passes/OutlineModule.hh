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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_OUTLINE_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_OUTLINE_MODULE_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    struct OutlineModuleInfo {
        FGBufferHandle mHighlightedDraws{};
        FGTextureHandle mInteriorMask{};
        FGTextureHandle mSilhouette{};
        FGTextureHandle mSeeds[2]{};
        // Premultiplied display-color overlay, resolved before final composition.
        FGTextureHandle mResolvedColor{};
        core::RevisionTracked::Revision mSettingsRevision{};
        core::float4 mColor{ 1.0f, 0.55f, 0.08f, 1.0f };
        core::f32 mWidth{ 3.0f };
        core::u32 mFirstJumpStep{ 1 };
        core::u32 mDrawCount{};
        bool mEnabled{};
    };

    /** Outlines the union of active HighlightComponent meshes independently of picking.
     * Adapts Ben Golus's antialiased silhouette and separable jump-flood algorithm:
     * https://gist.github.com/bgolus/a18c1a3fc9af2d73cc19169a809eb195
     * GetRevision describes configuration changes, not animation or camera movement.
     * Geometry and silhouettes refresh each frame, even when configuration is unchanged.
     */
    class OutlineModule final : public core::RevisionTracked {
    public:
        static constexpr core::f32 kMaxOutlineWidth{ 1000.0f };
        explicit OutlineModule( rhi::RenderResolution resolution );

        auto SetGeometryManager( GeometryCullModule& geometry ) -> void;
        auto SetEnabled( bool enabled ) -> void;
        auto SetStyle( const core::float4& color, core::f32 width ) -> void;
        auto SetTextureDropHover( core::u32 id ) -> void { mTextureDropHover = id; }

        // Register in this order; each stage creates its own output resources.
        auto RegisterHighlightFilter( FrameGraph& graph ) -> void;
        /** Pass 0: rasterizes highlighted meshes into a per-sample interior mask.
         * The mask replaces stencil exclusion without changing the RHI stencil API.
         * Depth testing is disabled, matching the article's through-occluder outline.
         */
        auto RegisterInteriorMask( FrameGraph& graph, rhi::Multisampling samples ) -> void;
        // Pass 1: resolve the interior mask to silhouette coverage for Sobel initialization.
        auto RegisterSilhouette( FrameGraph& graph, rhi::Multisampling samples ) -> void;
        // Pass 2: initialize subpixel seed coordinates in RG16_SNORM.
        auto RegisterSeedInit( FrameGraph& graph ) -> void;
        // Pass 4: horizontal + vertical jumps at each descending power of two.
        auto RegisterJumpFlood( FrameGraph& graph ) -> void;
        // Pass 5: distance-based color, excluding selected interiors per MSAA sample.
        auto RegisterOutline( FrameGraph& graph, rhi::Multisampling samples ) -> void;
        // Register after PresentationModule's final image, before its transition pass.
        auto RegisterComposite( FrameGraph& graph ) -> void;

    private:
        GeometryCullModule* mGeometry{};
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        BufferUploadBatch mUploads{};
        eastl::vector<DrawIndirectCommand> mHighlightedDraws{};
        core::float4 mColor{ 1.0f, 0.55f, 0.08f, 1.0f };
        core::f32 mWidth{ 3.0f };
        bool mEnabled{ true };
        core::u32 mTextureDropHover{};
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_OUTLINE_MODULE_HH
