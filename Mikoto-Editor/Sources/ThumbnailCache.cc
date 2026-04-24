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

#include <Assets/ImageProcessor.hh>
#include <Application/ThumbnailCache.hh>

namespace mikoto::editor {

    ThumbnailCache::ThumbnailCache( GpuDevice *device )
        : mDevice{ device }
    {}

    auto ThumbnailCache::Contains( const filesystem::Path &path ) const -> bool {
        return !mThumbnails[path].IsEmpty();
    }

    auto ThumbnailCache::GetThumbnail( const filesystem::Path &path ) const -> Thumbnail {
        return Thumbnail{ .mThumbnail = mThumbnails[path] };
    }

    auto ThumbnailCache::CreateThumbnail( const filesystem::Path &path ) -> Thumbnail {
        FileHandle file{ FileService::Get()->LoadFile( path ) };
        TextureHandle thumbnail{ mThumbnails.LoadOrGet(path, [&]() -> TextureHandle {
            // This lambda runs ONLY once (per asset)
            ImageHandle image{ ProcessImage2D( path ) };
            auto textureDesc{ TextureCreateDescription{}
                .SetWidth( as<i32>( image->mWidth ) )
                .SetHeight( as<i32>( image->mHeight ) )
                .SetImageData( image )
                .SetDimensions( TextureDimension::eTexture2D )
                .SetMultisampling( Multisampling::eMsaaX1 )
                .SetUsage( TextureUsageFlagsBits::kShaderResource )
                .SetFormat( Format::eRGBA8_UNORM ) };

            auto result{ mDevice->CreateTexture( textureDesc ) };
            result->SetDebugName( "Thumbnail" );
            return result;
        })};

        return Thumbnail{ .mThumbnail = thumbnail };
    }

    auto ThumbnailCache::CreateThumbnailAsync( const filesystem::Path &path ) -> void {
        mThumbnails.RequestLoad( path, [this, path]() -> TextureHandle {
            return CreateThumbnail( path ).mThumbnail;
        } );
    }
}// namespace Mikoto