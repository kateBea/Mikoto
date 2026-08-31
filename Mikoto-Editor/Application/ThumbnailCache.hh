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

#ifndef MIKOTOROOT_THUMBNAIL_CACHE_HH
#define MIKOTOROOT_THUMBNAIL_CACHE_HH

#include <EASTL/string.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Filesystem/Path.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <Assets/AssetsService.hh>

namespace mikoto::editor {

    // Thumbnail info
    struct Thumbnail {
        renderer::rhi::TextureHandle mThumbnail{};
    };

    class ThumbnailCache {
    public:

        explicit ThumbnailCache(renderer::rhi::IGpuDevice* device);

        MKT_NODISCARD auto Contains(const filesystem::Path& path ) const -> bool;
        MKT_NODISCARD auto GetThumbnail(const filesystem::Path& path) const -> Thumbnail;

        auto CreateThumbnailAsync(const filesystem::Path& path) -> void;
        MKT_NODISCARD auto CreateThumbnail(const filesystem::Path& path) -> Thumbnail;

    private:
        // [Internal usage]
        auto LoadTexture( const filesystem::Path &path ) -> renderer::rhi::TextureHandle;
        auto InsertThumbnail( const filesystem::Path &path, renderer::rhi::TextureHandle texture ) -> void;

    private:
        mutable std::mutex mMutex{};
        renderer::rhi::IGpuDevice* mDevice{};
        ankerl::unordered_dense::map<filesystem::Path, Thumbnail> mThumbnails{};
    };

}// namespace Mikoto

#endif//MIKOTOROOT_THUMBNAIL_CACHE_HH
