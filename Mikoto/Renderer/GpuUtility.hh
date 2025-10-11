//
// Created by zanet on 3/27/2025.
//

#ifndef GPURESOURCES_HH
#define GPURESOURCES_HH

#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class QueueType {
        TRANSFER_QUEUE,
        GRAPHICS_QUEUE,
        COMPUTE_QUEUE,
        PRESENT_QUEUE,
        INVALID_QUEUE
    };

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
    enum class ShaderStage : UInt32 {
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

    enum class TextureUsage {
        TEXTURE_USAGE_COLOR,
        TEXTURE_USAGE_DEPTH,
        TEXTURE_USAGE_NORMAL,
        TEXTURE_USAGE_STORAGE,       // compute shader writable
        TEXTURE_USAGE_CUBEMAP,       // for environment maps
        TEXTURE_USAGE_RENDER_TARGET, // render target attachments
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

    // --- 8-bit normalized formats ---
    TEXTURE_FORMAT_R8_UNORM,
    TEXTURE_FORMAT_RG8_UNORM,
    TEXTURE_FORMAT_RGB8_UNORM,
    TEXTURE_FORMAT_RGBA8_UNORM,

    // --- 8-bit signed normalized (rare, but sometimes used for normals) ---
    TEXTURE_FORMAT_R8_SNORM,
    TEXTURE_FORMAT_RG8_SNORM,
    TEXTURE_FORMAT_RGB8_SNORM,
    TEXTURE_FORMAT_RGBA8_SNORM,

    // --- 16-bit normalized ---
    TEXTURE_FORMAT_R16_UNORM,
    TEXTURE_FORMAT_RG16_UNORM,
    TEXTURE_FORMAT_RGB16_UNORM,
    TEXTURE_FORMAT_RGBA16_UNORM,

    // --- 16-bit float (HDR and data textures) ---
    TEXTURE_FORMAT_R16_FLOAT,
    TEXTURE_FORMAT_RG16_FLOAT,
    TEXTURE_FORMAT_RGB16_FLOAT,
    TEXTURE_FORMAT_RGBA16_FLOAT,

    // --- 32-bit float (HDR / G-buffer / compute data) ---
    TEXTURE_FORMAT_R32_FLOAT,
    TEXTURE_FORMAT_RG32_FLOAT,
    TEXTURE_FORMAT_RGB32_FLOAT,
    TEXTURE_FORMAT_RGBA32_FLOAT,

    // --- sRGB (gamma-correct color) ---
    TEXTURE_FORMAT_SRGB8,
    TEXTURE_FORMAT_SRGB8_ALPHA8,

    // --- Depth and stencil formats ---
    TEXTURE_FORMAT_D16_UNORM,
    TEXTURE_FORMAT_D24_UNORM_S8_UINT,
    TEXTURE_FORMAT_D32_FLOAT,
    TEXTURE_FORMAT_D32_FLOAT_S8_UINT,

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
        BUFFER_DATA_TYPE_UNKNOWN,
        BUFFER_DATA_UINT32,
        BUFFER_DATA_UINT16,
        BUFFER_DATA_FLOAT32,
    };

    template<typename T>
    MKT_NODISCARD auto InferSize( const Size elemCount) -> Size {
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
     * @deprecated Will specify format from load description
     */
    MKT_NODISCARD constexpr auto InferFormatFromChannels( const Int32 channelCount ) -> TextureFormat {
        switch ( channelCount ) {
            case 3:
                return TextureFormat::TEXTURE_FORMAT_RGB8_SNORM;
            case 4:
                return TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM;
            default:;
        }

        return TextureFormat::TEXTURE_FORMAT_RGB8_SNORM;
    }

    struct BufferDescription {
        Size SizeBytes{};
        Byte* Data{ nullptr };

        BufferUsage Usage{ BufferUsage::BUFFER_USAGE_VERTEX };
        BufferDataType Type{ BufferDataType::BUFFER_DATA_TYPE_UNKNOWN };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto WithSizeBytes(Size size) -> BufferDescription&;
        auto WithData(Byte* data) -> BufferDescription&;
        auto WithUsage(BufferUsage usage) -> BufferDescription&;
        auto WithBufferDataType(BufferDataType type) -> BufferDescription&;
        auto WithResourceUsageType(ResourceUsageType type) -> BufferDescription&;
    };

    struct TextureDescription {
        Int32 Width{};
        Int32 Height{};
        Int32 ChannelCount{ 4 };

        Byte* Data{ nullptr };

        TextureType Type{ TextureType::TEXTURE_2D };
        TextureUsage Usage{ TextureUsage::TEXTURE_USAGE_NORMAL };
        TextureFormat Format{ InferFormatFromChannels( this->ChannelCount ) };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto WithWidth( Int32 width ) -> TextureDescription&;
        auto WithHeight( Int32 height ) -> TextureDescription&;
        auto WithChannelCount( Int32 channels ) -> TextureDescription&;

        auto WithData( Byte* data ) -> TextureDescription&;
        auto WithType( TextureType type ) -> TextureDescription&;
        auto WithTextureUsage( TextureUsage usage ) -> TextureDescription&;

        auto WithFormat( TextureFormat format ) -> TextureDescription&;
        auto WithResourceType( ResourceUsageType type ) -> TextureDescription&;
    };


    /**
    * @struct TextureLoadDescription
    * @brief Holds information for loading a texture.
    *
    * The `TextureLoadInfo` structure stores metadata required to load a texture,
    * including the file path and texture type. It provides a fluent interface
    * for setting its properties.
    */
    struct TextureLoadDescription {
        const File *TextureFile{};
        TextureType Type{ TextureType::TEXTURE_INVALID };


        auto WithFile( const File *file ) -> TextureLoadDescription &;
        auto WithType( TextureType type ) -> TextureLoadDescription &;
    };

    struct ShaderModuleDescription {
        const File* ShaderFile{};
        std::string ShaderContents{  };
        ShaderStage Stage{ ShaderStage::VERTEX_STAGE };

        auto WithShaderFile( const File* file ) -> ShaderModuleDescription&;
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
