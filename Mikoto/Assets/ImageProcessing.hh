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

#ifndef MIKOTO_ASSETS_IMAGE_PROCESSING_HH
#define MIKOTO_ASSETS_IMAGE_PROCESSING_HH

#include <EASTL/fixed_hash_map.h>
#include <EASTL/vector.h>

#include <Filesystem/File.hh>

#include <Assets/Image.hh>

#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::asset {

    /** One active encoder invocation. Counters measure finished BC6H/BC7 blocks. */
    struct TextureCompressionProgress {
        filesystem::Path mPath{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        ImageFormat mFormat{};
        core::usize mCompletedBlocks{};
        core::usize mTotalBlocks{};
    };

    /** Copies active compression work without waiting for the encoder workers. */
    MKT_NODISCARD auto GetTextureCompressionProgress() -> eastl::vector<TextureCompressionProgress>;


    // Pending test with Google Wuffs
    // Using STB_Image for the time being

    /**
     * @brief Reads a 2D BC6H/BC7 or uncompressed RGBA KTX2 mip chain without transcoding.
     * @param file Retained file supplying encoded or serialized data.
     * @returns Image metadata and retained compressed or RGBA pixels.
     */
    MKT_NODISCARD auto ProcessImageCompressed( filesystem::FileHandle file ) -> ImageHandle;


    /**
     * @brief Area-filters one tightly packed, four-channel image into a smaller mip level.
     * @details Weights source texels by their exact overlap with each destination texel,
     * retaining odd-sized edges. Channels, including alpha, are averaged independently
     * in their stored numeric space, without gamma conversion or alpha premultiplication.
     * Byte results are rounded and clamped to [0, 255]; float results remain unclamped.
     * @param source Borrowed source pixels in row-major RGBA8 or RGBA32_FLOAT storage.
     * @param width Source width in texels.
     * @param height Source height in texels.
     * @param destination Writable destination pixels, using the same storage as source.
     * @param nextWidth Destination mip width in texels.
     * @param nextHeight Destination mip height in texels.
     * @param floating True for four float channels per texel; false for four byte channels.
     * @pre All dimensions are positive. Source and destination ranges must not overlap
     * and must hold their full width times height times four channels, without row padding.
     */
    auto FilterImageMipLevel( const void *source, core::u32 width, core::u32 height,
        void *destination, core::u32 nextWidth, core::u32 nextHeight, bool floating ) -> void;


    /** Builds a complete, area-filtered chain in the image's numeric channel space.
     * Compressed images keep their original base level and encode the smaller levels.
     * Fonts and numeric lookup tables opt out by bypassing this asset upload helper.
     */
    MKT_NODISCARD auto GenerateImageMipChain( const ImageHandle &image ) -> ImageHandle;

    /** Builds missing mips and decompresses formats the target device cannot upload. */
    MKT_NODISCARD auto PrepareImageForUpload( const asset::ImageHandle& image, renderer::rhi::IGpuDevice& device ) -> asset::ImageHandle;


    /**
     * @brief Reads a 2D KTX2 image and its mip chain without transcoding stored pixels.
     * @param data Encoded KTX2 bytes borrowed for this call.
     * @param sizeBytes Readable byte count.
     * @returns Image metadata and retained compressed or RGBA pixels.
     */
    MKT_NODISCARD auto ProcessImageCompressed( const void* data, core::usize sizeBytes ) -> ImageHandle;


    /**
     * @brief Configures the shared CPU encoding pool before its first use.
     * @param count Zero selects hardware concurrency minus three, with a minimum of one.
     * Positive counts are capped at this budget, shared with mesh and texture import jobs.
     * @throws core::RuntimeException If an existing pool would need resizing.
     */
    auto SetTextureCompressionWorkerCount( core::u32 count ) -> void;


    /**
     * @brief Returns the resolved worker count without starting the pool.
     * @returns Resolved worker count without starting the pool.
     */
    MKT_NODISCARD auto GetTextureCompressionWorkerCount() -> core::u32;


    /**
     * @brief Encodes RGBA8 as BC7, or opaque nonnegative float HDR as BC6H.
     * @details Retains dimensions and pads only the partial edge blocks.
     * HDR outside the UF16 range and HDR alpha remain float RGBA. Mip chains are retained;
     * mip generation runs separately before compression.
     * Large images encode independent groups of blocks on a shared bounded worker
     * pool. Small images encode on the caller. The call waits for complete output;
     * concurrent calls must keep their source pixels immutable until it returns.
     * @param image Source image and its CPU-owned pixels.
     * @returns Compressed image, or retained float RGBA when BC6H cannot represent it.
     */
    MKT_NODISCARD auto CompressImage( const ImageHandle& image ) -> ImageHandle;


    /**
     * @brief Decodes compressed pixels for CPU operations such as thumbnail filtering.
     * @param image Source image and its CPU-owned pixels.
     * @returns CPU-readable RGBA image.
     */
    MKT_NODISCARD auto DecompressImage( const ImageHandle& image ) -> ImageHandle;


    /**
     * @brief Loads a versioned KTX2 cache, rebuilding it when source metadata changes.
     * @param compress Defaults to false so development imports avoid encoding unless explicitly enabled.
     * When false, decodes the original source and bypasses the texture cache.
     * Native KTX2 sources keep their stored format regardless of this import policy.
     * @param source Source image path used to validate the derived cache.
     * @param cacheDirectory Root of the derived texture cache.
     * @returns Source image or matching cached texture.
     */
    MKT_NODISCARD auto LoadTextureImage( const filesystem::Path& source, const filesystem::Path& cacheDirectory, bool compress = false ) -> ImageHandle;


    /**
     * @brief Maps CPU image storage to its RHI upload format.
     * @param format CPU image storage format.
     * @returns Corresponding texture format.
     */
    MKT_NODISCARD auto GetTextureFormat( ImageFormat format ) -> renderer::rhi::Format;


    /**
     * @brief  HDR retains floating-point samples. preserve16Bit promotes 16-bit LDR data
     * to linear RGBA32F without gamma conversion, for numerical textures such as heightmaps.
     * @param file Retained file supplying encoded or serialized data.
     * @param preserve16Bit Whether 16-bit LDR inputs should retain their precision in float RGBA.
     * @returns Decoded image with the requested output precision.
     */
    auto ProcessImage2D( filesystem::FileHandle file, bool preserve16Bit = false ) -> ImageHandle;


    /**
     * @brief Decodes an encoded 2D image into CPU-owned pixels.
     * @param filepath Source image path.
     * @param preserve16Bit Whether 16-bit LDR inputs should retain their precision in float RGBA.
     * @returns Decoded image with the requested output precision.
     */
    auto ProcessImage2D( const filesystem::Path& filepath, bool preserve16Bit = false ) -> ImageHandle;


    // Compressed image in memory
    /**
     * @brief Decodes an encoded 2D image into CPU-owned pixels.
     * @param data Encoded image bytes borrowed for this call.
     * @param sizeBytes Readable byte count.
     * @param format RGBA8 or RGBA32F decoder output; KTX2 retains its stored format.
     * @returns Decoded image with the requested output precision.
     */
    auto ProcessImage2D( const void* data, core::usize sizeBytes, ImageFormat format ) -> ImageHandle;


    // Attempts to construct the cube image from the equirectangular file
    /**
     * @brief Declares cube import from an equirectangular image or six face files.
     * @param file Retained file supplying encoded or serialized data.
     * @returns Cube resource; this entry point is not implemented yet.
     */
    auto ProcessImageCube( filesystem::FileHandle file ) -> ImageHandle;


    /**
     * @brief Declares cube import from an equirectangular image or six face files.
     * @param files Retained source files indexed by cube face.
     * @returns Cube resource; this entry point is not implemented yet.
     */
    auto ProcessImageCube( const eastl::fixed_hash_map<ImageCubeFace, filesystem::FileHandle, kCubeFaceCount>& files ) -> ImageHandle;


    /**
     * @brief  Writes RGBA8 as PNG or RGBA32F as HDR. Throws if encoding or writing fails.
     * @param path Source file path.
     * @param data Tightly packed four-channel pixels in the selected format.
     * @param width Width in pixels.
     * @param height Height in pixels.
     * @param format RGBA8 selects PNG and RGBA32F selects HDR; other formats are ignored.
     */
    auto WriteImage( const filesystem::Path& path, const void* data, core::u32 width, core::u32 height, ImageFormat format ) -> void;


    /**
     * @brief Writes image metadata and pixels into a KTX2 file.
     * @param path Source file path.
     * @param image Source image and its CPU-owned pixels.
     */
    auto WriteImageCompressed( const filesystem::Path& path, const ImageHandle& image ) -> void;


    // TODO(kate): HDR image processor
    // Will take an HDR equirectangular image from file path and project it into a cube
    // You can use it to then generate your cube map
    // Initial implementation from: https://github.com/ivarout/HdriToCubemap

}// namespace mikoto::asset

#endif// MIKOTO_ASSETS_IMAGE_PROCESSING_HH
