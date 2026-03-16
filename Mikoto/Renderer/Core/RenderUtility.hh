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

#ifndef MIKOTO_RENDER_UTILITY_HH
#define MIKOTO_RENDER_UTILITY_HH

#include <stb_image.h>

#include <Common/Common.hh>
#include <Library/IO/File.hh>

#include <Renderer/Core/DeviceObjectHandle.hh>

namespace Mikoto {

    enum class MapType {
        DIFFUSE_TEXTURE,

        BASE_COLOR_TEXTURE,
        NORMAL_TEXTURE,
        METALLIC_TEXTURE,
        ROUGHNESS_TEXTURE,
        METALLIC_ROUGHNESS_TEXTURE,
        AMBIENT_OCCLUSION_TEXTURE,
        EMISSIVE_TEXTURE,
        SPECULAR_GLOSSINESS,

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
        UNIFORM,
        SHADER_STORAGE,
        INDIRECT_DRAW,
        UNDEFINED,
    };

    enum class SamplerFilter {
        FILTER_NEAREST,
        FILTER_LINEAR,
    };

    enum class SamplerWrapMode {
        WRAP_REPEAT,
        MIRRORED_REPEAT,
        WRAP_CLAMP_TO_EDGE,
        WRAP_CLAMP_TO_BORDER,
    };

