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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_DEBUG_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_DEBUG_MODULE_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::scene {
    class Scene;
}

namespace mikoto::renderer {

    enum class ArmatureDebugMode : core::u8 {
        eDisabled,
        eArmatureAndMesh,
        eArmatureOnly
    };

    struct ArmatureBone {
        core::float4 mAxisX{};
        core::float4 mAxisY{};
        core::float4 mAxisZ{};
        core::float4 mOrigin{};
        core::float4 mColor{};
    };

    struct TexturePassData {
        FGSamplerHandle mSampler{};

        FGTextureHandle mColorTarget{};
        FGTextureHandle mDepthTarget{};
        FGTextureHandle mImportedTexture{};
        FGBufferHandle mReadBackBuffer{};

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
        explicit DebugModule( renderer::rhi::RenderResolution resolution );

        auto SetMultisampling( rhi::Multisampling multisampling ) -> void;

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetArmatureMode( ArmatureDebugMode mode ) -> void;
        auto RegisterArmaturePass( FrameGraph& graph ) -> void;

        auto RegisterTrianglePass( FrameGraph& graph ) -> void;
        auto RegisterTexturePass( FrameGraph& graph ) -> void;
        auto RegisterSimpleComputePass( FrameGraph& graph ) -> void;

    private:
        auto GatherArmatures() -> void;

    private:
        static constexpr core::usize kMaxArmatureBones{ 32'768 };
        
        const scene::Scene* mScene{};
        
        eastl::vector<ArmatureBone> mArmatureBones{};
        ArmatureDebugMode mArmatureMode{ ArmatureDebugMode::eDisabled };
        
        renderer::rhi::RenderResolution mResolution{ renderer::rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_DEBUG_MODULE_HH
