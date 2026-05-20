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
        float4x4 mProjection{};
        float4x4 mView{};
        float4x4 mModel{};

        float4 mOutlineColor{};

        float4 mPosition{};
        float4 mSize{};
        float4 mColor{};

        float2 mTexCoords[4]{};

        f32 mOutlineWidth{};
        u32 mTextureAtlasID{};
    };

    struct TextVertexDescription {
        float3 mPosition{};
        u32 mTextureIndex{};
    };

    struct TextRenderingPassParameters {
        FGPipelineHandle mMsdfPipeline{};

        FGBufferHandle mMsdfTextRenderData{};
    };

    class TextRenderModule final {
    public:
        explicit TextRenderModule( RenderResolution resolution);

        auto SetScene(const scene::Scene* scene) -> void;
        auto SetCamera(const scene::Camera* camera) -> void;
        auto RegisterPasses(FrameGraph& graph) -> void;

    private:
        auto RegisterSlugPass(FrameGraph& graph) -> void;
        auto RegisterTextRender( FrameGraph& graph ) -> void;

        auto SetupTextRenderData( CommandContext& ctx, Blackboard& b ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        size_t mGlyphCount{};
        eastl::vector<TextDrawParameters> mTextInfo{};

        RenderResolution mResolution{ RenderResolution::e1080P };
    };

}// namespace Mikoto

#endif//MIKOTO_TEXT_RENDERING_HH
