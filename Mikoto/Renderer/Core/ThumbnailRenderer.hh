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

#ifndef MIKOTO_THUMBNAIL_RENDERER_HH
#define MIKOTO_THUMBNAIL_RENDERER_HH

#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <Scene/Scene.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/FrameGraph.hh>

#include <Assets/ShaderLibrary.hh>

namespace mikoto::renderer {

    struct ThumbnailRendererCreateInfo {
        rhi::IGpuDevice* mDevice{};
        eastl::string_view mName{};
        eastl::string_view mShaderBasePath{};
        eastl::string_view mAssetCacheThumbnails{ "Assets/.cache/Thumbnails" };

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        auto SetName( eastl::string_view name ) -> ThumbnailRendererCreateInfo&;
        auto SetDevice( rhi::IGpuDevice* device ) -> ThumbnailRendererCreateInfo&;
        auto SetAssetCachePathThumbnails( eastl::string_view path ) -> ThumbnailRendererCreateInfo&;
        auto SetShaderBasePath( eastl::string_view path ) -> ThumbnailRendererCreateInfo&;
        auto SetRenderResolution( rhi::RenderResolution resolution ) -> ThumbnailRendererCreateInfo&;
    };

    // Not sure how I will tackle this, my first idea is to submit the scenes with a single model
    // to this renderer, render the objects with low resolution and then save the result to a file in disk
    // and just use that saved image as the thumbnail
    class ThumbnailRenderer final : public IRenderer {
    public:
        explicit ThumbnailRenderer( const ThumbnailRendererCreateInfo& spec );

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Render( const scene::Scene* scene ) -> void override;

        // It creates the reduced version, there is no need
        // to render the full texture when displayed icon will be very small
        auto GenerateThumbnail( asset::ModelHandle model, core::i32 width, core::i32 height ) -> rhi::TextureHandle;
        auto GenerateThumbnail( asset::FontHandle model, core::i32 width, core::i32 height ) -> rhi::TextureHandle;
        auto GenerateThumbnail( rhi::TextureHandle original, const rhi::TextureSlice& slice ) -> rhi::TextureHandle;
        auto GenerateThumbnail( rhi::TextureHandle original, core::i32 width, core::i32 height ) -> rhi::TextureHandle;

        MKT_NODISCARD static auto Create( const ThumbnailRendererCreateInfo& spec ) -> eastl::unique_ptr<ThumbnailRenderer>;

    private:
        // [Internal usage]
        auto InitPasses() -> void;

    private:
        rhi::IGpuDevice* mDevice{};

        eastl::unique_ptr<FrameGraph> mFrameGraph{};
        eastl::unique_ptr<asset::ShaderLibrary> mShaderLibrary{};

        filesystem::Path mAssetCacheThumbnails{ "Assets/.cache/Thumbnails" };
    };

}// namespace Mikoto

#endif//MIKOTO_THUMBNAIL_RENDERER_HH
