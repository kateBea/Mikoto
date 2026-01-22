//
// Created by zanet on 4/12/2025.
//

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

    auto LoadHDRImageFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc* {
        stbi_set_flip_vertically_on_load( true );

        constexpr int targetChannelCount{ STBI_rgb_alpha };
        stbi_uc* data{ reinterpret_cast<stbi_uc*>( stbi_loadf(
                textureFile->GetPathCStr(),
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
        m_Data = LoadHDRImageFromFile( textureFile, m_Width, m_Height, m_Channels );
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

    StbImage::StbImage( const File* textureFile ) {
        m_Data = LoadImageFromFile( textureFile, m_Width, m_Height, m_Channels );
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
    auto InferElementCount( const BufferDataType dataType, const Size blockSize ) -> Size {
        switch (dataType) {

            case BufferDataType::BUFFER_DATA_TYPE_UNKNOWN:
                break;
            case BufferDataType::BUFFER_DATA_UINT32:
                return blockSize / sizeof( UInt32 );
            case BufferDataType::BUFFER_DATA_UINT16:
                return blockSize / sizeof( UInt16 );
            case BufferDataType::BUFFER_DATA_FLOAT32:
                return blockSize / sizeof( float );
        }

        return 0;
    }

    auto InferDimensions( RenderResolution resolution ) -> std::pair<float, float> {
        switch (resolution ) {
            case RenderResolution::RES_HD_720P: return std::make_pair( 1280.0f, 720.0f );
            case RenderResolution::RES_FHD_1080: return std::make_pair( 1920.0f, 1080.0f );
            case RenderResolution::RES_QHD_1440P: return std::make_pair( 2560.0f, 1440.0f );
            case RenderResolution::RES_UHD_3120P: return std::make_pair( 3840.0f, 2160.0f );
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

    auto TextureDescription::WithWidth( Int32 width ) -> TextureDescription& {
        Width = width;
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

    auto SamplerDescription::WithWrapS( SamplerWrapMode wrap ) -> SamplerDescription& {
        WrapS = wrap;
        return *this;
    }

    auto SamplerDescription::WithWrapT( SamplerWrapMode wrap ) -> SamplerDescription& {
        WrapT = wrap;
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