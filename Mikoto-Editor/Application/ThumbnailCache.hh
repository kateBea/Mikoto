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

#include <ankerl/unordered_dense.h>

#include <Assets/Texture.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    // Thumbnail info
    struct Thumbnail {
        TextureHandle ThumbnailImage{};
    };

    class ThumbnailCache {
    public:

        MKT_NODISCARD auto GetThumbnail(const Path& path) const -> Thumbnail;
        MKT_NODISCARD auto CreateThumbnail(const Path& path) const -> Thumbnail;

        auto CreateThumbnailAsync(const Path& path) const -> void;

    private:
        ankerl::unordered_dense::map<std::string, Thumbnail> m_Textures2D{};
        // Create the renderer for models, render the scene in very low resolution and scale
    };

}// namespace Mikoto

#endif//MIKOTOROOT_THUMBNAIL_CACHE_HH
