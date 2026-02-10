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

#if !defined(STB_IMAGE_IMPLEMENTATION)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION
#endif

#include <Assets/Texture.hh>
#include <Common/Common.hh>
#include <Core/Exception.hh>
#include <Library/Utility/Types.hh>

#include <Renderer/Core/Framebuffer.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {
    auto LoadImageFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc* {
        stbi_set_flip_vertically_on_load( true );

        constexpr int targetChannelCount{ STBI_rgb_alpha };
        stbi_uc* data{ stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>( textureFile->GetFileBytes() ),
                textureFile->GetFileContents().size(),
                std::addressof( outWidth ),
                std::addressof( outHeight ),
                std::addressof( outChannels ),
                targetChannelCount ) };

        if ( !data ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "LoadImageFromFile - Failed to load texture image: [{}]", textureFile->GetPathCStr() ) );
        }

        outChannels = 4;
        return data;
    }

    auto LoadImageFromMemory( const Byte* buffer, Size sizeBytes, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc* {
        stbi_set_flip_vertically_on_load( true );

        constexpr int targetChannelCount{ STBI_rgb_alpha };
        stbi_uc* data{ stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>( buffer ),
                sizeBytes,
                std::addressof( outWidth ),
                std::addressof( outHeight ),
                std::addressof( outChannels ),
                targetChannelCount ) };

        if ( !data ) {
            MKT_THROW_RUNTIME_ERROR( "LoadImageFromFile - Failed to load texture memory" );
        }

        outChannels = 4;
        return data;
    }

    auto LoadImageFloatFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc* {
        stbi_set_flip_vertically_on_load( true );

        constexpr int targetChannelCount{ STBI_rgb_alpha };
        stbi_uc* data{ reinterpret_cast<stbi_uc*>( stbi_loadf_from_memory(
                reinterpret_cast<const stbi_uc*>( textureFile->GetFileBytes() ),
                textureFile->GetFileContents().size(),
                std::addressof( outWidth ),
                std::addressof( outHeight ),
                std::addressof( outChannels ),
                targetChannelCount ) ) };

        if ( !data ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "LoadHDRImageFromFile - Failed to load HDR: [{}]", textureFile->GetPathCStr() ) );
        }

        outChannels = 4;
        return data;
    }

    MKT_NODISCARD auto FreeImageData( Byte* data ) -> void {
        stbi_image_free( data );
    }

    auto InferAPI( const std::string_view apiName ) -> GraphicsAPI {
        if (StringUtils::Equal( apiName, "Vulkan", StringUtils::StringComparisonPolicy::CASE_INSENSITIVE )) {
            return GraphicsAPI::VULKAN_API;
        }

        return GraphicsAPI::UNKNOWN;
    }

    STBImageHDR::STBImageHDR( const File* textureFile ) {
        m_Data = LoadImageFloatFromFile( textureFile, m_Width, m_Height, m_Channels );
    }

    STBImageHDR::~STBImageHDR() {
        if ( m_Data ) {
            stbi_image_free( m_Data );
            m_Data = nullptr;
        }
    }

    STBImageHDR::STBImageHDR( STBImageHDR&& other ) noexcept
        : m_Width( other.m_Width ),
          m_Height( other.m_Height ),
          m_Channels( other.m_Channels ),
          m_Data( other.m_Data ) {
            other.m_Data = nullptr;
            other.m_Width = 0;
            other.m_Height = 0;
            other.m_Channels = 0;
    }

    auto STBImageHDR::operator=( STBImageHDR&& other ) noexcept -> STBImageHDR& {
        if ( this != &other ) {
            if ( m_Data ) {
                stbi_image_free( m_Data );
            }

            m_Data = other.m_Data;
            m_Width = other.m_Width;
            m_Height = other.m_Height;
            m_Channels = other.m_Channels;

            other.m_Data = nullptr;
            other.m_Width = 0;
            other.m_Height = 0;
            other.m_Channels = 0;
        }

        return *this;
    }

    StbImage::StbImage( const File* textureFile, bool isHDR ) {
        // FIXME: it looks buggy when transformed into a cubeMap
        // if (isHDR) {
        //     m_Data = LoadImageFloatFromFile( textureFile, m_Width, m_Height, m_Channels );
        // } else {
        //     m_Data = LoadImageFromFile( textureFile, m_Width, m_Height, m_Channels );
        // }
        m_Data = LoadImageFromFile( textureFile, m_Width, m_Height, m_Channels );
    }

    StbImage::StbImage( const Byte* data, Size sizeBytes) {
        m_Data = LoadImageFromMemory( data, sizeBytes, m_Width, m_Height, m_Channels );
    }

    StbImage::~StbImage() {
        if ( m_Data ) {
            stbi_image_free( m_Data );
            m_Data = nullptr;
        }
    }

    StbImage::StbImage( StbImage&& other ) noexcept
        : m_Width( other.m_Width ),
          m_Height( other.m_Height ),
          m_Channels( other.m_Channels ),
          m_Data( other.m_Data ) {
        other.m_Data = nullptr;
        other.m_Width = 0;
        other.m_Height = 0;
        other.m_Channels = 0;
    }

    auto StbImage::operator=( StbImage&& other ) noexcept -> StbImage& {
        if ( this != &other ) {
            if ( m_Data ) {
                stbi_image_free( m_Data );
            }

            m_Data = other.m_Data;
            m_Width = other.m_Width;
            m_Height = other.m_Height;
            m_Channels = other.m_Channels;

            other.m_Data = nullptr;
            other.m_Width = 0;
            other.m_Height = 0;
            other.m_Channels = 0;
        }

        return *this;
    }

    auto FontLoadDescription::WithFile( const File* file ) -> FontLoadDescription& {
        this->FontFile = file;
        return *this;
    }

    auto FontLoadDescription::WithPixelSize( float pixelSize ) -> FontLoadDescription& {
        this->FontSize = pixelSize;
        return *this;
    }

    auto GetChanelString( UInt32 channelCount ) -> std::string_view {
        switch (channelCount) {
            case 1: return "Format_R";
            case 2: return "Format_RG";
            case 3: return "Format_RGB";
            case 4: return "Format_RGB_Aplha";
        }

        return "N / A";
    }

    auto GetTextureFormatString( TextureFormat format ) -> std::string_view {
        switch ( format ) {
            case TextureFormat::INVALID:
                return "INVALID";
            case TextureFormat::R8_UNORM:
                return "R8_UNORM";
            case TextureFormat::RG8_UNORM:
                return "RG8_UNORM";
            case TextureFormat::RGB8_UNORM:
                return "RGB8_UNORM";
            case TextureFormat::RGBA8_UNORM:
                return "RGBA8_UNORM";

            case TextureFormat::R8_SNORM:
                return "R8_SNORM";
            case TextureFormat::RG8_SNORM:
                return "RG8_SNORM";
            case TextureFormat::RGB8_SNORM:
                return "RGB8_SNORM";
            case TextureFormat::RGBA8_SNORM:
                return "RGBA8_SNORM";

            case TextureFormat::R16_UNORM:
                return "R16_UNORM";
            case TextureFormat::RG16_UNORM:
                return "RG16_UNORM";
            case TextureFormat::RGB16_UNORM:
                return "RGB16_UNORM";
            case TextureFormat::RGBA16_UNORM:
                return "RGBA16_UNORM";

            case TextureFormat::R16_FLOAT:
                return "R16_FLOAT";
            case TextureFormat::RG16_FLOAT:
                return "RG16_FLOAT";
            case TextureFormat::RGB16_FLOAT:
                return "RGB16_FLOAT";
            case TextureFormat::RGBA16_FLOAT:
                return "RGBA16_FLOAT";

            case TextureFormat::R32_FLOAT:
                return "R32_FLOAT";
            case TextureFormat::RG32_FLOAT:
                return "RG32_FLOAT";
            case TextureFormat::RGB32_FLOAT:
                return "RGB32_FLOAT";
            case TextureFormat::RGBA32_FLOAT:
                return "RGBA32_FLOAT";

            case TextureFormat::SRGB8:
                return "SRGB8";
            case TextureFormat::SRGB8_ALPHA8:
                return "SRGB8_ALPHA8";

            case TextureFormat::D16_UNORM:
                return "D16_UNORM";
            case TextureFormat::D24_UNORM_S8_UINT:
                return "D24_UNORM_S8_UINT";
            case TextureFormat::D32_FLOAT:
                return "D32_FLOAT";
            case TextureFormat::D32_FLOAT_S8_UINT:
                return "D32_FLOAT_S8_UINT";
        }

        return "UNKNOWN_FORMAT";
    }

    auto InferElementCount( const BufferDataType dataType, const Size blockSize ) -> Size {
        switch (dataType) {
            case BufferDataType::BUFFER_DATA_UINT32:
                return blockSize / sizeof( UInt32 );
            case BufferDataType::BUFFER_DATA_UINT16:
                return blockSize / sizeof( UInt16 );
            case BufferDataType::BUFFER_DATA_FLOAT32:
                return blockSize / sizeof( float );
            default:;
        }

        return 0;
    }

    auto InferDimensions( RenderResolution resolution ) -> std::pair<float, float> {
        switch (resolution ) {
            case RenderResolution::HD_720P: return std::make_pair( 1280.0f, 720.0f );
            case RenderResolution::FHD_1080: return std::make_pair( 1920.0f, 1080.0f );
            case RenderResolution::QHD_1440P: return std::make_pair( 2560.0f, 1440.0f );
            case RenderResolution::UHD_3120P: return std::make_pair( 3840.0f, 2160.0f );
        }

        return std::make_pair( 1920.0f, 1080.0f );
    }

    auto BufferDescription::ForElement( const Size size, const Size count ) -> BufferDescription& {
        this->ElementSize = size;
        this->ElementCount = count;
        return *this;
    }

    auto BufferDescription::WithSizeBytes( Size size ) -> BufferDescription& {
        SizeBytes = size;
        return *this;
    }

    auto BufferDescription::WithData( Byte* data ) -> BufferDescription& {
        Data = data;
        return *this;
    }

    auto BufferDescription::WithUsage( BufferUsage usage ) -> BufferDescription& {
        Usage = usage;
        return *this;
    }
    auto BufferDescription::WithBufferDataType( BufferDataType type ) -> BufferDescription& {
        Type = type;
        return *this;
    }

    auto BufferDescription::WithResourceUsageType( ResourceUsageType type ) -> BufferDescription& {
        UsageType = type;
        return *this;
    }

    auto TextureDescription::IsHDRMap( bool value ) -> TextureDescription& {
        IsHDR = value;
        return *this;
    }

    auto TextureDescription::WithWidth( Int32 width ) -> TextureDescription& {
        Width = width;
        return *this;
    }

    auto TextureDescription::WithName( std::string_view name ) -> TextureDescription& {
        Name = name;
        return *this;
    }

    auto TextureDescription::WithHeight( Int32 height ) -> TextureDescription& {
        Height = height;
        return *this;
    }

    auto TextureDescription::WithChannelCount( Int32 channels ) -> TextureDescription& {
        ChannelCount = channels;
        return *this;
    }

    auto TextureDescription::WithMapType( MapType type ) -> TextureDescription & {
        this->Map = type;
        return *this;
    }

    auto TextureDescription::WithFile( const File* file ) -> TextureDescription& {
        this->TextureFile = file;
        return *this;
    }

    auto TextureDescription::WithData( Byte* data ) -> TextureDescription& {
        Data = data;
        return *this;
    }

    auto TextureDescription::WithSize( Size size ) -> TextureDescription& {
        this->BufferSize = size;
        return *this;
    }

    auto TextureDescription::WithType( TextureType type ) -> TextureDescription& {
        Type = type;
        return *this;
    }

    auto TextureDescription::WithTextureUsage( TextureUsage usage ) -> TextureDescription& {
        this->Usage = usage;
        return *this;
    }

    auto TextureDescription::WithFormat( TextureFormat format ) -> TextureDescription& {
        Format = format;
        return *this;
    }

    auto TextureDescription::WithResourceType( ResourceUsageType type ) -> TextureDescription& {
        UsageType = type;
        return *this;
    }

    auto TextureLoadDescription::IsHDRMap( bool value ) -> TextureLoadDescription& {
        IsHDR = value;
        return *this;
    }

    auto TextureLoadDescription::WithMapType( MapType type ) -> TextureLoadDescription & {
        Map = type;
        return *this;
    }

    auto TextureLoadDescription::WithFile( const File* file ) -> TextureLoadDescription& {
        this->TextureFile = file;
        return *this;
    }

    auto TextureLoadDescription::WithType( TextureType type ) -> TextureLoadDescription& {
        this->Type = type;
        return *this;
    }

    auto TextureCubeLoadDescription::IsHDR( const bool value ) -> TextureCubeLoadDescription& {
        this->IsHdrMap = value;
        return *this;
    }

    auto TextureCubeLoadDescription::WithResourceUsage( ResourceUsageType usage ) -> TextureCubeLoadDescription & {
        this->ResourceUsage = usage;
        return *this;
    }

    auto TextureCubeLoadDescription::WithTextureUsage( TextureUsage usage ) -> TextureCubeLoadDescription & {
        this->Usage = usage;
        return *this;
    }

    auto TextureCubeLoadDescription::WithFacePath( const Path &file ) -> TextureCubeLoadDescription & {
        this->FacesRelativePaths.emplace_back( file );
        return *this;
    }

    auto TextureCubeLoadDescription::WithBasePath( const Path &file ) -> TextureCubeLoadDescription & {
        this->BasePath = file;
        return *this;
    }

    auto TextureCubeLoadDescription::WithType( TextureType type ) -> TextureCubeLoadDescription & {
        this->Type = type;
        return *this;
    }

    auto TextureCubeCreateDescription::IsHDR( bool value ) -> TextureCubeCreateDescription& {
        this->IsHdrMap = value;
        return *this;
    }

    auto TextureCubeCreateDescription::WithMipLevels( UInt32 levels ) -> TextureCubeCreateDescription & {
        this->MipLevels = levels;
        return *this;
    }

    auto TextureCubeCreateDescription::WithTextureFormat( TextureFormat format ) -> TextureCubeCreateDescription & {
        this->Format = format;
        return *this;
    }

    auto TextureCubeCreateDescription::WithDimensions( UInt32 dimensions ) -> TextureCubeCreateDescription & {
        this->Dimensions = dimensions;
        return *this;
    }

    auto TextureCubeCreateDescription::WithUsageType( TextureUsage usage ) -> TextureCubeCreateDescription & {
        this->Usage = usage;
        return *this;
    }

    auto TextureCubeCreateDescription::WithResourceUsage( ResourceUsageType usage ) -> TextureCubeCreateDescription & {
        this->ResourceUsage = usage;
        return *this;
    }

    auto TextureCubeCreateDescription::WithFacePath( const File *file ) -> TextureCubeCreateDescription & {
        this->Faces.emplace_back( file );
        return *this;
    }

    auto ShaderModuleDescription::WithShaderFile( const File* file ) -> ShaderModuleDescription& {
        ShaderFile = file;
        return *this;
    }

    auto ShaderModuleDescription::WithStage( ShaderStage stage ) -> ShaderModuleDescription& {
        Stage = stage;
        return *this;
    }

    auto SamplerDescription::WithMipLevels( float mipLevels ) -> SamplerDescription& {
        this->MipLevels = mipLevels;
        return *this;
    }

    auto SamplerDescription::WithMinFilter( SamplerFilter filter ) -> SamplerDescription& {
        MinFilter = filter;
        return *this;
    }

    auto SamplerDescription::WithMagFilter( SamplerFilter filter ) -> SamplerDescription& {
        MagFilter = filter;
        return *this;
    }

    auto SamplerDescription::WithWrapU( SamplerWrapMode wrap ) -> SamplerDescription& {
        WrapU = wrap;
        return *this;
    }

    auto SamplerDescription::WithWrapV( SamplerWrapMode wrap ) -> SamplerDescription& {
        WrapV = wrap;
        return *this;
    }

    auto SamplerDescription::WithWrapW( SamplerWrapMode wrap ) -> SamplerDescription& {
        WrapW = wrap;
        return *this;
    }

    auto FramebufferDescription::WithWidth( Int32 width ) -> FramebufferDescription& {
        Width = width;
        return *this;
    }

    auto FramebufferDescription::WithHeight( Int32 height ) -> FramebufferDescription& {
        Height = height;
        return *this;
    }

    auto FramebufferDescription::AddAttachment( TextureHandle color ) -> FramebufferDescription& {
        ColorAttachments.push_back( std::move( color ) );
        return *this;
    }

    auto FramebufferDescription::AddDepthAttachment( TextureHandle depth ) -> FramebufferDescription& {
        DepthAttachment.push_back( std::move( depth ) );
        return *this;
    }

    auto FramebufferDescription::WithSpecInfo( std::any nativeSpec ) -> FramebufferDescription& {
        NativeHandleSpec = std::move( nativeSpec );
        return *this;
    }
}// namespace Mikoto