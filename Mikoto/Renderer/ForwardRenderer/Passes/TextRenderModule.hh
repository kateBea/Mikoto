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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_TEXT_RENDER_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_TEXT_RENDER_MODULE_HH

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Text/Utility.hh>
#include <Text/FontAtlas.hh>
#include <Text/TextLayout.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/Core/FrameGraph.hh>

#include <Scene/Camera.hh>
#include <Scene/Component.hh>



namespace mikoto::renderer {

    inline constexpr core::u32 kMaxGlyphs{ 65536 };

    struct alignas( 16 ) TextDrawParameters {
        core::float4x4 mProjection{};
        core::float4x4 mView{};
        core::float4x4 mModel{};

        core::float4 mPosition{};
        core::float4 mSize{};
        core::float4 mColor{};

        core::float2 mTexCoords[4]{};

        core::u32 mTextureAtlasID{};
        core::f32 mDistanceRange{};
        GlyphImageType mImageType{ GlyphImageType::eOutline };
        core::f32 mOutlineWidth{};
        core::float4 mGlyphPlaneBounds{};
        core::float4 mBandTransform{};
        core::u32 mGlyphBands[4]{};
        core::float2 mViewport{};
        core::u32 mCurveTextureID{};
        core::u32 mBandTextureID{};
        core::float4 mOutlineColor{};
        // SlugHorn's unfiltered curve list: texture, first texel, count, reserved.
        core::u32 mSlugStroke[4]{};
    };

    struct TextVertexDescription {
        core::float3 mPosition{};
        core::u32 mTextureIndex{};
    };

    struct TextRenderingPassParameters {
        FGPipelineHandle mMsdfPipeline{};
        FGPipelineHandle mScreenPipeline{};
        FGPipelineHandle mSlugWorldPipeline{};
        FGPipelineHandle mSlugScreenPipeline{};

        FGBufferHandle mMsdfTextRenderData{};
    };

    class TextRenderModule final {
    public:
        explicit TextRenderModule( renderer::rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto RegisterTextRender( FrameGraph& graph ) -> void;

        // Register after presentation so screen text stays independent of exposure and scene effects.
        auto RegisterScreenPass( FrameGraph& graph ) -> void;

    private:
        static auto CreatePipeline( const char* name, bool slug, bool world ) -> FGPipelineDescription;

        auto SetupTextRenderData( CommandContext& ctx, core::Blackboard& b ) -> void;

    private:
        struct DrawBatch {
            core::u32 mOffset{};
            core::u32 mCount{};
            bool mIsWorld{};
            bool mIsSlug{};
        };

        struct TextItem {
            const scene::TransformComponent* mTransform{};
            const scene::TextComponent* mText{};
        };

        struct GlyphRequest {
            eastl::vector<core::u32> mCharacters;
            bool mUsed{};
        };

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        bool mReportedCapacity{};
        core::usize mGlyphCount{};
        core::usize mWorldGlyphCount{};
        eastl::vector<TextDrawParameters> mTextInfo{};

        eastl::vector<DrawBatch> mBatches{};

        text::TextLayout mLayout{};
        eastl::vector<TextItem> mItems{};

        text::AtlasTextureBindings mAtlasBindings{};
        ankerl::unordered_dense::map<Font*, GlyphRequest> mGlyphRequests{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_TEXT_RENDER_MODULE_HH

