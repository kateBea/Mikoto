//
// Created by zanet on 3/27/2025.
//

#ifndef GPURESOURCES_HH
#define GPURESOURCES_HH

#include <Library/Filesystem/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class PipelineType {
        INVALID_TYPE,
        GRAPHICS_PIPELINE,
        COMPUTE_PIPELINE,
        RAY_TRACING_PIPELINE
      };

    enum class BufferUsage {
        BUFFER_USAGE_VERTEX,
        BUFFER_USAGE_INDEX,
        BUFFER_USAGE_STAGING,
        BUFFER_USAGE_UNIFORM,
        BUFFER_USAGE_SHADER_STORAGE,
    };

    enum class SamplerFilter {
        FILTER_NEAREST,
        FILTER_LINEAR,
    };

    enum class SamplerWrapMode {
        WRAP_REPEAT,
        WRAP_CLAMP_TO_EDGE,
        WRAP_CLAMP_TO_BORDER,
    };

    /**
     * @enum ShaderStage
     * @brief Enum representing the different stages of the shader pipeline.
     *
     * This enum defines the various stages a shader can belong to in the graphics pipeline.
     * Each stage corresponds to a specific step in the rendering process, such as vertex processing, fragment
     * (pixel) processing, or compute shaders.
     */
    enum class ShaderStage : UInt32_T {
        VERTEX_STAGE,
        FRAGMENT_STAGE,
        COMPUTE_STAGE,
    };

    /**
     * @enum TextureType
     * @brief Enum representing various texture types.
     *
     * This enum defines the different types of textures that can be used in a graphics pipeline.
     * Each texture type corresponds to a specific role or purpose in rendering, such as diffuse, specular,
     * normal, and other material properties.
     */
    enum class TextureType {
        TEXTURE_INVALID = -1,
        TEXTURE_2D,
        TEXTURE_CUBE,
    };

    /**
     * @enum TextureFormat
     * @brief Enum representing various texture formats.
     *
     * This enum defines the format of the texture, determining how the texture data is stored in memory.
     * It includes formats such as RGBA and RGB, which describe how color data is represented per pixel.
     */
    enum class TextureFormat {
        TEXTURE_FORMAT_INVALID = -1,
        TEXTURE_FORMAT_RGBA8,
        TEXTURE_FORMAT_RGB8,
    };

    enum class ResourceUsageType {
        // Resource never changes after creation
        RESOURCE_USAGE_STATIC,

        // Resource can be updated sometimes
        RESOURCE_USAGE_DYNAMIC,

        // Resource can update often
        RESOURCE_USAGE_STREAM,
    };

    enum class BufferDataType {
        BUFFER_DATA_TYPE_INVALID,
        BUFFER_DATA_UINT32,
        BUFFER_DATA_UINT16,
        BUFFER_DATA_FLOAT32,
    };

    template<typename T>
    MKT_NODISCARD auto InferSize( const Size_T elemCount) -> Size_T {
        return sizeof(T) * elemCount;
    }

    /**
     * @brief Infers the texture format based on the number of channels.
     *
     * This function takes the number of channels and returns the appropriate texture format.
     * It is designed to map common channel counts to standard texture formats.
     *
     * @param channelCount The number of channels (e.g., 3 for RGB, 4 for RGBA).
     * @return The inferred texture format (either RGB8 or RGBA8).
     */
    MKT_NODISCARD constexpr auto InferFormatFromChannels( const Int32_T channelCount ) -> TextureFormat {
        switch ( channelCount ) {
            case 3:
                return TextureFormat::TEXTURE_FORMAT_RGB8;
            case 4:
                return TextureFormat::TEXTURE_FORMAT_RGBA8;
            default:;
        }

        return TextureFormat::TEXTURE_FORMAT_RGBA8;
    }

    struct BufferDescription {
        Size_T SizeBytes{};
        Byte_T* Data{ nullptr };

        BufferUsage Usage{ BufferUsage::BUFFER_USAGE_VERTEX };
        BufferDataType Type{ BufferDataType::BUFFER_DATA_FLOAT32 };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto WithSizeBytes(Size_T size) -> BufferDescription&;
        auto WithData(Byte_T* data) -> BufferDescription&;
        auto WithUsage(BufferUsage usage) -> BufferDescription&;
        auto WithBufferDataType(BufferDataType type) -> BufferDescription&;
        auto WithResourceUsageType(ResourceUsageType type) -> BufferDescription&;
    };

    struct TextureDescription {
        Int32_T Width{};
        Int32_T Height{};
        Int32_T ChannelCount{ 4 };

        Byte_T* Data{ nullptr };

        TextureType Type{ TextureType::TEXTURE_2D };
        TextureFormat Format{ InferFormatFromChannels( this->ChannelCount ) };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto WithWidth( Int32_T width ) -> TextureDescription&;
        auto WithHeight( Int32_T height ) -> TextureDescription&;
        auto WithChannelCount( Int32_T channels ) -> TextureDescription&;

        auto WithData( Byte_T* data ) -> TextureDescription&;
        auto WithType( TextureType type ) -> TextureDescription&;

        auto WithFormat( TextureFormat format ) -> TextureDescription&;
        auto WithResourceType( ResourceUsageType type ) -> TextureDescription&;
    };

    struct ShaderModuleDescription {
        std::string Uri{};
        std::string ShaderContents{  };
        ShaderStage Stage{ ShaderStage::VERTEX_STAGE };

        auto WithShaderFile( const File* file ) -> ShaderModuleDescription&;
        auto WithUri( const std::string& stage ) -> ShaderModuleDescription&;
        auto WithStage( ShaderStage stage ) -> ShaderModuleDescription&;
    };

    struct SamplerDescription {
        SamplerFilter MinFilter{ SamplerFilter::FILTER_NEAREST };
        SamplerFilter MagFilter{ SamplerFilter::FILTER_NEAREST };
        SamplerWrapMode WrapS{ SamplerWrapMode::WRAP_REPEAT };
        SamplerWrapMode WrapT{ SamplerWrapMode::WRAP_REPEAT };

        auto WithMinFilter( SamplerFilter filter ) -> SamplerDescription&;
        auto WithMagFilter( SamplerFilter filter ) -> SamplerDescription&;
        auto WithWrapS( SamplerWrapMode wrap ) -> SamplerDescription&;
        auto WithWrapT( SamplerWrapMode wrap ) -> SamplerDescription&;
    };
}// namespace Mikoto
#endif//GPURESOURCES_HH
