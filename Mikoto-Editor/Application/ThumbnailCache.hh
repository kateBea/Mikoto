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

#ifndef MIKOTO_APPLICATION_THUMBNAIL_CACHE_HH
#define MIKOTO_APPLICATION_THUMBNAIL_CACHE_HH

#include <EASTL/string.h>
#include <EASTL/shared_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

#include <Assets/Image.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/TextMaterial.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/ThumbnailRenderer.hh>
#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::editor {

    /** Deterministic CPU emitter snapshot, rasterized with the runtime's soft-disc appearance. */
    auto RenderParticlePreview( const renderer::ParticleEmitterSettings &settings ) -> asset::ImageHandle;


    /** @brief Mesh used to evaluate surface materials in the preview scene. */
    auto MaterialPreviewSpherePath() -> filesystem::Path;

    /** @brief Preview text color and opacity on a checkerboard using a neutral sample font.
     * A TextMaterial owns appearance, not font or layout. The sample therefore
     * shows its evaluated color without introducing surface lighting or exposure.
     */
    MKT_NODISCARD auto RenderTextMaterialPreview( const material::TextMaterial &material,
                                                  core::u32 width = 512, core::u32 height = 512 ) -> asset::ImageHandle;

    // Thumbnail info
    struct Thumbnail {
        renderer::rhi::TextureHandle mThumbnail{};
    };

    class ThumbnailCache {
    public:
        explicit ThumbnailCache(
                renderer::ThumbnailRenderer *renderer,
                renderer::rhi::IGpuDevice *device,
                eastl::string_view thumbNailsPath = "Assets/.cache/Thumbnails" );

        MKT_NODISCARD auto Contains( const filesystem::Path &path ) const -> bool;
        MKT_NODISCARD auto GetThumbnail( const filesystem::Path &path ) const -> Thumbnail;

        auto CreateThumbnailAsync( const filesystem::Path &path ) -> void;
        MKT_NODISCARD auto CreateThumbnail( const filesystem::Path &path ) -> Thumbnail;
        auto SetDirectory( const filesystem::Path &path ) -> void;
        auto UploadReady() -> void;

    private:
        // [Internal usage]
        struct CacheState;

    private:
        eastl::shared_ptr<CacheState> mState;

        renderer::rhi::IGpuDevice *mDevice{};
        renderer::ThumbnailRenderer *mThumbnailRenderer{};
    };

}// namespace mikoto::editor

#endif// MIKOTO_APPLICATION_THUMBNAIL_CACHE_HH
