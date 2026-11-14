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

#ifndef MIKOTO_RENDERER_CORE_THUMBNAIL_RENDERER_HH
#define MIKOTO_RENDERER_CORE_THUMBNAIL_RENDERER_HH

#include <EASTL/shared_ptr.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <Filesystem/Path.hh>

#include <Assets/ShaderLibrary.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/ThumbnailPasses.hh>

#include <Scene/Scene.hh>

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

    class ThumbnailRenderer final : public IRenderer {
    public:
        explicit ThumbnailRenderer( const ThumbnailRendererCreateInfo& spec );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        // Render-thread API. One job is retained until its submission completes;
        // CPU model preparation is asynchronous; Poll never waits. The callback receives owned RGBA pixels for caching.
        // A supplied image bypasses model rendering and is uploaded directly.
        using PreviewCallback = eastl::function<void( asset::ImageHandle )>;
        auto BeginModelPreview( asset::ModelHandle model,
            const ThumbnailPreviewSettings& settings,
            material::MaterialHandle material = {},
            PreviewCallback callback = {},
            core::u32 width = 512, core::u32 height = 512, asset::ImageHandle image = {} ) -> bool;

        auto Poll() -> void;
        MKT_NODISCARD auto IsBusy() const -> bool;
        MKT_NODISCARD auto GetPreviewTexture() const -> rhi::TextureHandle;
        MKT_NODISCARD auto GetPreviewError() const -> eastl::string_view;

        MKT_NODISCARD auto UploadPreview( asset::ImageHandle image ) -> rhi::TextureHandle;

        // It creates the reduced version, there is no need
        // to render the full texture when displayed icon will be very small
        MKT_NODISCARD auto GenerateThumbnail( asset::ModelHandle model, core::i32 width, core::i32 height ) -> rhi::TextureHandle;
        MKT_NODISCARD auto GenerateThumbnail( renderer::FontHandle model, core::i32 width, core::i32 height ) -> rhi::TextureHandle;
        MKT_NODISCARD auto GenerateThumbnail( rhi::TextureHandle original, const rhi::TextureSlice& slice ) -> rhi::TextureHandle;
        MKT_NODISCARD auto GenerateThumbnail( rhi::TextureHandle original, core::i32 width, core::i32 height ) -> rhi::TextureHandle;

        MKT_NODISCARD static auto RenderTexturePreview( const asset::ImageHandle& source, core::u32 width, core::u32 height ) -> asset::ImageHandle;

        /**
         * @brief Decodes audio into separate white waveform bars on an opaque black background.
         * @param path Encoded audio file supported by the audio module's decoder.
         * @param width Preview width in pixels, from 1 through 4096.
         * @param height Preview height in pixels, from 1 through 4096.
         * @returns Owned RGBA8 pixels showing the complete clip's peak-normalized waveform.
         * @throws core::RuntimeException If audio decoding is unavailable or fails, or dimensions are invalid.
         * @details CPU-only and worker-safe. Channels contribute separate extrema so opposite-phase
         * stereo cannot cancel. Decoding uses bounded chunks rather than retaining the whole PCM clip.
         */
        MKT_NODISCARD static auto RenderAudioPreview( const filesystem::Path& path, core::u32 width = 512, core::u32 height = 512 ) -> asset::ImageHandle;

        MKT_NODISCARD static auto Create( const ThumbnailRendererCreateInfo& spec ) -> eastl::unique_ptr<ThumbnailRenderer>;

    private:
        struct Preparation;
        auto SubmitPreview( const ThumbnailPreviewSettings& settings, material::MaterialHandle material ) -> void;

        eastl::shared_ptr<Preparation> mPreparation{};
        rhi::IGpuDevice* mDevice{};
        eastl::string mShaderPath{};
        asset::ShaderLibrary* mShaderLibrary{};
        eastl::unique_ptr<ThumbnailPasses> mPasses{};
        eastl::unique_ptr<FrameGraph> mGraph{};
        asset::ModelHandle mPreviewModel{};
        asset::ImageHandle mCompletedImage{};
        rhi::TextureHandle mFlatPreview{};
        PreviewCallback mCallback{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        bool mBusy{};
        bool mCompleted{};
        eastl::string mError{};
        filesystem::Path mAssetCacheThumbnails{ "Assets/.cache/Thumbnails" };
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_THUMBNAIL_RENDERER_HH

