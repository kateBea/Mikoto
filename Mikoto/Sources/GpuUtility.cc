//
// Created by kate on 10/5/2025.
//

#include <Assets/Texture.hh>
#include <Renderer/Framebuffer.hh>
#include <Renderer/GpuUtility.hh>

namespace Mikoto {

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

    auto TextureLoadDescription::WithFile( const File* file ) -> TextureLoadDescription& {
        this->TextureFile = file;
        return *this;
    }

    auto TextureLoadDescription::WithType( TextureType type ) -> TextureLoadDescription& {
        this->Type = type;
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