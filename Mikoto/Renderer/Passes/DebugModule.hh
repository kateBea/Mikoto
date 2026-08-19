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

#ifndef MIKOTO_DEBUG_PASSES_HH
#define MIKOTO_DEBUG_PASSES_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    struct TexturePassData {
        FGSamplerHandle mSampler{};

        FGTextureHandle mColorTarget{};
        FGTextureHandle mDepthTarget{};
        FGTextureHandle mImportedTexture{};

        FGPipelineHandle mPipeline{};
    };

    struct RenderModelPass {
        struct {
            core::float4x4 mModel{ math::constants::Identity<core::float4x4>() };
            core::float4x4 mView{};
            core::float4x4 mProjection{};
        } mCameraInfo{};

        FGTextureHandle mColorTarget{};
        FGTextureHandle mDepthTarget{};

        FGSamplerHandle mSampler{};

        FGBufferHandle mParameters{};

        FGPipelineHandle mPipeline{};
    };

    struct TrianglePassData {
        FGTextureHandle mColorTarget{};
        FGTextureHandle mDepthTarget{};

        FGPipelineHandle mPipeline{};
    };

    class DebugModule {
    public:
        explicit DebugModule( RenderResolution resolution );

        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:
        auto RegisterTrianglePass( FrameGraph& graph ) -> void;
        auto RegisterTexturePass( FrameGraph& graph ) -> void;
        auto RegisterSimpleComputePass( FrameGraph& graph ) -> void;

    private:
        RenderResolution mResolution{ RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif//MIKOTO_DEBUG_PASSES_HH