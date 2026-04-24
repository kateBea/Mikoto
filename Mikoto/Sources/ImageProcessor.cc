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

// Needed with FreeImage on Windows otherwise nonsense errors happen
// warning C4828: The file contains a character starting at offset 0xa9 that is illegal in the current source character set (codepage 65001).
#include <Platform/PlatformWin32.hh>
#include <FreeImage.h>

#include <EASTL/vector.h>

#include <stb_image.h>

#include <portable-file-dialogs.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>
#include <Filesystem/FileService.hh>

#include <Assets/ImageProcessor.hh>

#include <Renderer/Core/RenderSystem.hh>

// Refs: https://github.com/dfranx/DDS
// Refs: https://github.com/danoli3/FreeImage.git

namespace mikoto::asset {

#if MKT_ENABLE_FREE_IMAGE
    static auto FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) -> void{
        MKT_CORE_LOGGER_ERROR("Error FreeImage: {}", message);

        if(fif != FIF_UNKNOWN) {
            MKT_CORE_LOGGER_ERROR("File format: {}", FreeImage_GetFormatFromFIF(fif));
        }
    }

    MKT_NODISCARD constexpr auto GetFileType(FREE_IMAGE_FORMAT format) -> FileType {
        switch (format) {
            case FIF_BMP:   return FileType::eBmp;
            case FIF_ICO:   return FileType::eIco;
            case FIF_JPEG:  return FileType::eJpeg;
            case FIF_JNG:   return FileType::eJng;
            case FIF_KOALA: return FileType::eKoala;
            case FIF_LBM:   return FileType::eIff;
            case FIF_MNG:   return FileType::eMng;
            case FIF_PBM:   return FileType::ePbm;
            case FIF_PBMRAW:return FileType::ePbmRaw;
            case FIF_PCD:   return FileType::ePcd;
            case FIF_PCX:   return FileType::ePcx;
            case FIF_PGM:   return FileType::ePgm;
            case FIF_PGMRAW:return FileType::ePgmRaw;
            case FIF_PNG:   return FileType::ePng;
            case FIF_PPM:   return FileType::ePpm;
            case FIF_PPMRAW:return FileType::ePpmRaw;
            case FIF_RAS:   return FileType::eRas;
            case FIF_TARGA: return FileType::eTarga;
            case FIF_TIFF:  return FileType::eTiff;
            case FIF_WBMP:  return FileType::eWbmp;
            case FIF_PSD:   return FileType::ePsd;
            case FIF_CUT:   return FileType::eCut;
            case FIF_XBM:   return FileType::eXbm;
            case FIF_XPM:   return FileType::eXpm;
            case FIF_DDS:   return FileType::eDds;
            case FIF_GIF:   return FileType::eGif;
            case FIF_HDR:   return FileType::eHdr;
            case FIF_FAXG3: return FileType::eFaxG3;
            case FIF_SGI:   return FileType::eSgi;
            case FIF_EXR:   return FileType::eExr;
            case FIF_J2K:   return FileType::eJ2k;
            case FIF_JP2:   return FileType::eJp2;
            case FIF_PFM:   return FileType::ePfm;
            case FIF_PICT:  return FileType::ePict;
            case FIF_RAW:   return FileType::eRaw;
            case FIF_WEBP:  return FileType::eWebp;
            case FIF_JXR:   return FileType::eJxr;

            default:
                return FileType::eInvalid;
        }
    }

    struct FreeImageManager {
        FreeImageManager() {
            FreeImage_SetOutputMessage(FreeImageErrorHandler);

            // call this ONLY when linking with FreeImage as a static library
#ifdef FREEIMAGE_LIB
            FreeImage_Initialise();
#endif
            MKT_CORE_LOGGER_DEBUG( "Initialized FreeImage" );
        }

        ~FreeImageManager() {
            // call this ONLY when linking with FreeImage as a static library
#ifdef FREEIMAGE_LIB
            FreeImage_DeInitialise();
#endif
            MKT_CORE_LOGGER_DEBUG( "DeInitialized FreeImage" );
        }
    };

    static FreeImageManager sFreeImageManager{};
