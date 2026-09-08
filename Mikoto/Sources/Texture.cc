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

#include <Renderer/Rhi/Texture.hh>

namespace mikoto::renderer::rhi {

    auto ISampler::GetMinFilter() const -> SamplerFilter {
        return mMinFilter;
    }

    auto ISampler::GetMagFilter() const -> SamplerFilter {
        return mMagFilter;
    }

    auto ISampler::GetWrapU() const -> SamplerWrapMode {
        return mWrapU;
    }

    auto ISampler::GetWrapV() const -> SamplerWrapMode {
        return mWrapV;
    }

    auto ISampler::GetWrapW() const -> SamplerWrapMode {
        return mWrapW;
    }

    auto ISampler::GetMipLevels() const -> float {
        return mMipLevels;
    }

    auto ITexture::GetFormat() const -> Format {
        return mFormat;
    }

    auto ITexture::GetWidth() const -> core::u32 {
        return mWidth;
    }

    auto ITexture::GetHeight() const -> core::u32 {
        return mHeight;
    }

    auto ITexture::GetTextureUsage() const -> TextureUsageFlags {
        return mTextureUsage;
    }

    auto ITexture::GetImage() const -> asset::ImageHandle {
        return mImageData;
    }

    auto ITexture::GetSizeBytes() const -> core::usize {
        if ( mImageData && mImageData->mBufferSpan ) {
            return mImageData->mBufferSpan->GetSize();
        }

        return mBufferSpan ? mBufferSpan->GetSize() : 0;
    }

    auto ITexture::GetDimension() const -> TextureDimension {
        return mDimension;
    }

    auto ITexture::GetSampleCount() const -> Multisampling {
        return mMultisampling;
    }

    auto ITexture::GetMipLevelCount() const -> core::u32 {
        return mMipCount;
    }

}
