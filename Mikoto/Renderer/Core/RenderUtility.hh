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
        VERTEX,
        INDEX,
        STAGING,
        UNIFORM,
        SSBO,
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
        VERTEX,
        FRAGMENT,
        COMPUTE,
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
        COLOR,
        DEPTH,
        STENCIL,
        DEPTH_STENCIL,
        NORMAL,
        STORAGE,      // compute shader writable
        CUBE,         // for environment maps
        RENDER_TARGET,// render target attachments
    };

    /**
     * @enum TextureFormat
     * @brief Enum representing various texture formats.
     *
     * This enum defines the format of the texture, determining how the texture data is stored in memory.
     * It includes formats such as RGBA and RGB, which describe how color data is represented per pixel.
     */
    enum class TextureFormat {
        INVALID = -1,

        R8_UNORM,
        RG8_UNORM,
        RGB8_UNORM,
        RGBA8_UNORM,

        R8_SNORM,
        RG8_SNORM,
        RGB8_SNORM,
        RGBA8_SNORM,

        R16_UNORM,
        RG16_UNORM,
        RGB16_UNORM,
        RGBA16_UNORM,

        R16_FLOAT,
        RG16_FLOAT,
        RGB16_FLOAT,
        RGBA16_FLOAT,

        R32_FLOAT,
        RG32_FLOAT,
        RGB32_FLOAT,
        RGBA32_FLOAT,

        SRGB8,
        SRGB8_ALPHA8,

        D16_UNORM,
        D24_UNORM_S8_UINT,
        D32_FLOAT,
        D32_FLOAT_S8_UINT,
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

    enum class LoadOp {
        CLEAR,
        LOAD,
        UNDEFINED,
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

        BufferUsage Usage{ BufferUsage::VERTEX };
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
        Size BufferSize{}; // Optional

        const File* TextureFile{ nullptr };

        MapType Map{ MapType::UNDEFINED_TEXTURE };

        TextureType Type{ TextureType::TEXTURE_2D };
        TextureUsage Usage{ TextureUsage::NORMAL };
        TextureFormat Format{ TextureFormat::RGBA8_SNORM };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto WithWidth( Int32 width ) -> TextureDescription&;
        auto WithHeight( Int32 height ) -> TextureDescription&;
        auto WithChannelCount( Int32 channels ) -> TextureDescription&;

        auto WithMapType( MapType type ) -> TextureDescription&;

        auto WithFile( const File* file) -> TextureDescription&;

        auto WithData( Byte* data ) -> TextureDescription&;
        auto WithSize( Size size ) -> TextureDescription&;
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
        bool IsHDR{ false };
        const File* TextureFile{};
        MapType Map{ MapType::UNDEFINED_TEXTURE };
        TextureType Type{ TextureType::TEXTURE_UNKNOWN };

        auto IsHDRMap( bool value ) -> TextureLoadDescription&;
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

        TextureUsage Usage{ TextureUsage::CUBE };
        ResourceUsageType ResourceUsage{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        bool IsHdrMap{ false };

        auto IsHDR( bool value ) -> TextureCubeLoadDescription&;

        auto WithResourceUsage( ResourceUsageType usage ) -> TextureCubeLoadDescription&;
        auto WithTextureUsage( TextureUsage usage ) -> TextureCubeLoadDescription&;

        auto WithFacePath( const Path& file ) -> TextureCubeLoadDescription&;
        auto WithBasePath( const Path& file ) -> TextureCubeLoadDescription&;
        auto WithType( TextureType type ) -> TextureCubeLoadDescription&;
    };

    struct TextureCubeCreateDescription {
        // If this is an HDR Faces[0] must have the path to the 2D image
        std::vector<const File*> Faces{};
        ResourceUsageType ResourceUsage{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        UInt32 MipLevels{ 1 };
        UInt32 Dimensions{ 1024 };
        TextureFormat Format{ TextureFormat::RGBA8_UNORM };
        TextureUsage Usage{ TextureUsage::CUBE };

        bool IsHdrMap{ false };

        auto IsHDR( bool value ) -> TextureCubeCreateDescription&;

        auto WithMipLevels( UInt32 levels ) -> TextureCubeCreateDescription&;
        auto WithTextureFormat( TextureFormat format ) -> TextureCubeCreateDescription&;
        auto WithDimensions( UInt32 dimensions ) -> TextureCubeCreateDescription&;
        auto WithUsageType( TextureUsage usage ) -> TextureCubeCreateDescription&;
        auto WithResourceUsage( ResourceUsageType usage ) -> TextureCubeCreateDescription&;
        auto WithFacePath( const File* file ) -> TextureCubeCreateDescription&;
    };

    struct ShaderModuleDescription {
        const File* ShaderFile{};
        ShaderStage Stage{ ShaderStage::VERTEX };

        auto WithShaderFile( const File* file ) -> ShaderModuleDescription&;
        auto WithStage( ShaderStage stage ) -> ShaderModuleDescription&;
    };

    struct SamplerDescription {
        bool CubeSampler{};

        float MipLevels{ 1.0f };

        SamplerFilter MinFilter{ SamplerFilter::FILTER_NEAREST };
        SamplerFilter MagFilter{ SamplerFilter::FILTER_NEAREST };
        SamplerWrapMode WrapU{ SamplerWrapMode::WRAP_CLAMP_TO_EDGE };
        SamplerWrapMode WrapV{ SamplerWrapMode::WRAP_CLAMP_TO_EDGE };
        SamplerWrapMode WrapW{ SamplerWrapMode::WRAP_CLAMP_TO_EDGE };

        auto WithMipLevels(float mipLevels) -> SamplerDescription&;
        auto WithMinFilter( SamplerFilter filter ) -> SamplerDescription&;
        auto WithMagFilter( SamplerFilter filter ) -> SamplerDescription&;

        auto WithWrapU( SamplerWrapMode wrap ) -> SamplerDescription&;
        auto WithWrapV( SamplerWrapMode wrap ) -> SamplerDescription&;
        auto WithWrapW( SamplerWrapMode wrap ) -> SamplerDescription&;
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
        HD_720P,
        FHD_1080,
        QHD_1440P,
        UHD_3120P,
    };

    MKT_NODISCARD auto InferAPI( std::string_view apiName ) -> GraphicsAPI;

    MKT_NODISCARD auto InferElementCount(BufferDataType dataType, Size blockSize) -> Size;

    MKT_NODISCARD auto InferDimensions(RenderResolution resolution) -> std::pair<float, float>;

    auto FreeImageData( Byte* data ) -> void;
    MKT_NODISCARD auto LoadImageFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc*;
    MKT_NODISCARD auto LoadImageFloatFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc*;

    class STBImageHDR final {
    public:
        explicit STBImageHDR( const File* textureFile );

        ~STBImageHDR();

        DISABLE_COPY_FOR( STBImageHDR );

        STBImageHDR( STBImageHDR&& other ) noexcept;

        auto operator=( STBImageHDR&& other ) noexcept -> STBImageHDR&;

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

    class StbImage final {
    public:
        explicit StbImage( const File* textureFile, bool isHDR = false );

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
