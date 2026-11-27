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

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>

#include <EASTL/algorithm.h>
#include <EASTL/atomic.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <cmp_core.h>

#include <glm/gtc/packing.hpp>
#include <glm/gtx/compatibility.hpp>

#include <ktx.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <vulkan/vulkan_core.h>

#include <Core/Core.hh>
#include <Core/Exception.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/Path.hh>

#include <Assets/Asset.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/Utility.hh>

#include <Renderer/Rhi/GpuDevice.hh>

// Refs: https://github.com/dfranx/DDS

namespace mikoto::asset {

    constexpr core::ubyte kKtx2Signature[]{ 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

    constexpr core::u32 kBlockWidth{ 4 };
    constexpr core::usize kBlockBytes{ 16 };
    constexpr core::u32 kMaximumImageSize{ 16384 };
    constexpr core::usize kBlocksPerGroup{ 256 };
    constexpr core::usize kSerialBlockLimit{ 512 };

    struct CompressionActivity;

    struct CompressionActivities {
        std::mutex mMutex{};
        eastl::vector<const CompressionActivity*> mActive{};
    };

    static auto ActiveCompressions() -> CompressionActivities& {
        static CompressionActivities activities{};
        return activities;
    }

    MKT_NODISCARD static auto IsBlockCompressed( ImageFormat format ) -> bool {
        return format == ImageFormat::eCompressed_BC6 || format == ImageFormat::eCompressed_BC7;
    }

    MKT_NODISCARD static auto BlockBytes( core::u32 width, core::u32 height ) -> core::usize {
        return core::usize{ ( width + 3 ) / 4 } * ( ( height + 3 ) / 4 ) * kBlockBytes;
    }

    static auto ImageLevelBytes( ImageFormat format, core::u32 width, core::u32 height ) -> core::usize {
        return IsBlockCompressed( format ) ? BlockBytes( width, height ) : core::usize( width ) * height * 4 * ( format == ImageFormat::eRGBA32_FLOAT ? sizeof( core::f32 ) : 1 );
    }

    static auto ValidateImage( const ImageHandle& image ) -> void {
        if ( !image || !image->mBufferSpan || !image->mWidth || !image->mHeight ||
             image->mWidth > kMaximumImageSize || image->mHeight > kMaximumImageSize ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid image dimensions or missing pixels." );
        }

        const bool compressed{ IsBlockCompressed( image->mFormat ) };
        core::usize expected{};
        core::u32 width{ image->mWidth }, height{ image->mHeight };
        if ( !image->mMipCount || image->mMipCount > 32 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid image mip count." );
        }
        for ( core::u32 mip{}; mip < image->mMipCount; ++mip ) {
            expected += ImageLevelBytes( image->mFormat, width, height );
            if ( width == 1 && height == 1 && mip + 1 < image->mMipCount ) {
                MKT_THROW_RUNTIME_ERROR( "Image mip chain extends past one texel." );
            }
            width = eastl::max( 1u, width / 2 );
            height = eastl::max( 1u, height / 2 );
        }

        if ( image->mBufferSpan->GetSize() != expected || ( !compressed && image->mChannels != 4 ) ) {
            MKT_THROW_RUNTIME_ERROR( "Image pixel size does not match its dimensions and format." );
        }
    }

    // Register once per image. Block workers publish progress once per group,
    // keeping locks and UI access out of the inner encoding loop.
    struct CompressionActivity {
        TextureCompressionProgress mProgress{};
        eastl::atomic<core::usize> mCompleted{};

        CompressionActivity( const ImageHandle& image, core::usize blocks ) {
            mProgress.mPath = image->mFileHandle ? image->mFileHandle->GetPath() : filesystem::Path{};
            mProgress.mWidth = image->mWidth;
            mProgress.mHeight = image->mHeight;
            mProgress.mFormat = image->mFormat == ImageFormat::eRGBA32_FLOAT
                ? ImageFormat::eCompressed_BC6 : ImageFormat::eCompressed_BC7;
            mProgress.mTotalBlocks = blocks;

            auto& activities{ ActiveCompressions() };
            const std::lock_guard lock{ activities.mMutex };
            activities.mActive.push_back( this );
        }

        ~CompressionActivity() {
            auto& activities{ ActiveCompressions() };
            const std::lock_guard lock{ activities.mMutex };
            activities.mActive.erase( eastl::find( activities.mActive.begin(), activities.mActive.end(), this ) );
        }

        DISABLE_COPY_AND_MOVE_FOR( CompressionActivity );
    };

    // Each encoding job owns its options and reuses them across block groups.
    struct BlockCodec : public core::INonCopyable {
        void* mOptions{};
        bool mHdr{};

        explicit BlockCodec( bool hdr )
            : mHdr{ hdr } {
            int status{};

            if ( hdr ) {
                status = CreateOptionsBC6( &mOptions );
            } else {
                // The pinned codec initializes shared BC7 ramp tables behind an
                // unsynchronized flag. Serialize option creation, not encoding.
                static std::mutex initializationMutex;
                const std::lock_guard lock{ initializationMutex };
                status = CreateOptionsBC7( &mOptions );
            }

            if ( status != 0 ) {
                MKT_THROW_RUNTIME_ERROR( "Could not create Compressonator block codec." );
            }

            if ( hdr ) {
                SetQualityBC6( mOptions, 0.2f );
                SetSignedBC6( mOptions, false );
            } else {
                SetQualityBC7( mOptions, 0.5f );
                SetAlphaOptionsBC7( mOptions, true, false, false );
            }
        }

        ~BlockCodec() {
            if ( mHdr ) {
                DestroyOptionsBC6( mOptions );
            } else {
                DestroyOptionsBC7( mOptions );
            }
        }
    };

    struct KtxDeleter {
        auto operator()( ktxTexture2* texture ) const -> void {
            ktxTexture_Destroy( ktxTexture( texture ) );
        }
    };
    using KtxImagePtr = eastl::unique_ptr<ktxTexture2, KtxDeleter>;

    static auto CheckKtx( KTX_error_code status ) -> void {
        if ( status != KTX_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Ktx2 error: {}", ktxErrorString( status ) ) );
        }
    }

    MKT_NODISCARD static auto IsKtx2( const void* data, core::usize size ) -> bool {
        return data && size >= sizeof( kKtx2Signature ) &&
               std::memcmp( data, kKtx2Signature, sizeof( kKtx2Signature ) ) == 0;
    }

    static auto WriteImageUint8( const filesystem::Path& path, const void* data, core::u32 width, core::u32 height ) -> void {
        MKT_BEGIN_PROFILER();
        constexpr core::u32 kChannelNumber{ 4 };

        if ( !stbi_write_png( path.GetAbsolute().c_str(), width, height, kChannelNumber, data, width * kChannelNumber ) ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Could not write PNG image: {}", path.GetAbsolute() ) );
        }
    }

    static auto WriteImageFloat32( const filesystem::Path& path, const void* data, core::u32 width, core::u32 height ) -> void {
        MKT_BEGIN_PROFILER();
        constexpr core::u32 kChannelNumber{ 4 };

        if ( !stbi_write_hdr( path.GetAbsolute().c_str(), width, height, kChannelNumber, core::as<const float*>( data ) ) ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Could not write HDR image: {}", path.GetAbsolute() ) );
        }
    }


    auto GetTextureCompressionProgress() -> eastl::vector<TextureCompressionProgress> {
        auto& activities{ ActiveCompressions() };
        const std::lock_guard lock{ activities.mMutex };
        eastl::vector<TextureCompressionProgress> result{};
        result.reserve( activities.mActive.size() );

        for ( const auto* activity: activities.mActive ) {
            auto& progress{ result.emplace_back( activity->mProgress ) };
            progress.mCompletedBlocks = activity->mCompleted.load( eastl::memory_order_relaxed );
        }

        return result;
    }

    auto ProcessImage2D( const filesystem::Path& filepath, bool preserve16Bit ) -> ImageHandle {
        MKT_BEGIN_PROFILER();
        filesystem::FileHandle fileHandle{ filesystem::FileService::Get()->LoadFile( filepath ) };
        return ProcessImage2D( fileHandle, preserve16Bit );
    }


    auto WriteImage( const filesystem::Path& path, const void* data, core::u32 width, core::u32 height, ImageFormat format ) -> void {
        MKT_BEGIN_PROFILER();

        switch ( format ) {
            case ImageFormat::eRGBA8_UINT:
                WriteImageUint8( path, data, width, height );
                break;
            case ImageFormat::eRGBA32_FLOAT:
                WriteImageFloat32( path, data, width, height );
                break;
            default:;
        }
    }

    auto ProcessImage2D( const void* data, core::usize sizeBytes, ImageFormat format ) -> ImageHandle {
        MKT_BEGIN_PROFILER();

        if ( !data || !sizeBytes || sizeBytes > static_cast<core::usize>( std::numeric_limits<int>::max() ) ) {
            MKT_THROW_RUNTIME_ERROR( "Image data is empty or exceeds the decoder size limit." );
        }
        // Container contents determine the format, not the filename or stb output hint.
        if ( IsKtx2( data, sizeBytes ) ) {
            return ProcessImageCompressed( data, sizeBytes );
        }

        const auto* encoded{ static_cast<const stbi_uc*>( data ) };
        const auto size{ static_cast<int>( sizeBytes ) };
        core::i32 width{};
        core::i32 height{};
        core::i32 channels{};

        if ( !stbi_info_from_memory( encoded, size, &width, &height, &channels ) ||
             width <= 0 || height <= 0 || width > 16384 || height > 16384 ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported image header or dimensions (maximum 16384)." );
        }

        if ( format != ImageFormat::eRGBA8_UINT && format != ImageFormat::eRGBA32_FLOAT ) {
            MKT_THROW_RUNTIME_ERROR( "stb image decoding requires RGBA8 or RGBA32F output." );
        }

        const bool hdr{ format == ImageFormat::eRGBA32_FLOAT };

        if ( hdr && stbi_is_16_bit_from_memory( encoded, size ) ) {
            // stbi_loadf promotes LDR through its color/gamma conversion path.
            // Decode 16-bit numerical data directly to retain every input level.
            const auto release16{ []( stbi_us* pixels ) {
                stbi_image_free( pixels );
            } };
            eastl::unique_ptr<stbi_us, decltype( release16 )> pixels{
                stbi_load_16_from_memory( encoded, size, &width, &height, &channels, STBI_rgb_alpha ), release16
            };

            if ( !pixels ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Image decode failed: {}", stbi_failure_reason() ) );
            }

            eastl::vector<core::f32> samples( static_cast<core::usize>( width ) * height * 4 );

            for ( core::usize i{}; i < samples.size(); ++i ) {
                samples[i] = pixels.get()[i] / 65535.0f;
            }

            auto result{ ImageHandle::New() };
            result->mWidth = width;
            result->mHeight = height;
            result->mChannels = 4;
            result->mFormat = ImageFormat::eRGBA32_FLOAT;
            result->mBufferSpan = memory::BufferSpanHandle::New( samples.data(), samples.size() * sizeof( core::f32 ) );
            return result;
        }

        void* decoded{ hdr ? static_cast<void*>( stbi_loadf_from_memory( encoded, size, &width, &height, &channels, STBI_rgb_alpha ) ) : 
            static_cast<void*>( stbi_load_from_memory( encoded, size, &width, &height, &channels, STBI_rgb_alpha ) ) };

        if ( !decoded ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Image decode failed: {}", stbi_failure_reason() ) );
        }

        const auto release{ []( core::ubyte* pixels ) {
            stbi_image_free( pixels );
        } };
        eastl::unique_ptr<core::ubyte, decltype( release )> pixels{ static_cast<core::ubyte*>( decoded ), release };

        auto result{ ImageHandle::New() };
        result->mWidth = width;
        result->mHeight = height;
        result->mChannels = 4;
        result->mFormat = format;
        result->mBufferSpan = memory::BufferSpanHandle::New( decoded, core::usize{ static_cast<core::u32>( width ) } * height * 4 * ( hdr ? sizeof( core::f32 ) : 1 ) );
        return result;
    }

    auto ProcessImage2D( filesystem::FileHandle file, bool preserve16Bit ) -> ImageHandle {
        MKT_BEGIN_PROFILER();
        ImageHandle result{ ImageHandle::CreateEmpty() };

        if ( file.IsEmpty() ) {
            return result;
        }

        const auto size{ file->GetContentsString().size() };

        if ( !size || size > static_cast<core::usize>( std::numeric_limits<int>::max() ) ) {
            MKT_THROW_RUNTIME_ERROR( "Image file is empty or exceeds the decoder size limit." );
        }

        const auto* bytes{ reinterpret_cast<const stbi_uc*>( file->GetContentsBytes() ) };
        const bool hdr{ stbi_is_hdr_from_memory( bytes, static_cast<int>( size ) ) != 0 ||
                        ( preserve16Bit && stbi_is_16_bit_from_memory( bytes, static_cast<int>( size ) ) != 0 ) };
        result = ProcessImage2D( bytes, size, hdr ? ImageFormat::eRGBA32_FLOAT : ImageFormat::eRGBA8_UINT );
        result->mFileHandle = file;

        return result;
    }

    auto SetTextureCompressionWorkerCount( core::u32 count ) -> void {
        SetImportWorkerCount( count );
    }

    auto GetTextureCompressionWorkerCount() -> core::u32 {
        return GetImportWorkerCount();
    }

    auto GetTextureFormat( ImageFormat format ) -> renderer::rhi::Format {
        using renderer::rhi::Format;

        switch ( format ) {
            case ImageFormat::eRGBA8_UINT:
                return renderer::rhi::Format::eRGBA8_UNORM;
            case ImageFormat::eRGBA32_FLOAT:
                return renderer::rhi::Format::eRGBA32_FLOAT;
            case ImageFormat::eCompressed_BC6:
                return renderer::rhi::Format::eBC6H_UFLOAT;
            case ImageFormat::eCompressed_BC7:
                return renderer::rhi::Format::eBC7_UNORM;
            default:
                MKT_THROW_RUNTIME_ERROR( "Unsupported texture image format." );
        }
    }

    // Apply existing codecs to each level independently; storage stays mip-major.
    static auto ConvertMipChain( const ImageHandle &image, ImageHandle ( *convert )( const ImageHandle & ) ) -> ImageHandle {
        auto result{ ImageHandle::New() };
        result->mFileHandle = image->mFileHandle;
        result->mWidth = image->mWidth;
        result->mHeight = image->mHeight;
        result->mMipCount = image->mMipCount;
        eastl::vector<core::ubyte> pixels{};
        core::usize offset{};
        core::u32 width{ image->mWidth }, height{ image->mHeight };
        for ( core::u32 mip{}; mip < image->mMipCount; ++mip ) {
            auto level{ ImageHandle::New() };
            level->mWidth = width;
            level->mHeight = height;
            level->mChannels = image->mChannels;
            level->mFormat = image->mFormat;
            const auto bytes{ ImageLevelBytes( image->mFormat, width, height ) };
            level->mBufferSpan = memory::BufferSpanHandle::New( static_cast<const core::ubyte *>( image->mBufferSpan->GetData() ) + offset, bytes );
            level = convert( level );
            if ( mip && result->mFormat != level->mFormat ) {
                // HDR alpha/range fallback must be consistent across the chain.
                return image;
            }
            result->mFormat = level->mFormat;
            result->mChannels = level->mChannels;
            const auto *data{ static_cast<const core::ubyte *>( level->mBufferSpan->GetData() ) };
            pixels.insert( pixels.end(), data, data + level->mBufferSpan->GetSize() );
            offset += bytes;
            width = eastl::max( 1u, width / 2 );
            height = eastl::max( 1u, height / 2 );
        }
        result->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
        return result;
    }

    auto CompressImage( const ImageHandle& image ) -> ImageHandle {
        MKT_BEGIN_PROFILER();
        ValidateImage( image );
        if ( image->mMipCount > 1 ) {
            return ConvertMipChain( image, CompressImage );
        }

        if ( IsBlockCompressed( image->mFormat ) ) {
            return image;
        }

        if ( image->mFormat != ImageFormat::eRGBA8_UINT && image->mFormat != ImageFormat::eRGBA32_FLOAT ) {
            MKT_THROW_RUNTIME_ERROR( "Compressonator expects RGBA8 or RGBA32F pixels." );
        }

        const bool hdr{ image->mFormat == ImageFormat::eRGBA32_FLOAT };
        const auto* floats{ static_cast<const core::f32*>( image->mBufferSpan->GetData() ) };
        const auto* bytes{ static_cast<const core::ubyte*>( image->mBufferSpan->GetData() ) };

        if ( hdr ) {
            for ( core::usize pixel{}; pixel < core::usize{ image->mWidth } * image->mHeight; ++pixel ) {
                // BC6H has RGB only. Retain float pixels for HDR textures with alpha.
                if ( floats[pixel * 4 + 3] != 1.0f ) {
                    return image;
                }

                for ( core::usize channel{}; channel < 3; ++channel ) {
                    // BC6H UF16 cannot retain negative/out-of-range RGB. Its
                    // pinned CPU decoder also lacks signed BC6H support.
                    const core::f32 value{ floats[pixel * 4 + channel] };

                    if ( value < 0.0f || value > 65504.0f ) {
                        return image;
                    }
                }
            }
        }

        eastl::vector<core::ubyte> blocks( BlockBytes( image->mWidth, image->mHeight ) );
        const core::usize blockCount{ blocks.size() / kBlockBytes };
        const core::u32 blocksPerRow{ ( image->mWidth + kBlockWidth - 1 ) / kBlockWidth };
        CompressionActivity activity{ image, blockCount };

        const auto encodeGroup{ [&]( const BlockCodec& codec, core::usize begin, core::usize end ) {
            for ( core::usize block{ begin }; block < end; ++block ) {
                const core::u32 bx{ static_cast<core::u32>( block % blocksPerRow ) * kBlockWidth };
                const core::u32 by{ static_cast<core::u32>( block / blocksPerRow ) * kBlockWidth };

                core::ubyte rgba[64]{};
                core::u16 rgbHalf[48]{};

                for ( core::u32 y{}; y < kBlockWidth; ++y ) {
                    for ( core::u32 x{}; x < kBlockWidth; ++x ) {
                        const core::usize pixel{ core::usize{ eastl::min( by + y, image->mHeight - 1 ) } * image->mWidth +
                                                 eastl::min( bx + x, image->mWidth - 1 ) };

                        if ( hdr ) {
                            for ( core::u32 channel{}; channel < 3; ++channel ) {
                                const core::f32 value{ floats[pixel * 4 + channel] };
                                rgbHalf[( y * 4 + x ) * 3 + channel] = glm::packHalf1x16( value );
                            }
                        } else {
                            eastl::copy_n( bytes + pixel * 4, 4, rgba + ( y * 4 + x ) * 4 );
                        }
                    }
                }

                auto* destination{ blocks.data() + block * kBlockBytes };

                // The BC6 core API consumes packed RGB half floats, with stride in shorts.
                const auto status{ hdr ? CompressBlockBC6( rgbHalf, 12, destination, codec.mOptions ) : 
                    CompressBlockBC7( rgba, 16, destination, codec.mOptions ) };

                if ( status != 0 ) {
                    MKT_THROW_RUNTIME_ERROR( "Compressonator failed to encode a texture block." );
                }
            }

            activity.mCompleted.fetch_add( end - begin, eastl::memory_order_relaxed );
        } };

        if ( blockCount <= kSerialBlockLimit ) {
            const BlockCodec codec{ hdr };
            encodeGroup( codec, 0, blockCount );
        } else {
            const core::usize groupCount{ ( blockCount + kBlocksPerGroup - 1 ) / kBlocksPerGroup };
            RunImportJobs( groupCount, [&]( core::usize group ) {
                const BlockCodec codec{ hdr };
                const core::usize begin{ group * kBlocksPerGroup };
                encodeGroup( codec, begin, eastl::min( begin + kBlocksPerGroup, blockCount ) );
            } );
        }

        auto result{ ImageHandle::New() };
        result->mWidth = image->mWidth;
        result->mHeight = image->mHeight;
        result->mChannels = hdr ? 3 : 4;
        result->mFormat = hdr ? ImageFormat::eCompressed_BC6 : ImageFormat::eCompressed_BC7;
        result->mFileHandle = image->mFileHandle;
        result->mBufferSpan = memory::BufferSpanHandle::New( blocks.data(), blocks.size() );
        return result;
    }

    auto DecompressImage( const ImageHandle& image ) -> ImageHandle {
        MKT_BEGIN_PROFILER();
        ValidateImage( image );
        if ( image->mMipCount > 1 ) {
            return ConvertMipChain( image, DecompressImage );
        }

        if ( !IsBlockCompressed( image->mFormat ) ) {
            return image;
        }

        const bool hdr{ image->mFormat != ImageFormat::eCompressed_BC7 };
        BlockCodec codec{ hdr };
        eastl::vector<core::ubyte> pixels( core::usize{ image->mWidth } * image->mHeight * 4 * ( hdr ? sizeof( core::f32 ) : 1 ) );
        const auto* blocks{ static_cast<const core::ubyte*>( image->mBufferSpan->GetData() ) };

        for ( core::u32 by{}; by < image->mHeight; by += 4 ) {
            for ( core::u32 bx{}; bx < image->mWidth; bx += 4 ) {
                core::ubyte rgba[64]{};
                core::u16 rgbHalf[48]{};
                const auto* block{ blocks + ( core::usize{ by / 4 } * ( ( image->mWidth + 3 ) / 4 ) + bx / 4 ) * kBlockBytes };
                const auto status{ hdr ? DecompressBlockBC6( block, rgbHalf, codec.mOptions ) : DecompressBlockBC7( block, rgba, codec.mOptions ) };

                if ( status != 0 ) {
                    MKT_THROW_RUNTIME_ERROR( "Compressonator failed to decode a texture block." );
                }

                for ( core::u32 y{}; y < 4 && by + y < image->mHeight; ++y ) {
                    for ( core::u32 x{}; x < 4 && bx + x < image->mWidth; ++x ) {
                        const core::usize pixel{ core::usize{ by + y } * image->mWidth + bx + x };

                        if ( hdr ) {
                            core::f32 color[4]{ 0, 0, 0, 1 };

                            for ( core::u32 channel{}; channel < 3; ++channel ) {
                                color[channel] = glm::unpackHalf1x16( rgbHalf[( y * 4 + x ) * 3 + channel] );
                            }

                            std::memcpy( pixels.data() + pixel * sizeof( color ), color, sizeof( color ) );
                        }
                        else {
                            eastl::copy_n( rgba + ( y * 4 + x ) * 4, 4, pixels.data() + pixel * 4 );
                        }
                    }
                }
            }
        }

        auto result{ ImageHandle::New() };
        result->mWidth = image->mWidth;
        result->mHeight = image->mHeight;
        result->mChannels = 4;
        result->mFormat = hdr ? ImageFormat::eRGBA32_FLOAT : ImageFormat::eRGBA8_UINT;
        result->mFileHandle = image->mFileHandle;
        result->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
        return result;
    }

    auto WriteImageCompressed( const filesystem::Path& path, const ImageHandle& image ) -> void {
        ValidateImage( image );
        // VkFormat values are the KTX2 wire format, independent of the active RHI.
        ktxTextureCreateInfo info{};

        switch ( image->mFormat ) {
            case ImageFormat::eCompressed_BC6:
                info.vkFormat = VK_FORMAT_BC6H_UFLOAT_BLOCK;
                break;
            case ImageFormat::eCompressed_BC7:
                info.vkFormat = VK_FORMAT_BC7_UNORM_BLOCK;
                break;
            case ImageFormat::eRGBA8_UINT:
                info.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
                break;
            case ImageFormat::eRGBA32_FLOAT:
                info.vkFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;
            default:
                MKT_THROW_RUNTIME_ERROR( "KTX2 cache expects BC6H, BC7 or uncompressed RGBA." );
        }

        info.baseWidth = image->mWidth;
        info.baseHeight = image->mHeight;
        info.baseDepth = 1;
        info.numDimensions = 2;
        info.numLevels = image->mMipCount;
        info.numLayers = 1;
        info.numFaces = 1;

        // Let the KTX container own a copy of the source pixels before encoding it.
        ktxTexture2* texture{};
        CheckKtx( ktxTexture2_Create( &info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture ) );
        KtxImagePtr owned{ texture };
        core::usize offset{};
        core::u32 width{ image->mWidth }, height{ image->mHeight };
        for ( core::u32 mip{}; mip < image->mMipCount; ++mip ) {
            const auto size{ ImageLevelBytes( image->mFormat, width, height ) };
            CheckKtx( ktxTexture_SetImageFromMemory( ktxTexture( texture ), mip, 0, 0,
                static_cast<const ktx_uint8_t *>( image->mBufferSpan->GetData() ) + offset, size ) );
            offset += size;
            width = eastl::max( 1u, width / 2 );
            height = eastl::max( 1u, height / 2 );
        }

        // Retain the encoded allocation until the complete cache write succeeds.
        ktx_uint8_t* bytes{};
        ktx_size_t size{};
        CheckKtx( ktxTexture_WriteToMemory( ktxTexture( texture ), &bytes, &size ) );
        const auto release{ []( ktx_uint8_t* data ) {
            std::free( data );
        } };
        eastl::unique_ptr<ktx_uint8_t, decltype( release )> data{ bytes, release };

        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( path ) };

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open compressed texture file for writing." );
        }

