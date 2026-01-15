//
// Created by zanet on 4/7/2025.
//

#ifndef RENDERUTILITY_HH
#define RENDERUTILITY_HH

#include <stb_image.h>

#include <Common/Common.hh>
#include <Library/IO/File.hh>

namespace Mikoto {

    // If the texture2D represents a type of Map for pbr materials
    enum class MapType {
        ALBEDO_TEXTURE,
        NORMAL_TEXTURE,
        METALLIC_TEXTURE,
        ROUGHNESS_TEXTURE,
        AMBIENT_OCCLUSION_TEXTURE,
        EMISSIVE_TEXTURE,
        UNDEFINED_TEXTURE,
    };

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

    enum class Blending {
        MODE_OPAQUE,    // Fully opaque (no transparency)
        MODE_MASKED,    // Cutout transparency (alpha test)
        MODE_ADDITIVE,  // Additive blending (glowing effects)
        MODE_MULTIPLY,  // Multiplicative blending (darkening effects)
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
        STAGE_UNKNOWN,
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
        TEXTURE_UNKNOWN = -1,
        TEXTURE_2D,
        TEXTURE_CUBE,
    };

    enum class TextureUsage {
        TEXTURE_USAGE_COLOR,
        TEXTURE_USAGE_DEPTH,
        TEXTURE_USAGE_NORMAL,
        TEXTURE_USAGE_STORAGE,      // compute shader writable
        TEXTURE_USAGE_CUBE,         // for environment maps
        TEXTURE_USAGE_RENDER_TARGET,// render target attachments
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

        TEXTURE_FORMAT_R8_UNORM,
        TEXTURE_FORMAT_RG8_UNORM,
        TEXTURE_FORMAT_RGB8_UNORM,
        TEXTURE_FORMAT_RGBA8_UNORM,

        TEXTURE_FORMAT_R8_SNORM,
        TEXTURE_FORMAT_RG8_SNORM,
        TEXTURE_FORMAT_RGB8_SNORM,
        TEXTURE_FORMAT_RGBA8_SNORM,

        TEXTURE_FORMAT_R16_UNORM,
        TEXTURE_FORMAT_RG16_UNORM,
        TEXTURE_FORMAT_RGB16_UNORM,
        TEXTURE_FORMAT_RGBA16_UNORM,

        TEXTURE_FORMAT_R16_FLOAT,
        TEXTURE_FORMAT_RG16_FLOAT,
        TEXTURE_FORMAT_RGB16_FLOAT,
        TEXTURE_FORMAT_RGBA16_FLOAT,

        TEXTURE_FORMAT_R32_FLOAT,
        TEXTURE_FORMAT_RG32_FLOAT,
        TEXTURE_FORMAT_RGB32_FLOAT,
        TEXTURE_FORMAT_RGBA32_FLOAT,

        TEXTURE_FORMAT_SRGB8,
        TEXTURE_FORMAT_SRGB8_ALPHA8,

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
    MKT_NODISCARD auto InferSize( const Size elemCount ) -> Size {
        return sizeof( T ) * elemCount;
    }

    struct BufferDescription {
        Size SizeBytes{};
        Byte* Data{ nullptr };

        Size ElementCount{};
        Size ElementSize{};

        BufferUsage Usage{ BufferUsage::BUFFER_USAGE_VERTEX };
        BufferDataType Type{ BufferDataType::BUFFER_DATA_TYPE_UNKNOWN };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto ForElement( Size size, Size count ) -> BufferDescription&;
        auto WithSizeBytes( Size size ) -> BufferDescription&;
        auto WithData( Byte* data ) -> BufferDescription&;
        auto WithUsage( BufferUsage usage ) -> BufferDescription&;
        auto WithBufferDataType( BufferDataType type ) -> BufferDescription&;
        auto WithResourceUsageType( ResourceUsageType type ) -> BufferDescription&;
    };

    struct TextureDescription {
        Int32 Width{};
        Int32 Height{};
        Int32 ChannelCount{ 4 };

        Byte* Data{ nullptr };

        const File* TextureFile{ nullptr };

        MapType Map{ MapType::UNDEFINED_TEXTURE };

        TextureType Type{ TextureType::TEXTURE_2D };
        TextureUsage Usage{ TextureUsage::TEXTURE_USAGE_NORMAL };
        TextureFormat Format{ TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto WithWidth( Int32 width ) -> TextureDescription&;
        auto WithHeight( Int32 height ) -> TextureDescription&;
        auto WithChannelCount( Int32 channels ) -> TextureDescription&;

        auto WithMapType( MapType type ) -> TextureDescription&;

        auto WithFile( const File* file) -> TextureDescription&;

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
        const File* TextureFile{};
        MapType Map{ MapType::UNDEFINED_TEXTURE };
        TextureType Type{ TextureType::TEXTURE_UNKNOWN };


        auto WithMapType( MapType type ) -> TextureLoadDescription&;
        auto WithFile( const File* file ) -> TextureLoadDescription&;
        auto WithType( TextureType type ) -> TextureLoadDescription&;
    };

    struct TextureCubeLoadDescription {
        // +X -> right.jpg
        // -X -> left.jpg
        // +Y -> top.jpg      // might need vertical flip for vulkan
        // -Y -> bottom.jpg   // might need vertical flip for vulkan
        // -Z -> front.jpg
        // +Z -> back.jpg

