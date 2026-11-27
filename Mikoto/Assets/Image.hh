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

#ifndef MIKOTO_ASSETS_IMAGE_HH
#define MIKOTO_ASSETS_IMAGE_HH

#include <EASTL/unique_ptr.h>
#include <EASTL/fixed_hash_map.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

#include <Memory/BufferSpan.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

namespace mikoto::asset {

    enum class ImageFormat {
        eInvalid = -1,

        eCompressed_BC1,    //
        eCompressed_BC2,    //
        eCompressed_BC3,    //
        eCompressed_BC4,    //
        eCompressed_BC5,    //
        eCompressed_BC6,    //
        eCompressed_BC7,    // Compressed formats that will get loaded with ktx2

        eRGBA8_UINT,        // 8 bits per channel
        eRGBA32_FLOAT,      // 32-bit floating point
    };

    enum class ImageCubeFace {
        eTop,
        eBottom,
        eFront,
        eBack,
        eLeft,
        eRight,
    };

    constexpr inline core::u32 kCubeFaceCount{ 6 };

    // Flat textures, equirectangular maps
    struct Image : public core::ReferenceCounted<Image> {
        filesystem::FileHandle mFileHandle{};
        memory::BufferSpanHandle mBufferSpan{};

        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mChannels{};
        // Buffer contains tightly packed levels from mip zero to the last mip.
        core::u32 mMipCount{ 1 };

        ImageFormat mFormat{ ImageFormat::eRGBA8_UINT };

        ~Image() override = default;
    };

    // This abstraction extend support to help with loading
    // images from KTX or cube images split into 6 faces
    struct ImageCube final : public Image {
        struct FaceSlice {
            core::usize mOffset{};
            core::usize mSizeBytes{};
        };

        // When cube is list of 2D images in disk
        // this contains the base path of folder that contains all 6 images
        filesystem::Path mBasePath{};

        memory::BufferSpanHandle mBufferSpan{};
        eastl::fixed_hash_map<ImageCubeFace, FaceSlice, kCubeFaceCount> mFaces{};
    };

    using ImageHandle = core::Ref<Image>;

}// namespace Mikoto

#endif// MIKOTO_ASSETS_IMAGE_HH
