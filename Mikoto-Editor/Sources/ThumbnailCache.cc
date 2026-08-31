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

#include <Filesystem/FileService.hh>

#include <Threading/TaskService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <Assets/ImageProcessor.hh>
#include <Application/ThumbnailCache.hh>

namespace mikoto::editor {

    using namespace mikoto::asset;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    ThumbnailCache::ThumbnailCache( IGpuDevice *device )
        : mDevice{ device }
    {}

    auto ThumbnailCache::Contains( const filesystem::Path &path ) const -> bool {
        std::lock_guard lock{ mMutex };
        return mThumbnails.contains( path );
    }

    auto ThumbnailCache::GetThumbnail( const filesystem::Path &path ) const -> Thumbnail {
        std::lock_guard lock{ mMutex };
        const auto it{ mThumbnails.find( path ) };
        if (it != mThumbnails.end()) {
            return it->second;
        }

        return Thumbnail{ .mThumbnail = TextureHandle::CreateEmpty() };
    }

    auto ThumbnailCache::CreateThumbnail( const filesystem::Path &path ) -> Thumbnail {
        std::lock_guard lock{ mMutex };
        const auto it{ mThumbnails.find( path ) };
        if (it != mThumbnails.end()) {
            return it->second;
        }

        TextureHandle result{ LoadTexture( path ) };

        const auto itInsert{
            mThumbnails.try_emplace( path, Thumbnail{ .mThumbnail = result } ) };
        return itInsert.first->second;
    }

    auto ThumbnailCache::LoadTexture( const filesystem::Path &path ) -> renderer::rhi::TextureHandle {
        FileHandle file{ FileService::Get()->LoadFile( path ) };
        ImageHandle image{ ProcessImage2D( path ) };
        auto textureDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( image->mWidth ) )
            .SetHeight( as<i32>( image->mHeight ) )
            .SetImageData( image )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kShaderResource )
            .SetFormat( image->mFormat == ImageFormat::eRGBA8_UINT ? Format::eRGBA8_UNORM : Format::eRGBA32_FLOAT ) };

        TextureHandle result{ mDevice->CreateTexture( textureDesc ) };
        result->SetDebugName( string::Format( "Thumbnail {}", path.GetC_Str()) );

        return result;
    }

    auto ThumbnailCache::InsertThumbnail( const filesystem::Path &path, renderer::rhi::TextureHandle texture ) -> void {
        std::lock_guard lock{ mMutex };

        // Emplace is not used because the entry might already exist
        mThumbnails[path] = Thumbnail{ .mThumbnail = texture };
    }

    auto ThumbnailCache::CreateThumbnailAsync( const filesystem::Path &path ) -> void {
        std::lock_guard lock{ mMutex };
        const auto it{ mThumbnails.find( path ) };

        if (it == mThumbnails.end()) {
            // Does not exist, load.
            // Mark as existing before we offload the load
            mThumbnails[path] = Thumbnail{};

            threading::TaskService::Get()->Submit( [this, path]() -> void {
                TextureHandle result{ LoadTexture( path ) };
                InsertThumbnail( path, result );
            });
        }
    }
}// namespace Mikoto