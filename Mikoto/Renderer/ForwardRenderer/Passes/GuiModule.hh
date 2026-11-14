//    Copyright 2025 ケイト
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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_GUI_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_GUI_MODULE_HH

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Interface/Canvas.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    class GuiModule final {
    public:
        explicit GuiModule( rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        auto SetGeometryManager( GeometryCullModule& geom ) -> void;

        auto RegisterUploadPass( FrameGraph& graph ) -> void;
        auto RegisterRenderPass( FrameGraph& graph ) -> void;

    private:
        struct Quad {
            core::float4 mPositions[4]{};
            core::float4 mColor{};
            core::float4 mUV{0, 0, 1, 1};
            core::float2 mSize{};
            core::f32 mRounding{};
            core::f32 mStroke{};
            core::u32 mTexture{};
            core::u32 mKind{};
            core::f32 mDistanceRange{};
            core::f32 mPadding{};
        };

        static constexpr core::usize kMaxQuads{ 65536 };

        auto Upload( CommandContext& ctx ) -> void;

    private:
        FGBufferHandle mBuffer{};
        FGSamplerHandle mSampler{};
        eastl::vector<Quad> mQuads{};
        core::u32 mWorldQuads{};
        FontHandle mDefaultFont{};
        bool mReportedCapacity{};
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryCullModule* mGeometryCullModule{};

        rhi::RenderResolution mResolution { rhi::RenderResolution::e1080P };
    };

}// namespace mikoto

#endif//MIKOTO_RENDERER_FORWARD_RENDERER_GUI_MODULE_HH