        file->Write( bytes, size );
    }

    auto ProcessImageCompressed( filesystem::FileHandle file ) -> ImageHandle {
        if ( !file ) {
            MKT_THROW_RUNTIME_ERROR( "Missing KTX2 image file." );
        }

        return ProcessImageCompressed( file->GetContentsBytes(), file->GetContentsString().size() );
    }

    auto ProcessImageCompressed( const void* data, core::usize sizeBytes ) -> ImageHandle {
        if ( !IsKtx2( data, sizeBytes ) ) {
            MKT_THROW_RUNTIME_ERROR( "Expected a KTX2 image signature." );
        }

        ktxTexture2* texture{};
        CheckKtx( ktxTexture2_CreateFromMemory( static_cast<const ktx_uint8_t*>( data ),
            sizeBytes, KTX_TEXTURE_CREATE_NO_FLAGS, &texture ) );
        KtxImagePtr owned{ texture };

        if ( texture->numDimensions != 2 || !texture->numLevels || texture->numLevels > 32 || texture->numLayers != 1 ||
             texture->numFaces != 1 || texture->isArray || !texture->baseWidth || !texture->baseHeight ||
             texture->baseWidth > kMaximumImageSize || texture->baseHeight > kMaximumImageSize ||
             texture->supercompressionScheme != KTX_SS_NONE ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported KTX2 image layout (requires 2D without supercompression)." );
        }

        auto image{ ImageHandle::New() };

        switch ( texture->vkFormat ) {
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
                image->mFormat = ImageFormat::eCompressed_BC6;
                break;
            case VK_FORMAT_BC7_UNORM_BLOCK:
                image->mFormat = ImageFormat::eCompressed_BC7;
                break;
            case VK_FORMAT_R8G8B8A8_UNORM:
                image->mFormat = ImageFormat::eRGBA8_UINT;
                break;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                image->mFormat = ImageFormat::eRGBA32_FLOAT;
                break;
            default:
                MKT_THROW_RUNTIME_ERROR( "Unsupported KTX2 image format." );
        }

        CheckKtx( ktxTexture_LoadImageData( ktxTexture( texture ), nullptr, 0 ) );
        image->mWidth = texture->baseWidth;
        image->mHeight = texture->baseHeight;
        image->mMipCount = texture->numLevels;
        image->mChannels = image->mFormat == ImageFormat::eCompressed_BC6 ? 3 : 4;
        eastl::vector<core::ubyte> pixels{};
        core::u32 width{ image->mWidth }, height{ image->mHeight };
        for ( core::u32 mip{}; mip < image->mMipCount; ++mip ) {
            const auto size{ ImageLevelBytes( image->mFormat, width, height ) };
            ktx_size_t offset{};
            CheckKtx( ktxTexture_GetImageOffset( ktxTexture( texture ), mip, 0, 0, &offset ) );
            if ( ktxTexture_GetImageSize( ktxTexture( texture ), mip ) != size || offset > texture->dataSize || size > texture->dataSize - offset ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid KTX2 mip byte count." );
            }
            pixels.insert( pixels.end(), texture->pData + offset, texture->pData + offset + size );
            width = eastl::max( 1u, width / 2 );
            height = eastl::max( 1u, height / 2 );
        }
        image->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
        ValidateImage( image );
        return image;
    }

    auto LoadTextureImage( const filesystem::Path& source, const filesystem::Path& cacheDirectory, bool compress ) -> ImageHandle {
        MKT_BEGIN_PROFILER();

        if ( !compress ) {
            return ProcessImage2D( filesystem::FileHandle::New( source ) );
        }

        const auto native{ source.GetPathTyped<std::filesystem::path>() };
        // Inspect only the header before looking in the derived cache. A native
        // KTX2 source already contains uploadable pixels/blocks and needs no re-encoding.
        core::ubyte header[sizeof( kKtx2Signature )]{};
        std::ifstream input{ native, std::ios::binary };
        input.read( reinterpret_cast<char*>( header ), sizeof( header ) );

        if ( IsKtx2( header, static_cast<core::usize>( input.gcount() ) ) ) {
            return ProcessImage2D( filesystem::FileHandle::New( source ) );
        }

        const auto modified{ std::filesystem::last_write_time( native ).time_since_epoch().count() };
        const auto size{ std::filesystem::file_size( native ) };
        const filesystem::Path cached{ cacheDirectory.GetPathTyped<std::filesystem::path>() /
                                       string::Format( "{}", GetHashedAssetID( source ) ).c_str() /
                                       string::Format( "texture-v2-{}-{}.ktx2", modified, size ).c_str() };

        if ( cached.Exists() ) {
            try {
                return ProcessImageCompressed( filesystem::FileHandle::New( cached ) );
            } catch ( const std::exception& error ) {
                MKT_CORE_LOGGER_WARN( "Rebuilding texture cache '{}': {}", cached.GetC_Str(), error.what() );
            }
        }

        auto image{ CompressImage( GenerateImageMipChain( ProcessImage2D( filesystem::FileHandle::New( source ) ) ) ) };

        try {
            WriteImageCompressed( cached, image );
        } catch ( const std::exception& error ) {
            // A read-only/full cache must not prevent using the source texture.
            MKT_CORE_LOGGER_WARN( "Cannot cache texture '{}': {}", source.GetC_Str(), error.what() );
        }

        return image;
    }

    // Integrate the complete source footprint of each output texel. Fractional
    // boundary weights keep the last source row/column in odd-sized reductions.
    // memcpy also permits unaligned float buffers without type-punning their bytes.
    auto FilterImageMipLevel( const void *source, core::u32 width, core::u32 height,
        void *destination, core::u32 nextWidth, core::u32 nextHeight, bool floating ) -> void {
        const auto *src{ static_cast<const core::ubyte *>( source ) };
        auto *dst{ static_cast<core::ubyte *>( destination ) };
        const core::usize pixelBytes{ 4 * ( floating ? sizeof( float ) : 1 ) };
        for ( core::u32 y{}; y < nextHeight; ++y ) {
            const double top{ double( y ) * height / nextHeight }, bottom{ double( y + 1 ) * height / nextHeight };
            for ( core::u32 x{}; x < nextWidth; ++x ) {
                const double left{ double( x ) * width / nextWidth }, right{ double( x + 1 ) * width / nextWidth };
                double sum[4]{};
                for ( core::u32 sy{ core::u32( top ) }; sy < core::u32( std::ceil( bottom ) ); ++sy ) {
                    for ( core::u32 sx{ core::u32( left ) }; sx < core::u32( std::ceil( right ) ); ++sx ) {
                        const double weight{ ( std::min( right, double( sx + 1 ) ) - std::max( left, double( sx ) ) ) *
                                             ( std::min( bottom, double( sy + 1 ) ) - std::max( top, double( sy ) ) ) };
                        const auto index{ ( core::usize( sy ) * width + sx ) * pixelBytes };
                        for ( core::u32 c{}; c < 4; ++c ) {
                            float value{};
                            if ( floating ) {
                                std::memcpy( &value, src + index + c * sizeof( float ), sizeof( float ) );
                            }
                            else {
                                value = src[index + c];
                            }
                            sum[c] += value * weight;
                        }
                    }
                }
                const auto output{ ( core::usize( y ) * nextWidth + x ) * pixelBytes };
                for ( core::u32 c{}; c < 4; ++c ) {
                    const float value{ float( sum[c] / ( ( right - left ) * ( bottom - top ) ) ) };
                    if ( floating ) {
                        std::memcpy( dst + output + c * sizeof( float ), &value, sizeof( float ) );
                    }
                    else {
                        dst[output + c] = core::ubyte( std::clamp( std::round( value ), 0.0f, 255.0f ) );
                    }
                }
            }
        }
    }

    auto GenerateImageMipChain( const ImageHandle &image ) -> ImageHandle {
        ValidateImage( image );
        if ( image->mMipCount > 1 || ( image->mWidth == 1 && image->mHeight == 1 ) ) {
            return image;
        }
        if ( IsBlockCompressed( image->mFormat ) ) {
            auto result{ CompressImage( GenerateImageMipChain( DecompressImage( image ) ) ) };
            // Do not degrade a pre-compressed source's original pixels.
            if ( result->mFormat == image->mFormat ) {
                const auto *data{ static_cast<const core::ubyte *>( result->mBufferSpan->GetData() ) };
                eastl::vector<core::ubyte> pixels{ data, data + result->mBufferSpan->GetSize() };
                std::memcpy( pixels.data(), image->mBufferSpan->GetData(), image->mBufferSpan->GetSize() );
                result->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
            }
            return result;
        }

        auto result{ ImageHandle::New() };
        result->mFileHandle = image->mFileHandle;
        result->mWidth = image->mWidth;
        result->mHeight = image->mHeight;
        result->mChannels = image->mChannels;
        result->mFormat = image->mFormat;
        const bool floating{ image->mFormat == ImageFormat::eRGBA32_FLOAT };
        const core::usize pixelBytes{ 4 * ( floating ? sizeof( float ) : 1 ) };
        const auto *base{ static_cast<const core::ubyte *>( image->mBufferSpan->GetData() ) };
        eastl::vector<core::ubyte> pixels{ base, base + image->mBufferSpan->GetSize() };
        core::usize previousOffset{};
        core::u32 width{ image->mWidth }, height{ image->mHeight };
        while ( width > 1 || height > 1 ) {
            const core::u32 nextWidth{ eastl::max( 1u, width / 2 ) }, nextHeight{ eastl::max( 1u, height / 2 ) };
            const auto offset{ pixels.size() };
            pixels.resize( offset + core::usize( nextWidth ) * nextHeight * pixelBytes );
            FilterImageMipLevel( pixels.data() + previousOffset, width, height,
                pixels.data() + offset, nextWidth, nextHeight, floating );
            previousOffset = offset;
            width = nextWidth;
            height = nextHeight;
            ++result->mMipCount;
        }
        result->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
        return result;
    }

    auto PrepareImageForUpload( const ImageHandle& image, renderer::rhi::IGpuDevice& device ) -> ImageHandle {
        if ( !image ) {
            return image;
        }
        if ( image->mFormat == ImageFormat::eCompressed_BC6 || image->mFormat == ImageFormat::eCompressed_BC7 ) {
            const bool direct3D{ device.IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D12 ) };

            if ( !device.SupportsBCTextureCompression() || ( direct3D && ( image->mWidth % 4 || image->mHeight % 4 ) ) ) {
                return GenerateImageMipChain( DecompressImage( image ) );
            }
        }

        return GenerateImageMipChain( image );
    }

}// namespace mikoto::asset
