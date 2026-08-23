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

#ifndef MIKOTO_TEXT_RENDERING_HH
#define MIKOTO_TEXT_RENDERING_HH

#include <EASTL/vector.h>
#include <EASTL/array.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Text/Unicode.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    inline constexpr u32 kMaxGlyphs{ 1024 * 1024 };

    struct TextDrawParameters {
        core::float4x4 mProjection{};
        core::float4x4 mView{};
        core::float4x4 mModel{};

        core::float4 mOutlineColor{};
        core::f32 mOutlineWidth{};

        core::float4 mPosition{};
        core::float4 mSize{};
        core::float4 mColor{};

        core::float2 mTexCoords[4]{};

        core::u32 mTextureAtlasID{};
    };

    struct TextVertexDescription {
        core::float3 mPosition{};
        core::u32 mTextureIndex{};
    };

    struct TextRenderingPassParameters {
        FGPipelineHandle mMsdfPipeline{};

        FGBufferHandle mMsdfTextRenderData{};
    };

    class TextRenderModule final {
    public:
        explicit TextRenderModule( RenderResolution resolution );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:
        auto RegisterSlugPass( FrameGraph& graph ) -> void;
        auto RegisterTextRender( FrameGraph& graph ) -> void;

        auto SetupTextRenderData( CommandContext& ctx, Blackboard& b ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        core::usize mGlyphCount{};
        eastl::vector<TextDrawParameters> mTextInfo{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };

}// namespace Mikoto

#endif//MIKOTO_TEXT_RENDERING_HH