#endif

    auto ProcessImage2D( const Path &filepath ) -> ImageHandle {
        FileHandle fileHandle{ FileService::Get()->LoadFile( filepath ) };
        return ProcessImage2D( fileHandle );
    }

    auto ProcessImage2D( FileHandle file ) -> ImageHandle {
        ImageHandle result{ ImageHandle::CreateEmpty() };

        if (file.IsEmpty()) {
            return result;
        }

        // Create the instance
        result = ImageHandle::Spawn();

        // channels will be ignored and will only contain the
        // actual image channel count
        i32 width{}, height{}, channels{};

        // Because we work with RGBA formats by default if not HDR values
        // to simplify the API we do not expose arbitrary image formats
        result->mChannels = 4;

        if (file->GetType() == FileType::eHdr) {
            constexpr int targetChannelCount{ STBI_rgb_alpha };
            float* data{ stbi_loadf_from_memory(
                    r_cast<const stbi_uc*>( file->GetContentsBytes() ),
                    as<int>( file->GetContentsString().size() ),
                    MKT_ADDRESSOF( width ),
                    MKT_ADDRESSOF( height ),
                    MKT_ADDRESSOF( channels ),
                    targetChannelCount ) };

            result->mFormat = ImageFormat::eRGBA_32F;
            result->mBufferSpan = BufferSpanHandle::Spawn( rc_cast<byte_t*>( data ), size_t{ as<size_t>( width * height * result->mChannels ) } );

            stbi_image_free( data );
        } else {
            constexpr int targetChannelCount{ STBI_rgb_alpha };
            stbi_uc* data{ stbi_load_from_memory(
                    r_cast<const stbi_uc*>( file->GetContentsBytes() ),
                    as<int>( file->GetContentsString().size() ),
                    MKT_ADDRESSOF( width ),
                    MKT_ADDRESSOF( height ),
                    MKT_ADDRESSOF( channels ),
                    targetChannelCount ) };

            result->mFormat = ImageFormat::eRGBA_8;
            result->mBufferSpan = BufferSpanHandle::Spawn( as<byte_t*>( data ), size_t{ as<size_t>( width * height * result->mChannels ) } );

            stbi_image_free( data );
        }

        result->mFileHandle = file;
        result->mWidth = width;
        result->mHeight = height;

#if MKT_ENABLE_FREE_IMAGE // Unavailable for now. Image manipulation with FreeImage will be used for other common formats stb does not support like TIFF
        //image format
        FREE_IMAGE_FORMAT fif{ FIF_UNKNOWN };
        //pointer to the image, once loaded
        FIBITMAP *originalBitmap{ nullptr };
        //pointer to the image data
        BYTE* bits{ nullptr };

        //check the file signature and deduce its format
        fif = FreeImage_GetFileType(file->GetPath().GetC_Str(), 0);

        //if still unknown, try to guess the file format from the file extension
        if(fif == FIF_UNKNOWN) {
            fif = FreeImage_GetFIFFromFilename(file->GetPath().GetC_Str());
        }

        //if still unknown, return failure
        if(fif == FIF_UNKNOWN) {
            return result;
        }

        //check that the plugin has reading capabilities and load the file
        if(FreeImage_FIFSupportsReading(fif)) {
            originalBitmap = FreeImage_Load(fif, file->GetPath().GetC_Str());
        }

        // FIXME: Attempt load from memory
        if (!originalBitmap) {
            FIMEMORY data{ .data = file->GetContentsBytes() };
            originalBitmap = FreeImage_LoadFromMemory( fif, MKT_ADDRESSOF( data ) );
        }

        //if the image failed to load, return failure
        if(!originalBitmap) {
            return result;
        }

        if (RenderSystem::Get()->IsApiActive( GraphicsAPI::eD3D11 )) {
            FreeImage_FlipVertical( originalBitmap );
        }

        // retrieve the image data
        bits = FreeImage_GetBits(originalBitmap);

        //get the image width and height
        const u32 pitch{ FreeImage_GetPitch(originalBitmap) };
        const u32 originalBPP{ FreeImage_GetBPP(originalBitmap) };
        const u32 widthInPixels{ FreeImage_GetWidth(originalBitmap) };
        const u32 heightInPixels{ FreeImage_GetHeight(originalBitmap) };
        const size_type memorySize{ FreeImage_GetMemorySize(originalBitmap) };
        const FREE_IMAGE_TYPE imageType{ FreeImage_GetImageType(originalBitmap) };

        //if this somehow one of these failed (they shouldn't), return failure
        if((bits == nullptr) || (widthInPixels == 0) || (heightInPixels == 0)) {
            FreeImage_Unload(originalBitmap);
            return result;
        }

        // Create the instance
        result = ImageHandle::Spawn();

        const u32 targetBPP{ 32 };

        result->mFileHandle = file;
        result->mWidth = widthInPixels;
        result->mHeight = heightInPixels;
        result->mChannels = targetBPP / 8; // Because we use RGBA
        result->mFormat = ImageFormat::eRGBA_8;

        if (imageType == FREE_IMAGE_TYPE::FIT_BITMAP && originalBPP != targetBPP) {
            // Because we work with RGBA formats by default if not HDR values
            // to simplify the API we do not expose arbitrary image formats
            FIBITMAP* newImage{ FreeImage_ConvertToRGBA16(originalBitmap) };
            FIBITMAP* newImageRGB{ FreeImage_ConvertTo32Bits(newImage) };

            bits = FreeImage_GetBits(newImageRGB);

            result->mBufferSpan = BufferSpanHandle::Spawn( as<byte_type*>( bits ), size_type{ widthInPixels * heightInPixels * (targetBPP / 8) } );
            FreeImage_Unload(newImage);
            FreeImage_Unload(newImageRGB);
        } else if (imageType == FREE_IMAGE_TYPE::FIT_RGBF) {
            FIBITMAP* newImage{ FreeImage_ConvertToRGBAF(originalBitmap) };

            bits = FreeImage_GetBits(newImage);

            result->mBufferSpan = BufferSpanHandle::Spawn( as<byte_type*>( bits ), size_type{ widthInPixels * heightInPixels * (originalBPP / 8) } );
            FreeImage_Unload(newImage);
        }else {
            result->mBufferSpan = BufferSpanHandle::Spawn( as<byte_type*>( bits ), size_type{ widthInPixels * heightInPixels * (targetBPP / 8) } );
        }

        // TODO: compare free image and stb_image

        //Free FreeImage's copy of the data
        FreeImage_Unload(originalBitmap);
#endif

        return result;
    }
}// namespace Mikoto