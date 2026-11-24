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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_VIDEO_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_VIDEO_MODULE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Video/Video.hh>

#include <Renderer/Core/FrameGraph.hh>

#include <Scene/Scene.hh>

namespace mikoto::renderer {

    // https://github.com/turanszkij/mini_video

    class VideoModule final {
    public:
        MKT_NODISCARD auto PrepareFrame( const scene::Scene* scene ) -> core::u64;
        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:
        auto GatherPlaybacks() -> void;
        auto GatherPresentationTextures( eastl::vector<rhi::TextureHandle>& textures, bool decodedInputs ) const -> void;
        auto GatherOutputTextures( eastl::vector<rhi::TextureHandle>& textures ) const -> void;
        auto RecordDecodeWork( CommandContext& context ) -> void;
        auto Present( CommandContext& context ) -> void;

    private:
        const scene::Scene* mScene{};
        eastl::vector<VideoRenderDataHandle> mPlaybacks{};

        FGVideoHandle mGathered{};
        FGVideoHandle mUploaded{};
        FGVideoHandle mDecoded{};
        FGVideoHandle mConverted{};
        FGVideoHandle mPresented{};
        FGPipelineHandle mPipeline{};
        FGSamplerHandle mSampler{};
    };
}

#endif
