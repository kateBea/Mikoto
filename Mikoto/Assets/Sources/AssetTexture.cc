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

#include <Core/Exception.hh>

#include <Assets/AssetTexture.hh>

#include <Renderer/Rhi/Texture.hh>

namespace mikoto::asset {

    auto TextureLoadDescription::SetPath( const filesystem::Path& path ) -> TextureLoadDescription& {
        MKT_BEGIN_PROFILER();
        mPath = path;
        return *this;
    }

    auto TextureLoadDescription::SetDimensions( renderer::rhi::TextureDimension dim ) -> TextureLoadDescription& {
        MKT_BEGIN_PROFILER();
        mDimension = dim;
        return *this;
    }


    Texture::Texture( core::Ref<renderer::rhi::ITexture> resource, AssetRef<Texture> reference )
        : mReference{ eastl::move( reference ) }, mResource{ eastl::move( resource ) } {
        if ( !mResource ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot construct Texture without a resource." );
        }
    }

    Texture::~Texture() = default;
    auto Texture::GetResource() const -> core::Ref<renderer::rhi::ITexture> {
        return mResource;
    }

    auto Texture::GetFormat() const -> renderer::rhi::Format {
        return mResource->GetFormat();
    }

    auto Texture::GetWidth() const -> core::u32 {
        return mResource->GetWidth();
    }

    auto Texture::GetHeight() const -> core::u32 {
        return mResource->GetHeight();
    }

    auto Texture::GetTextureUsage() const -> renderer::rhi::TextureUsageFlags {
        return mResource->GetTextureUsage();
    }

    auto Texture::GetImage() const -> asset::ImageHandle {
        return mResource->GetImage();
    }

    auto Texture::GetSourcePath() const -> const filesystem::Path& {
        return mReference.GetPath();
    }

    auto Texture::GetSizeBytes() const -> core::usize {
        return mResource->GetSizeBytes();
    }

    auto Texture::GetDimension() const -> renderer::rhi::TextureDimension {
        return mResource->GetDimension();
    }

    auto Texture::GetSampleCount() const -> renderer::rhi::Multisampling {
        return mResource->GetSampleCount();
    }

    auto Texture::GetMipLevelCount() const -> core::u32 {
        return mResource->GetMipLevelCount();
    }

    auto Texture::GetStorageSizeBytes() const -> core::usize {
        return mResource->GetStorageSizeBytes();
    }

    auto Texture::CanEvict() const -> bool {
        return mResource->GetRefCount() == 1;
    }

    auto Texture::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        return { sizeof( *this ) + mResource->GetSizeBytes(), mResource->GetStorageSizeBytes() };
    }
}// namespace mikoto::asset