    enum class ShaderStage : UInt32 {
        STAGE_UNKNOWN,
        VERTEX,
        FRAGMENT,
        COMPUTE,
    };

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
        STORAGE,
        CUBE,
        RENDER_TARGET,
    };

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

        // Resource can be updated while GPU is reading from it
        RESOURCE_USAGE_DYNAMIC,

        RESOURCE_USAGE_STREAMING,
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

    enum class Multisampling {
        MSAA_X1 = 1,
        MSAA_X2 = 2,
        MSAA_X4 = 4,
        MSAA_X8 = 8,
        MSAA_X16 = 16
    };

    template<typename T>
    MKT_NODISCARD auto InferSize( const Size elemCount ) -> Size {
        return sizeof( T ) * elemCount;
    }

    struct BufferDescription {
        BufferSpanHandle BufferSpanHnd{}; 

        // Use the buffer view handle instead of this
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
        auto WidthHandle( BufferSpanHandle handle ) -> BufferDescription&;
        auto WithUsage( BufferUsage usage ) -> BufferDescription&;
        auto WithBufferDataType( BufferDataType type ) -> BufferDescription&;
        auto WithResourceUsageType( ResourceUsageType type ) -> BufferDescription&;
    };

    struct TextureDescription {
        std::string Name{};

        UInt32 MipLevelCount{ 1 };

        Int32 Width{};
        Int32 Height{};
        Int32 ChannelCount{ 4 };

        bool IsHDR{ false };

        Byte* Data{ nullptr };
        Size BufferSize{}; // Optional

        Multisampling MSAA{ Multisampling::MSAA_X1 };

        const File* TextureFile{ nullptr };

        MapType Map{ MapType::UNDEFINED_TEXTURE };

        TextureType Type{ TextureType::TEXTURE_2D };
        TextureUsage Usage{ TextureUsage::NORMAL };
        TextureFormat Format{ TextureFormat::RGBA8_SNORM };
        ResourceUsageType UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        auto IsHDRMap( bool value ) -> TextureDescription&;
        auto WithWidth( Int32 width ) -> TextureDescription&;
        auto WithName( std::string_view name ) -> TextureDescription&;
        auto WithHeight( Int32 height ) -> TextureDescription&;
        auto WithMipLevelCount( UInt32 count ) -> TextureDescription&;
        auto WithChannelCount( Int32 channels ) -> TextureDescription&;
        auto WithSampleCount( Multisampling sampleCount ) -> TextureDescription&;

        auto WithMapType( MapType type ) -> TextureDescription&;

        auto WithFile( const File* file) -> TextureDescription&;

        auto WithData( Byte* data ) -> TextureDescription&;
        auto WithSize( Size size ) -> TextureDescription&;
        auto WithType( TextureType type ) -> TextureDescription&;
        auto WithTextureUsage( TextureUsage usage ) -> TextureDescription&;

        auto WithFormat( TextureFormat format ) -> TextureDescription&;
        auto WithResourceType( ResourceUsageType type ) -> TextureDescription&;
    };

    struct TextureLoadDescription {
        bool IsHDR{ false };
        const File* TextureFile{};
        MapType Map{ MapType::UNDEFINED_TEXTURE };
        TextureType Type{ TextureType::TEXTURE_UNKNOWN };
        TextureFormat Format{ TextureFormat::RGBA8_UNORM };

        auto IsHDRMap( bool value ) -> TextureLoadDescription&;
        auto WithMapType( MapType type ) -> TextureLoadDescription&;
        auto WithFile( const File* file ) -> TextureLoadDescription&;
        auto WithType( TextureType type ) -> TextureLoadDescription&;
        auto WithFormat( TextureFormat format ) -> TextureLoadDescription&;
    };

    struct TextureCubeLoadDescription {
        Int32 Width{};
        Int32 Height{};
        Int32 MipLevels{ 4 };

        Byte* Data{ nullptr };

        // Base address of 6 cube faces
        // or path to the single cube image
        Path BasePath{};

        // Used when the cube is split in 6 faces
        std::vector<Path> FacesRelativePaths{};

        TextureUsage Usage{ TextureUsage::CUBE };
        TextureType Type{ TextureType::TEXTURE_UNKNOWN };
        ResourceUsageType ResourceUsage{ ResourceUsageType::RESOURCE_USAGE_STATIC };

        // Load as LDR. If we specify a path to
        // an Equirectangular image
        bool WantLDR{ false };

        auto LoadLDR( bool value ) -> TextureCubeLoadDescription&;

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

        Byte* Data{ nullptr };
        Size SizeBytes{};

        bool UseCubeImageLoader{ false };

        Int32 Width{};
        Int32 Height{};
        UInt32 MipLevels{ 1 };
        UInt32 Dimensions{ 1024 };

        TextureFormat Format{ TextureFormat::RGBA8_UNORM };
        TextureUsage Usage{ TextureUsage::CUBE };

        Multisampling MSAA{ Multisampling::MSAA_X1 };

        // If set the cube is loaded as an LDR
        bool WantLDR{ false };

        bool IsFacesSplit{ false };

        auto LoadLDR( bool value ) -> TextureCubeCreateDescription&;

        auto WidthSize( Size size ) -> TextureCubeCreateDescription&;
        auto WithWidth( Int32 width ) -> TextureCubeCreateDescription&;
        auto WithHeight( Int32 height ) -> TextureCubeCreateDescription&;
        auto WithData( Byte* data ) -> TextureCubeCreateDescription&;
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
        SamplerWrapMode WrapU{ SamplerWrapMode::WRAP_REPEAT };
        SamplerWrapMode WrapV{ SamplerWrapMode::WRAP_REPEAT };
        SamplerWrapMode WrapW{ SamplerWrapMode::WRAP_REPEAT };

        Vec4F BorderColor{ 0.0f, 0.0f, 0.0f, 1.0f };

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

    MKT_NODISCARD auto GetChanelString( UInt32 channelCount ) -> std::string_view;
    MKT_NODISCARD auto GetTextureFormatString( TextureFormat format ) -> std::string_view;

    MKT_NODISCARD auto InferAPI( std::string_view apiName ) -> GraphicsAPI;
    MKT_NODISCARD auto InferElementCount(BufferDataType dataType, Size blockSize) -> Size;
    MKT_NODISCARD auto InferDimensions(RenderResolution resolution) -> std::pair<float, float>;

    MKT_NODISCARD auto LoadImageFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc*;
    MKT_NODISCARD auto LoadImageFloatFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc*;
    MKT_NODISCARD auto LoadImageFromMemory( const Byte* buffer, Size sizeBytes, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc*;
    MKT_NODISCARD auto LoadImageFromMemory( const Byte* buffer, Size sizeBytes, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc*;

    auto FreeImageData( Byte* data ) -> void;

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

    class ImageLoader2D final {
    public:
        explicit ImageLoader2D( const File* textureFile );

        explicit ImageLoader2D( const Byte* data, Size sizeBytes );

        ~ImageLoader2D();

        DISABLE_COPY_FOR( ImageLoader2D );

        ImageLoader2D( ImageLoader2D&& other ) noexcept;

        auto operator=( ImageLoader2D&& other ) noexcept -> ImageLoader2D&;

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

    class ImageLoaderCube final {
    public:
        explicit ImageLoaderCube( const Path& fileName );

        ImageLoaderCube( ImageLoaderCube&& other ) noexcept;
        auto operator=( ImageLoaderCube&& other ) noexcept -> ImageLoaderCube&;

        MKT_NODISCARD auto GetData() const -> Byte* { return m_Data; }
        MKT_NODISCARD auto GetSize() const -> Size { return m_Size; }
        MKT_NODISCARD auto GetWidth() const -> Int32 { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32 { return m_Height; }
        MKT_NODISCARD auto GetMipLevels() const -> Int32 { return m_MipLevels; }

        MKT_NODISCARD auto IsValid() const -> bool { return m_Data != nullptr; }

        ~ImageLoaderCube();

    public:
        DISABLE_COPY_FOR( ImageLoaderCube );

    private:
        Int32 m_Width{};
        Int32 m_Height{};
        Size m_MipLevels{};
        Size m_Size{};
        Byte* m_Data{ nullptr };
    };
}

#endif//MIKOTO_RENDER_UTILITY_HH