        Path BasePath{};
        std::vector<Path> FacesRelativePaths{};
        TextureType Type{ TextureType::TEXTURE_UNKNOWN };

        TextureUsage Usage{ TextureUsage::TEXTURE_USAGE_CUBE };
        ResourceUsageType ResourceUsage{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto WithResourceUsage( ResourceUsageType usage ) -> TextureCubeLoadDescription&;
        auto WithTextureUsage( TextureUsage usage ) -> TextureCubeLoadDescription&;

        auto WithFacePath( const Path& file ) -> TextureCubeLoadDescription&;
        auto WithBasePath( const Path& file ) -> TextureCubeLoadDescription&;
        auto WithType( TextureType type ) -> TextureCubeLoadDescription&;
    };

    struct TextureCubeCreateDescription {
        std::vector<const File*> Faces{};
        ResourceUsageType ResourceUsage{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto WithResourceUsage( ResourceUsageType usage ) -> TextureCubeCreateDescription&;
        auto WithFacePath( const File* file ) -> TextureCubeCreateDescription&;
    };

    struct ShaderModuleDescription {
        const File* ShaderFile{};
        ShaderStage Stage{ ShaderStage::VERTEX_STAGE };

        auto WithShaderFile( const File* file ) -> ShaderModuleDescription&;
        auto WithStage( ShaderStage stage ) -> ShaderModuleDescription&;
    };

    struct SamplerDescription {
        bool CubeSampler{};

        SamplerFilter MinFilter{ SamplerFilter::FILTER_NEAREST };
        SamplerFilter MagFilter{ SamplerFilter::FILTER_NEAREST };
        SamplerWrapMode WrapS{ SamplerWrapMode::WRAP_REPEAT };
        SamplerWrapMode WrapT{ SamplerWrapMode::WRAP_REPEAT };

        auto WithMinFilter( SamplerFilter filter ) -> SamplerDescription&;
        auto WithMagFilter( SamplerFilter filter ) -> SamplerDescription&;
        auto WithWrapS( SamplerWrapMode wrap ) -> SamplerDescription&;
        auto WithWrapT( SamplerWrapMode wrap ) -> SamplerDescription&;
    };

    /**
    * @struct FontLoadDescription
    * @brief Holds information for loading a font.
    *
    * The `FontLoadInfo` structure stores metadata required to load a font,
    * It is simply a fluent interface for setting up properties to construct a font.
    */
    struct FontLoadDescription {
        const File* FontFile{};
        float FontSize{ 48 };

        /**
        * @brief Sets the path of the model.
        * @param file The absolute or relative path to the model file.
        * @return Reference to the modified ModelLoadInfo.
        */
        auto WithFile( const File* file ) -> FontLoadDescription&;

        /**
         * @brief Sets the pixel size of the font.
         * @param pixelSize The desired pixel size.
         * @return Reference to the modified FontLoadInfo.
         */
        auto WithPixelSize( float pixelSize ) -> FontLoadDescription&;
    };

    enum class GraphicsAPI {
        INVALID_API = -1,
        VULKAN_API,
        DIRECTX_12,
        DIRECTX_11,
        UNKNOWN,
    };

    enum class ShaderDataType {
        NONE,
        FLOAT_TYPE, // Represents a single float data type
        FLOAT2_TYPE,// Represents a two float data type
        FLOAT3_TYPE,// Represents a three float data type
        FLOAT4_TYPE,// Represents a four float data type

        MAT3_TYPE,// Represents 3x3 float matrix data type
        MAT4_TYPE,// Represents 4x4 float matrix data type

        UINT_TYPE, // Represents a single int data type
        INT_TYPE, // Represents a single int data type
        INT2_TYPE,// Represents a two int data type
        INT3_TYPE,// Represents a three int data type
        INT4_TYPE,// Represents a four int data type
        BOOL_TYPE,// Represents a single boolean data type
        COUNT,
    };

    enum class RenderResolution {
        RES_HD_720P,
        RES_FHD_1080,
        RES_QHD_1440P,
        RES_UHD_3120P,
    };

    auto InferElementCount(BufferDataType dataType, Size blockSize) -> Size;

    MKT_NODISCARD auto InferDimensions(RenderResolution resolution) -> std::pair<float, float>;

    // By default, textures are loaded with rgba format which is supported by most of gpus
    auto FreeImageData( Byte* data ) -> void;
    MKT_NODISCARD auto LoadImageFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc*;

    MKT_NODISCARD auto InferAPI( std::string_view apiName ) -> GraphicsAPI;

    class STBImageHDR final {

    };

    class StbImage final {
    public:
        explicit StbImage( const File* textureFile );

        ~StbImage();

        DISABLE_COPY_FOR( StbImage );

        StbImage( StbImage&& other ) noexcept;

        auto operator=( StbImage&& other ) noexcept -> StbImage&;

        MKT_NODISCARD auto GetData() const -> Byte* { return m_Data; }
        MKT_NODISCARD auto GetWidth() const -> Int32 { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32 { return m_Height; }
        MKT_NODISCARD auto GetChannels() const -> Int32 { return m_Channels; }

        MKT_NODISCARD auto IsValid() const -> bool { return m_Data != nullptr; }

    private:
        Int32 m_Width{};
        Int32 m_Height{};
        Int32 m_Channels{};
        Byte* m_Data{ nullptr };
    };
}// namespace Mikoto
#endif//RENDERUTILITY_HH
