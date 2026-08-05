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

#ifndef MIKOTO_RHI_HH
#define MIKOTO_RHI_HH

#include <EASTL/span.h>
#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <EASTL/utility.h>
#include <EASTL/any.h>// Any requires lot of shit from utility so it needs to go after

#include <glm/glm.hpp>

#include <Assets/Image.hh>

#include <Core/Core.hh>
#include <Core/Flag.hh>
#include <Core/Types.hh>
#include <Core/ResourcePool.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

#include <Memory/BufferSpan.hh>

namespace mikoto::renderer {
    class IGpuDevice;
}

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;
    using namespace mikoto::memory;

    // TODO: List of RHI improvements
    // - Specify which description parameters are required to speak the same language for all supported APIs,
    // for example when calling BindVertexBuffer the vertex description must specify SetElementStride(...)

    using MipLevel = u32;
    using ArraySlice = u32;

    // Synchronization
    struct PipelineStageFlagsProperties {
        using Data = u32;
    };
    using PipelineStageFlags = Flags<PipelineStageFlagsProperties>;

    struct PipelineStageFlagsBits {
        static constexpr PipelineStageFlags kNone{ 0 };
        static constexpr PipelineStageFlags kTop{ BIT_SET(0) };
        static constexpr PipelineStageFlags kDrawIndirect{ BIT_SET(1) };
        static constexpr PipelineStageFlags kVertexInput{ BIT_SET(2) };
        static constexpr PipelineStageFlags kVertexShader{ BIT_SET(3) };
        static constexpr PipelineStageFlags kHullShader{ BIT_SET(4) };
        static constexpr PipelineStageFlags kDomainShader{ BIT_SET(5) };
        static constexpr PipelineStageFlags kGeometryShader{ BIT_SET(6) };
        static constexpr PipelineStageFlags kPixelShader{ BIT_SET(7) };
        static constexpr PipelineStageFlags kComputeShader{ BIT_SET(8) };
        static constexpr PipelineStageFlags kColorAttachment{ BIT_SET(9) };
        static constexpr PipelineStageFlags kDepthStencil{ BIT_SET(10) };
        static constexpr PipelineStageFlags kCopy{ BIT_SET(11) };
        static constexpr PipelineStageFlags kBottom{ BIT_SET(12) };
        static constexpr PipelineStageFlags kAll{ 0xFFFFFFFF };
    };

    struct AccessFlagsProperties {
        using Data = u32;
    };
    using AccessFlags = Flags<AccessFlagsProperties>;

    struct AccessFlagsBits {
        static constexpr AccessFlags kNone{ 0 };
        static constexpr AccessFlags kIndirectRead{ BIT_SET(0) };
        static constexpr AccessFlags kIndexRead{ BIT_SET(1) };
        static constexpr AccessFlags kVertexRead{ BIT_SET(2) };
        static constexpr AccessFlags kConstantRead{ BIT_SET(3) };
        static constexpr AccessFlags kShaderRead{ BIT_SET(4) };
        static constexpr AccessFlags kShaderWrite{ BIT_SET(5) };
        static constexpr AccessFlags kRenderTargetRead{ BIT_SET(6) };
        static constexpr AccessFlags kRenderTargetWrite{ BIT_SET(7) };
        static constexpr AccessFlags kDepthStencilRead{ BIT_SET(8) };
        static constexpr AccessFlags kDepthStencilWrite{ BIT_SET(9) };
        static constexpr AccessFlags kCopyRead{ BIT_SET(10) };
        static constexpr AccessFlags kCopyWrite{ BIT_SET(11) };
    };

    struct TextureLayoutProperties {
        using Data = u32;
    };
    using TextureLayout = Flags<TextureLayoutProperties>;

    struct TextureLayoutBits {
        static constexpr TextureLayout kUnknown{ 0 };
        static constexpr TextureLayout kGeneral{ BIT_SET(1) };
        static constexpr TextureLayout kColorAttachment{ BIT_SET(2) };
        static constexpr TextureLayout kDepthStencil{ BIT_SET(3) };
        static constexpr TextureLayout kShaderResource{ BIT_SET(4) };
        static constexpr TextureLayout kUnorderedAccess{ BIT_SET(5) }; // For RW textures
        static constexpr TextureLayout kCopySrc{ BIT_SET(6) };
        static constexpr TextureLayout kCopyDst{ BIT_SET(7) };
        static constexpr TextureLayout kPresent{ BIT_SET(8) };
    };

    enum class ObjectType {
        // Vulkan
        Vk_Device,
        Vk_Buffer,
        Vk_Sampler,
        Vk_Shader,
        Vk_Format,
        Vk_Image,
        Vk_ImageView,
        Vk_Framebuffer,
        Vk_CmdPool,
        Vk_CmdBuffer,
        Vk_DescriptorSetLayout,
        Vk_DescriptorSet,
        Vk_Pipeline,
        Vk_PipelineLayout,
        Vk_Semaphore,

        // D3D12
        D3D12_Fence,
        D3D12_Device,

        // D3D11
        D3D11_Device,
        D3D11_Texture2D,
        D3D11_RTV,
        D3D11_SRV,
        D3D11_DSV,

        D3D11_Buffer,
        D3D11_Sampler,
        D3D11_D3DBlob,
        D3D11_CommandList,
        D3D11_Shader,
        D3D11_VertexShader,
        D3D11_PixelShader,
        D3D11_InputLayout,
        D3D11_BlendState,
        D3D11_RasterizerState,
        D3D11_DepthStencilState,
    };

    enum class LoadOp {
        eLoad,
        eClear,
        eDontCare,
    };

    enum class SamplerFilter {
        eNearest,
        eLinear,
    };

    enum class InputRate {
        ePerVertex,
        ePerInstance
    };

    enum class SamplerWrapMode {
        eRepeat,
        eMirroredRepeat,
        eClampToEdge,
        eClampToBorder,
    };

    enum class PrimitiveTopology {
        eInvalid = -1,
        ePointList,
        eLineList,
        eLineStrip,
        eTriangleList,
        eTriangleStrip,
        eTriangleFan
    };

    enum class CullMode {
        eNone,
        eCullFront,
        eCullBack,
    };

    enum class DepthCompareOp {
        eNever,
        eLess,
        eEqual,
        eLessOrEqual,
        eGreater,
        eNotEqual,
        eGreaterOrEqual,
        eAlways
    };

    enum class PolygonMode {
        eLines,
        ePoint,
        eFill
    };

    enum class GraphicsAPI {
        eInvalid = -1,
        eVulkan,
        eD3D12,
        eD3D11,
    };

    enum class RenderResolution {
        e720P,
        e1080P,
        e1440P,
        e3120P,
    };

    enum class ShaderDataType {
        eInvalid = -1,
        eFloat, // Represents a single float data type
        eFloat2,// Represents a two float data type
        eFloat3,// Represents a three float data type
        eFloat4,// Represents a four float data type

        eFloat3x3,// Represents 3x3 float matrix data type
        eFloat4x4,// Represents 4x4 float matrix data type

        eUInt,// Represents a single int data type
        eInt, // Represents a single int data type
        eInt2,// Represents a two int data type
        eInt3,// Represents a three int data type
        eInt4,// Represents a four int data type
        eBool,// Represents a single boolean data type
    };

    enum class PipelineType {
        eInvalid = -1,
        eGraphics,
        eCompute,
    };

    enum class WindingOrder {
        eClockwise,
        eCounterClockwise,
    };

    enum class AttachmentLoadOp {
        eInvalid = -1,
        eClear,
        eLoad,
    };

    enum class Format {
        eUnknown,

        eR8_UINT,
        eR8_SINT,
        eR8_UNORM,
        eR8_SNORM,
        eRG8_UINT,
        eRG8_SINT,
        eRG8_UNORM,
        eRG8_SNORM,
        eR16_UINT,
        eR16_SINT,
        eR16_UNORM,
        eR16_SNORM,
        eR16_FLOAT,
        eBGRA4_UNORM,
        eB5G6R5_UNORM,
        eB5G5R5A1_UNORM,
        eRGBA8_UINT,
        eRGBA8_SINT,
        eRGBA8_UNORM,
        eRGBA8_SNORM,
        eBGRA8_UNORM,
        eBGRX8_UNORM,
        eSRGBA8_UNORM,
        eSBGRA8_UNORM,
        eSBGRX8_UNORM,
        eR10G10B10A2_UNORM,
        eR11G11B10_FLOAT,
        eRG16_UINT,
        eRG16_SINT,
        eRG16_UNORM,
        eRG16_SNORM,
        eRG16_FLOAT,
        eR32_UINT,
        eR32_SINT,
        eR32_FLOAT,
        eRGBA16_UINT,
        eRGBA16_SINT,
        eRGBA16_FLOAT,
        eRGBA16_UNORM,
        eRGBA16_SNORM,
        eRG32_UINT,
        eRG32_SINT,
        eRG32_FLOAT,
        eRGB32_UINT,
        eRGB32_SINT,
        eRGB32_FLOAT,
        eRGBA32_UINT,
        eRGBA32_SINT,
        eRGBA32_FLOAT,

        eD16,
        eD24S8,
        eX24G8_UINT,
        eD32,
        eD32S8,
        eX32G8_UINT,

        eBC1_UNORM,
        eBC1_UNORM_SRGB,
        eBC2_UNORM,
        eBC2_UNORM_SRGB,
        eBC3_UNORM,
        eBC3_UNORM_SRGB,
        eBC4_UNORM,
        eBC4_SNORM,
        eBC5_UNORM,
        eBC5_SNORM,
        eBC6H_UFLOAT,
        eBC6H_SFLOAT,
        eBC7_UNORM,
        eBC7_UNORM_SRGB,

        eCount,
    };

    enum class Multisampling {
        eMsaaX1,
        eMsaaX2,
        eMsaaX4,
        eMsaaX8,
        eMsaaX16,
    };

    enum class HeapType {
        eDeviceLocal,
        eUpload,
        eReadback
    };

    enum class CpuAccessType {
        eNone,
        eRead,
        eWrite,
    };

    enum class TextureDimension {
        eInvalid = -1,
        eTexture1D,
        eTexture1DArray,
        eTexture2D,
        eTexture2DArray,
        eTextureCube,
        eTextureCubeArray,
        eTexture2DMS,
        eTexture2DMSArray,
        eTexture3D
    };

    enum class ResourceStates {
        eUnknown,
        eCommon,
        eConstantBuffer,
        eVertexBuffer,
        eIndexBuffer,
        eIndirectArgument,
        eShaderResource,
        eUnorderedAccess,
        eRenderTarget,
        eDepthWrite,
        eDepthRead,
        eCopyDest,
        eCopySource,
        eResolveDest,
        eResolveSource,
        ePresent,
        eAccelStructRead,
        eAccelStructWrite,
        eAccelStructBuildInput,
        eAccelStructBuildBlas,
    };

    // Describes how a resource is used by shaders
    enum class ResourceType {
        eInvalid = -1,

        eTexture_SRV,
        eTexture_UAV,
        eTypedBuffer_SRV,
        eTypedBuffer_UAV,
        eStructuredBuffer_SRV,
        eStructuredBuffer_UAV,
        eRawBuffer_SRV,
        eRawBuffer_UAV,
        eConstantBuffer,
        eSampler,
        eRayTracingAccelStruct,
        ePushConstants,
        eVolatileConstantBuffer,
        eSamplerFeedbackTexture_UAV,
    };

    enum class BufferDataType {
        eInvalid = -1,
        eUInt32,
        eUInt16,
        eFloat32,
    };

    enum class FormatKind {
        Integer,
        Normalized,
        Float,
        DepthStencil
    };

    struct FormatInfo {
        Format mFormat;
        const char* mName;
        u8 mBytesPerBlock;
        u8 mBlockSize;
        FormatKind mKind;
        bool mHasRed : 1;
        bool mHasGreen : 1;
        bool mHasBlue : 1;
        bool mHasAlpha : 1;
        bool mHasDepth : 1;
        bool mHasStencil : 1;
        bool mIsSigned : 1;
        bool mIsSRGB : 1;
    };

    struct BufferUsageFlagsProperties {
        using Data = u32;
    };

    using BufferUsageFlags = Flags<BufferUsageFlagsProperties>;

    struct BufferUsageFlagsBits {
        static constexpr BufferUsageFlags kNone{ 0 };

        // Pipeline bindings
        static constexpr BufferUsageFlags kVertex{ BIT_SET(0) };
        static constexpr BufferUsageFlags kIndex{ BIT_SET(1) };
        static constexpr BufferUsageFlags kConstant{ BIT_SET(2) };
        static constexpr BufferUsageFlags kStorage{ BIT_SET(3) };

        // Indirect
        static constexpr BufferUsageFlags kIndirectDraw{ BIT_SET(5) };

        // Transfer / copy
        static constexpr BufferUsageFlags kCopySrc{ BIT_SET(6) };
        static constexpr BufferUsageFlags kCopyDst{ BIT_SET(7) };
    };

    struct TextureFlagsProperties {
        using Data = u32;
    };

    using TextureUsageFlags = Flags<TextureFlagsProperties>;

    struct TextureUsageFlagsBits {
        static constexpr TextureUsageFlags kNone{ 0 };

        static constexpr TextureUsageFlags kRenderTarget{ BIT_SET( 0 ) };
        static constexpr TextureUsageFlags kDepthTarget{ BIT_SET( 1 ) };
        static constexpr TextureUsageFlags kStencilTarget{ BIT_SET( 2 ) };
        static constexpr TextureUsageFlags kDepthStencilTarget{ BIT_SET( 3 ) };
        static constexpr TextureUsageFlags kShaderResource{ BIT_SET( 4 ) };

        static constexpr TextureUsageFlags kCopySrc{ BIT_SET(5) };
        static constexpr TextureUsageFlags kCopyDst{ BIT_SET(6) };
    };

    struct ShaderFlagsProperties {
        using Data = u32;
    };

    using ShaderStage = Flags<ShaderFlagsProperties>;

    // For resource visibility
    struct ShaderFlagsBits {
        static constexpr ShaderStage kVertex{ BIT_SET( 0 ) };
        static constexpr ShaderStage kPixel{ BIT_SET( 1 ) };
        static constexpr ShaderStage kCompute{ BIT_SET( 2 ) };

        static constexpr ShaderStage kGeometry{ BIT_SET( 3 ) };

        static constexpr ShaderStage kHull{ BIT_SET( 4 ) };
        static constexpr ShaderStage kDomain{ BIT_SET( 5 ) };

        static constexpr ShaderStage kRayGeneration{ BIT_SET( 6 ) };
        static constexpr ShaderStage kIntersection{ BIT_SET( 7 ) };
        static constexpr ShaderStage kAnyHit{ BIT_SET( 8 ) };
        static constexpr ShaderStage kClosestHit{ BIT_SET( 9 ) };
        static constexpr ShaderStage kMiss{ BIT_SET( 10 ) };

        static constexpr ShaderStage kAll{ ~0U };
    };

    enum class ShaderType {
        eInvalid = -1,

        eVertex,
        ePixel,
        eCompute,

        eGeometry,

        eHull,
        eDomain,

        // Ray tracing
        eRayGeneration,
        eIntersection,
        eAnyHit,
        eClosestHit,
        eMiss,

        eCount
    };

    enum class QueueType {
        eInvalid = -1,
        eTransfer,
        eGraphics,
        eCompute,
        ePresent,
    };

    struct QueueOpSupportFlagsProperties {
        using Data = u32;
    };

    using QueueOpSupportFlags = Flags<QueueOpSupportFlagsProperties>;

    struct QueueOpSupportFlagsBits {
        static constexpr QueueOpSupportFlags kGraphics{ BIT_SET( 0 ) };
        static constexpr QueueOpSupportFlags kTransfer{ BIT_SET( 1 ) };
        static constexpr QueueOpSupportFlags kCompute{ BIT_SET( 2 ) };
        static constexpr QueueOpSupportFlags kPresentation{ BIT_SET( 3 ) };
    };

    // Describes a piece of a buffer
    struct BufferRange {
        u64 mByteOffset = 0;
        u64 mByteSize = 0;

        BufferRange() = default;

        BufferRange( u64 byteOffset, u64 byteSize )
            : mByteOffset{ byteOffset }, mByteSize{ byteSize } {}

        //[[nodiscard]] constexpr bool IsEntireBuffer(const BufferDesc& desc) const { return (byteOffset == 0) && (byteSize == ~0ull || byteSize == desc.byteSize); }
        //constexpr bool operator== (const BufferRange& other) const { return byteOffset == other.byteOffset && byteSize == other.byteSize; }

        auto SetByteOffset( u64 value ) -> BufferRange&;
        auto SetByteSize( u64 value ) -> BufferRange&;

        auto Validate(size_t bufferByteSize ) -> BufferRange&;
    };

    struct TextureSubresourceSet {
        static constexpr MipLevel AllMipLevels{ MipLevel(-1) };
        static constexpr ArraySlice AllArraySlices{ ArraySlice(-1) };

        MipLevel mBaseMipLevel{ 0 };
        MipLevel mNumMipLevels{ 1 };
        ArraySlice mBaseArraySlice{ 0 };
        ArraySlice mNumArraySlices{ 1 };

        TextureSubresourceSet() = default;

        TextureSubresourceSet(MipLevel baseMipLevel, MipLevel numMipLevels, ArraySlice baseArraySlice, ArraySlice numArraySlices)
            : mBaseMipLevel(baseMipLevel)
            , mNumMipLevels(numMipLevels)
            , mBaseArraySlice(baseArraySlice)
            , mNumArraySlices(numArraySlices)
        {
        }
    };

    static inline constexpr u32 kMaxViewports{ 16 };
    static inline constexpr u32 kMaxScissors{ 10 };
    static inline constexpr u32 kMaxShaders{ 15 };
    static inline constexpr u32 kMaxSlotsPerTable{ 15 };
    static inline constexpr u32 kMaxColorFormats{ 10 };
    static inline constexpr u32 kMaxCubeFaces{ 6 };
    static inline constexpr u32 kMaxRenderTargets{ 8 };
    static inline constexpr u32 kMaxVertexAttributes{ 16 };
    static inline constexpr u32 kMaxVertexBindings{ 16 };
    static inline constexpr u32 kMaxVertexBuffers{ 8 };
    static inline constexpr u32 kMaxBindingLayouts{ 8 };
    static inline constexpr u32 kMaxBarriers{ 25 };
    static inline constexpr u32 kMaxBindlessRegisterSpaces{ 16 };
    static inline constexpr u32 kMaxPushConstantSize{ 128 }; // Vulkan guarantees 128 bytes across supported devices

    static inline const BufferRange EntireBuffer{ BufferRange(0, ~0ull) };
    static inline const TextureSubresourceSet AllSubResources{ TextureSubresourceSet(0, TextureSubresourceSet::AllMipLevels, 0, TextureSubresourceSet::AllArraySlices) };

    MKT_NODISCARD auto InferAPI( eastl::string_view apiName ) -> GraphicsAPI;

    MKT_NODISCARD auto GetFormatInfo(Format format) -> const FormatInfo&;

    MKT_NODISCARD auto InferDimensions( RenderResolution resolution ) -> eastl::pair<float, float>;
    MKT_NODISCARD auto InferDimensions( RenderResolution resolution, u32 mipLevel ) -> eastl::pair<u32, u32>;
    MKT_NODISCARD auto InferDimensions( float width, float height, u32 mipLevel ) -> eastl::pair<u32, u32>;

    MKT_NODISCARD auto InferElementCount( Format dataType, size_t sizeBytes ) -> size_t;

    MKT_NODISCARD auto IsBuffer(ResourceType type) noexcept -> bool;
    MKT_NODISCARD auto IsTexture(ResourceType type) noexcept -> bool;
    MKT_NODISCARD auto IsSampler(ResourceType type) noexcept -> bool;

    struct Color {
        f32 mR{};
        f32 mG{};
        f32 mB{};
        f32 mA{};

        Color() = default;

        Color( f32 c )
            : mR{ c }, mG{ c }, mB{ c }, mA{ c } {}

        Color( const float4& c )
            : mR{ c.r }, mG{ c.g }, mB{ c.b }, mA{ c.a } {}

        Color( f32 r, f32 g, f32 b, f32 a )
            : mR{ r }, mG{ g }, mB{ b }, mA{ a } {}

        auto operator==( const Color& other ) const -> bool {
            return mR == other.mR && mG == other.mG && mB == other.mB && mA == other.mA;
        }

        auto operator!=( const Color& other ) const -> bool {
            return !( *this == other );
        }

        friend auto operator/( const Color& color, float value ) -> Color {
            return { color.mR / value, color.mG / value, color.mB / value, color.mA / value };
        }

        friend auto operator+( const Color& color, float value ) -> Color {
            return { color.mR + value, color.mG + value, color.mB + value, color.mA + value };
        }

        friend auto operator+( float value, const Color& color ) -> Color {
            return { color.mR + value, color.mG + value, color.mB + value, color.mA + value };
        }

        friend auto operator/( float value, const Color& color ) -> Color {
            return { color.mR / value, color.mG / value, color.mB / value, color.mA / value };
        }

        friend auto operator/( const Color& lhs, const Color& rhs ) -> Color {
            return { lhs.mR / rhs.mR, lhs.mG / rhs.mG, lhs.mB / rhs.mB, lhs.mA / rhs.mA };
        }
    };

    static const inline Color kColorWhite{ 1.f };
    static const inline Color kColorBlack{ 0.f };
    static const inline Color kColorTransparent{ 0.f, 0.f, 0.f, 0.f };

    static const inline Color kColorRed{ 1.f, 0.f, 0.f, 1.f };
    static const inline Color kColorGreen{ 0.f, 1.f, 0.f, 1.f };
    static const inline Color kColorBlue{ 0.f, 0.f, 1.f, 1.f };

    static const inline Color kColorYellow{ 1.f, 1.f, 0.f, 1.f };
    static const inline Color kColorCyan{ 0.f, 1.f, 1.f, 1.f };
    static const inline Color kColorMagenta{ 1.f, 0.f, 1.f, 1.f };

    static const inline Color kColorGray{ 0.5f, 0.5f, 0.5f, 1.f };
    static const inline Color kColorLightGray{ 0.75f, 0.75f, 0.75f, 1.f };
    static const inline Color kColorDarkGray{ 0.25f, 0.25f, 0.25f, 1.f };

    static const inline Color kColorOrange{ 1.f, 0.5f, 0.f, 1.f };
    static const inline Color kColorPink{ 1.f, 0.75f, 0.8f, 1.f };

    struct Viewport {
        f32 mMinX{};
        f32 mMaxX{};
        f32 mMinY{};
        f32 mMaxY{};
        f32 mMinZ{ 0.f };
        f32 mMaxZ{ 1.f };

        bool mFlip{ false };

        Viewport() = default;

        Viewport( f32 width, f32 height )
            : mMinX{ 0.f }, mMaxX{ width }, mMinY{ 0.f }, mMaxY{ height }, mMinZ{ 0.f }, mMaxZ{ 1.f } {}

        Viewport( f32 minX, f32 maxX, f32 minY, f32 maxY, f32 minZ, f32 maxZ )
            : mMinX{ minX }, mMaxX{ maxX }, mMinY{ minY }, mMaxY{ maxY }, mMinZ{ minZ }, mMaxZ{ maxZ } {}

        auto operator==( const Viewport& other ) const -> bool {
            return mMinX == other.mMinX && mMinY == other.mMinY && mMinZ == other.mMinZ && mMaxX == other.mMaxX && mMaxY == other.mMaxY && mMaxZ == other.mMaxZ;
        }

        auto operator!=( const Viewport& other ) const -> bool {
            return !( *this == other );
        }

        MKT_NODISCARD auto GetWidth() const -> float {
            return mMaxX - mMinX;
        }

        MKT_NODISCARD auto GetHeight() const -> float {
            return mMaxY - mMinY;
        }
    };

    struct Rect {
        i32 mMinX{};
        i32 mMaxX{};
        i32 mMinY{};
        i32 mMaxY{};

        Rect() = default;

        Rect( i32 width, i32 height )
            : mMinX{ 0 }, mMaxX{ width }, mMinY{ 0 }, mMaxY{ height } {}

        Rect( i32 minX, i32 maxX, i32 minY, i32 maxY )
            : mMinX{ minX }, mMaxX{ maxX }, mMinY{ minY }, mMaxY{ maxY } {}

        explicit Rect( const Viewport& viewport )
            : mMinX{ as<i32>( glm::floor( viewport.mMinX ) ) },
              mMaxX{ as<i32>( glm::ceil( viewport.mMaxX ) ) },
              mMinY{ as<i32>( glm::floor( viewport.mMinY ) ) },
              mMaxY{ as<i32>( glm::ceil( viewport.mMaxY ) ) } {}

        auto operator==( const Rect& other ) const -> bool {
            return mMinX == other.mMinX && mMinY == other.mMinY && mMaxX == other.mMaxX && mMaxY == other.mMaxY;
        }

        auto operator!=( const Rect& other ) const -> bool {
            return !( *this == other );
        }

        MKT_NODISCARD auto ComputeWidth() const -> i32 {
            return mMaxX - mMinX;
        }

        MKT_NODISCARD auto ComputeHeight() const -> i32 {
            return mMaxY - mMinY;
        }
    };

    struct ViewportState {
        //These are in pixels
        // note: you can only set each of these either in the PSO or per draw call in DrawArguments
        // it is not legal to have the same state set in both the PSO and DrawArguments
        // leaving these vectors empty means no state is set
        eastl::fixed_vector<Viewport, kMaxViewports> mViewports{};
        eastl::fixed_vector<Rect, kMaxViewports> mScissorRects{};

        auto AddViewport(const Viewport& v) -> ViewportState& { mViewports.push_back(v); return *this; }
        auto AddScissorRect(const Rect& r) -> ViewportState& { mScissorRects.push_back(r); return *this; }
        auto AddViewportAndScissorRect(const Viewport& v) -> ViewportState& { return AddViewport(v).AddScissorRect(Rect(v)); }
    };

    struct DrawArguments {
        u32 mVertexCount{ 0 };
        u32 mIndexCount{ 0 };

        u32 mInstanceCount{ 1 };
        u32 mFirstVertex{ 0 };
        u32 mFirstInstance{ 0 };

        u32 mFirstIndex{ 0 };
        i32 mVertexOffset{ 0 };

        constexpr auto SetVertexCount(u32 value) -> DrawArguments& { mVertexCount = value; return *this; }
        constexpr auto SetIndexCount(u32 value) -> DrawArguments& { mIndexCount = value; return *this; }
        constexpr auto SetInstanceCount(u32 value) -> DrawArguments& { mInstanceCount = value; return *this; }
        constexpr auto SetFirstVertex(u32 value) -> DrawArguments& { mFirstVertex = value; return *this; }
        constexpr auto SetFirstInstance(u32 value) -> DrawArguments& { mFirstInstance = value; return *this; }
        constexpr auto SetFirstIndex(u32 value) -> DrawArguments& { mFirstIndex = value; return *this; }
        constexpr auto SetVertexOffset(i32 value) -> DrawArguments& { mVertexOffset = value; return *this; }
    };

    struct DrawIndirectArguments {
        u32 mVertexCount{ 0 };
        u32 mInstanceCount{ 1 };
        u32 mStartVertexLocation{ 0 };
        u32 mStartInstanceLocation{ 0 };

        constexpr auto SetVertexCount(u32 value) -> DrawIndirectArguments& { mVertexCount = value; return *this; }
        constexpr auto SetInstanceCount(u32 value) -> DrawIndirectArguments& { mInstanceCount = value; return *this; }
        constexpr auto SetStartVertexLocation(u32 value) -> DrawIndirectArguments& { mStartVertexLocation = value; return *this; }
        constexpr auto SetStartInstanceLocation(u32 value) -> DrawIndirectArguments& { mStartInstanceLocation = value; return *this; }
    };

    struct DrawIndexedIndirectArguments {
        u32 mIndexCount{ 0 };
        u32 mInstanceCount{ 1 };
        u32 mStartIndexLocation{ 0 };
        i32 mBaseVertexLocation{ 0 };
        u32 mStartInstanceLocation{ 0 };

        constexpr auto SetIndexCount(u32 value) -> DrawIndexedIndirectArguments& { mIndexCount = value; return *this; }
        constexpr auto SetInstanceCount(u32 value) -> DrawIndexedIndirectArguments& { mInstanceCount = value; return *this; }
        constexpr auto SetStartIndexLocation(u32 value) -> DrawIndexedIndirectArguments& { mStartIndexLocation = value; return *this; }
        constexpr auto SetBaseVertexLocation(i32 value) -> DrawIndexedIndirectArguments& { mBaseVertexLocation = value; return *this; }
        constexpr auto SetStartInstanceLocation(u32 value) -> DrawIndexedIndirectArguments& { mStartInstanceLocation = value; return *this; }
    };

    struct TextureSlice {
        u32 x{};
        u32 y{};
        u32 z{};

        u32 mWidth{};
        u32 mHeight{};
        u32 mDepth{};
        u32 mMipLevel{};

        u32 mArrayLayer{};// Cube face for example
    };

    struct CommandListParameters {
        u32 mMaxThreadConcurrency{}; // Set to value > 0 to support recording from multiple threads from same command buffer
        QueueType mQueueType{ QueueType::eGraphics };

        auto SetQueueType(QueueType value) -> CommandListParameters& { mQueueType = value; return *this; }
        auto SetMaxThreadConcurrency(u32 value) -> CommandListParameters& { mMaxThreadConcurrency = value; return *this; }
    };

    struct Object {
        enum class Type {
            Pointer,
            Integer,
            None
        };

        Type mType{ Type::None };
        void* mPointer{ nullptr };
        u64 mInteger{ 0 };

        explicit Object(void* p) : mType(Type::Pointer), mPointer(p) {}
        explicit Object(u64 i) : mType(Type::Integer), mInteger(i) {}
        Object() = default;

        template<typename T>
        operator T*() const {
            if (mType == Type::Pointer) {
                return as<T*>(mPointer);
            }

            return nullptr;
        }
    };

    class DeviceObject : public IResource {
    public:

        explicit DeviceObject() = default;

        auto Initialize( IGpuDevice* device ) -> void;

        auto SetResourceState( ResourceStates state ) -> void { mResourceState.store( state ); }
        MKT_NODISCARD auto GetResourceState() const -> ResourceStates { return mResourceState.load(); }

        virtual auto SetDebugName(const eastl::string_view name) -> void { mDebugName = name; }

        MKT_NODISCARD auto GetDebugName() const -> const eastl::string& { return mDebugName; }
        MKT_NODISCARD static auto GetDefaultDebugName() -> eastl::string_view { return "DeviceObject"; }

        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType ) -> Object { return Object(nullptr); }
        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType type ) const -> Object { return const_cast<DeviceObject*>(this)->GetNativeHandle( type ); }

        MKT_NODISCARD auto GetHeapType() const -> HeapType { return mHeapType; }

        ~DeviceObject() override = default;

    protected:

        DeviceObject( HeapType heapType, ResourceType resourceType );

        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;

    protected:
        IGpuDevice* mDevice{};
        eastl::string mDebugName{};

        ResourceType mResourceType{ ResourceType::eInvalid };
        eastl::atomic<ResourceStates> mResourceState{ ResourceStates::eUnknown };

        // By default, the resource is device local
        // lives in memory "only accessible by device"
        HeapType mHeapType{ HeapType::eDeviceLocal };
        CpuAccessType mCpuAccess{ CpuAccessType::eNone };
    };

    struct BufferCreateDescription {
        eastl::string mName{};
        bool mKeepInitializerResources{ false };

        BufferSpanHandle mSpanHandle{};

        // Hint to specify the count of elements
        // Used by the backend to compute alignment
        // If specified both must be different to 0
        // this says the buffer is typed and needs to hold mElementCount
        // elements of size mElementSize. If it is going to be treated as raw bytes
        // then mElementSize holds the total size in bytes
        size_t mElementCount{};
        size_t mElementSize{};

        // For Vulkan and D3D12 when we need to manage
        // frequently updating uniform/constant buffers
        size_t mMaxVersions{ 0 };
        bool mIsVolatile{};

        bool mTrackState{ true };
        ResourceStates mInitialState{ ResourceStates::eUnknown };

        Format mFormat{ Format::eUnknown };
        HeapType mHeapType{ HeapType::eDeviceLocal };
        CpuAccessType mCpuAccess{ CpuAccessType::eNone };
        ResourceType mResourceType{ ResourceType::eConstantBuffer };

        BufferDataType mDataType{ BufferDataType::eInvalid };
        BufferUsageFlags mUsageFlags{ BufferUsageFlagsBits::kNone };

        auto SetName( eastl::string_view name ) -> BufferCreateDescription&;
        auto ForElement( size_t byteSize, size_t count ) -> BufferCreateDescription&;
        auto SetByteSize( size_t byteSize ) -> BufferCreateDescription&;
        auto SetFormat(Format format ) -> BufferCreateDescription&;
        auto SetInitialData( BufferSpanHandle data ) -> BufferCreateDescription&;
        auto SetBufferUsage( BufferUsageFlags usage ) -> BufferCreateDescription&;
        auto SetBufferDataType( BufferDataType type ) -> BufferCreateDescription&;
        auto SetHeapType( HeapType type ) -> BufferCreateDescription&;
        auto SetCpuAccessType( CpuAccessType type ) -> BufferCreateDescription&;
        auto SetResourceType( ResourceType type ) -> BufferCreateDescription&;

        auto SetKeepInitializerResources( bool value ) -> BufferCreateDescription&;

        auto SetIsVolatile( bool value ) -> BufferCreateDescription&;
        auto SetMaxVersions( u32 count ) -> BufferCreateDescription&;

        constexpr auto SetInitialState(ResourceStates value) -> BufferCreateDescription& { mInitialState = value; return *this; }
        constexpr auto EnableAutomaticStateTracking(ResourceStates initialState) -> BufferCreateDescription& {
            mInitialState = initialState;
            mTrackState = true;
            return *this;
        }
    };

    // GPU to GPU sync
    class ISemaphore : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using SemaphoreHandle = Ref<ISemaphore>;

    // CPU to GPU sync
    class IFence : public DeviceObject {
    public:

        MKT_NODISCARD virtual auto GetCompletionValue() const -> u64 = 0;

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using FenceHandle = Ref<IFence>;

    class IShaderModule : public DeviceObject {
    public:
        MKT_NODISCARD auto GetType() const -> ShaderType { return mStage; }

        MKT_NODISCARD virtual auto GetContents() const -> const void* { return nullptr; }
        MKT_NODISCARD virtual auto GetContentsByteSize() const -> size_t { return 0; }

        virtual auto DumpShaderCode() -> void {}

        ~IShaderModule() override = default;

    protected:
        explicit IShaderModule( ShaderType stage, eastl::string_view entryPoint )
            : mEntryPoint{ entryPoint }, mStage{ stage } {}

    protected:
        eastl::string mEntryPoint{};
        ShaderType mStage{ ShaderType::eInvalid };
    };

    using ShaderModuleHandle = Ref<IShaderModule>;

    // RHI enforces to do any copies via commands
    class IBuffer : public DeviceObject {
    public:

        MKT_NODISCARD auto GetUsage() const -> BufferUsageFlags { return mUsage; }
        MKT_NODISCARD auto GetDataType() const -> BufferDataType { return mDataType; }

        MKT_NODISCARD auto GetData() const -> BufferSpanHandle { return mSpan; }
        MKT_NODISCARD auto GetSizeBytes() const -> size_t { return mElementCount == 0 ? mElementSize : mElementCount * mElementSize; }

        MKT_NODISCARD auto GetFormat() const -> Format { return mFormat; }

        // FIXME: does not produce expected results
        MKT_NODISCARD auto GetCount() const -> size_t {
            return mElementCount == 0 ? InferElementCount(mFormat, mElementSize) : mElementCount * mElementSize;
        }

        using DeviceObject::Initialize;

    protected:
        explicit IBuffer( const BufferCreateDescription& desc )
            : DeviceObject{ desc.mHeapType, desc.mResourceType },
              mSpan{ desc.mSpanHandle },
              mElementCount{ desc.mElementCount },
              mElementSize{ desc.mElementSize },
              mDataType{ desc.mDataType },
              mUsage{ desc.mUsageFlags },
              mIsVolatile{ desc.mIsVolatile },
              mMaxVersions{ desc.mMaxVersions },
              mFormat{ desc.mFormat }
        {}

    protected:
        BufferSpanHandle mSpan{};

        // These 2 attributes specify that the buffer is typed
        // meaning it will hold mElementCount elements of mElementSize size in bytes
        // This is useful for the backend API to manage alignment as it considers necessary
        // If it is going to be treated as raw bytes
        // then mElementSize holds the total size in bytes
        size_t mElementCount{};
        size_t mElementSize{};

        BufferDataType mDataType{ BufferDataType::eInvalid };
        BufferUsageFlags mUsage{ BufferUsageFlagsBits::kNone };

        bool mIsVolatile{};
        size_t mMaxVersions{ 0 };

        Format mFormat{ Format::eUnknown };
    };

    using BufferHandle = Ref<IBuffer>;

    struct VertexBindingDescription {
        u32 mBinding{};
        u32 mStride{};
        InputRate mRate{}; // ePerVertex / ePerInstance

        auto SetBinding(u32 binding) -> VertexBindingDescription&;
        auto SetStride(u32 stride) -> VertexBindingDescription&;
        auto SetInputRate(InputRate rate) -> VertexBindingDescription&;
    };

    struct VertexAttributeDescription {
        eastl::string mName{};
        u32 mLocation{};
        u32 mBinding{}; // which buffer
        Format mFormat{};
        u32 mOffset{};

        auto SetName(eastl::string name) -> VertexAttributeDescription&;
        auto SetLocation(uint32_t loc) -> VertexAttributeDescription&;
        auto SetBinding(uint32_t binding) -> VertexAttributeDescription&;
        auto SetFormat(rhi::Format format) -> VertexAttributeDescription&;
        auto SetOffset(uint32_t offset) -> VertexAttributeDescription&;
    };

    class IInputLayout : public DeviceObject {
    public:
        MKT_NODISCARD virtual auto GetNumAttributes() const -> u32 = 0;
        MKT_NODISCARD virtual auto GetAttributeDescription(u32 index) const -> const VertexAttributeDescription& = 0;

        using DeviceObject::Initialize;
    };

    using InputLayoutHandle = Ref<IInputLayout>;

    struct TextureCreateDescription {
        eastl::string mName{};

        u32 mWidth{};
        u32 mHeight{};
        u32 mMipCount{ 1 };

        bool mKeepInitializerResources{ false };

        // Only one of these can be used, this is provided so we can
        // initialize a GPU texture from an Image or a buffer of raw bytes from CPU side
        asset::ImageHandle mImageHandle{};

        // Can init texture from a buffer as well, example usages are noise texture
        // from SSAO
        BufferSpanHandle mBufferSpan{};

        bool mTrackState{ true };
        ResourceStates mInitialState{ ResourceStates::eUnknown };

        HeapType mHeapType{ HeapType::eDeviceLocal };

        Multisampling mMSAA{ Multisampling::eMsaaX1 };

        Format mFormat{ Format::eRGBA8_SNORM };
        TextureDimension mDimension{ TextureDimension::eTexture2D };

        TextureUsageFlags mUsage{ TextureUsageFlagsBits::kNone };

        ResourceType mResourceType{ ResourceType::eInvalid };

        TextureSubresourceSet mSubresourceSet{};

        auto SetName( eastl::string_view name ) -> TextureCreateDescription&;
        auto SetWidth( u32 width ) -> TextureCreateDescription&;
        auto SetHeight( u32 height ) -> TextureCreateDescription&;
        auto SetMipCount( u32 count ) -> TextureCreateDescription&;

        auto SetKeepInitializerResources( bool value ) -> TextureCreateDescription&;

        auto SetImageData( asset::ImageHandle image) -> TextureCreateDescription&;
        auto SetBufferData( BufferSpanHandle buffer) -> TextureCreateDescription&;

        auto SetHeapType( HeapType heapType) -> TextureCreateDescription&;

        auto SetMultisampling( Multisampling sampleCount ) -> TextureCreateDescription&;

        auto SetFormat( Format usage ) -> TextureCreateDescription&;
        auto SetDimensions( TextureDimension dimensions ) -> TextureCreateDescription&;

        auto SetUsage( TextureUsageFlags usage ) -> TextureCreateDescription&;

        auto SetResourceType( ResourceType usage ) -> TextureCreateDescription&;

        auto SetSubResources( const TextureSubresourceSet& subResources ) -> TextureCreateDescription&;

        constexpr auto SetInitialState(ResourceStates value) -> TextureCreateDescription& { mInitialState = value; return *this; }
        constexpr auto EnableAutomaticStateTracking(ResourceStates initialState) -> TextureCreateDescription& {
            mInitialState = initialState;
            mTrackState = true;
            return *this;
        }
    };

    class ITexture : public DeviceObject {
    public:
        MKT_NODISCARD auto GetFormat() const -> Format {
            return mFormat;
        }

        MKT_NODISCARD auto GetWidth() const -> u32 {
            return mWidth;
        }

        MKT_NODISCARD auto GetHeight() const -> u32 {
            return mHeight;
        }

        MKT_NODISCARD auto GetTextureUsage() const -> TextureUsageFlags {
            return mTextureUsage;
        }

        MKT_NODISCARD auto GetImage() const -> asset::ImageHandle { return mImageData; }
        MKT_NODISCARD auto GetSizeBytes() const -> size_t { return mImageData->mBufferSpan->GetSize(); }

        MKT_NODISCARD auto GetDimension() const -> TextureDimension { return mDimension; }
        MKT_NODISCARD auto GetSampleCount() const -> Multisampling { return mMultisampling; }
        MKT_NODISCARD auto GetMipLevelCount() const -> u32 { return mMipCount; }

        ~ITexture() override = default;

    protected:
        explicit ITexture( const TextureCreateDescription& desc )
            : DeviceObject{ desc.mHeapType, desc.mResourceType },
              mWidth{ desc.mWidth },
              mHeight{ desc.mHeight },
              mMipCount{ desc.mMipCount },
              mImageData{ desc.mImageHandle },
              mBufferSpan{ desc.mBufferSpan },
              mFormat{ desc.mFormat },
              mDimension{ desc.mDimension },
              mTextureUsage{ desc.mUsage },
              mMultisampling{ desc.mMSAA }, mSubResources{ desc.mSubresourceSet } {}

    protected:
        u32 mWidth{};
        u32 mHeight{};
        u32 mMipCount{ 1 };

        asset::ImageHandle mImageData{};
        BufferSpanHandle mBufferSpan{};

        Format mFormat{ Format::eUnknown };
        TextureDimension mDimension{ TextureDimension::eInvalid };

        TextureUsageFlags mTextureUsage{ TextureUsageFlagsBits::kShaderResource };

        Multisampling mMultisampling{ Multisampling::eMsaaX1 };

        TextureSubresourceSet mSubResources{};
    };

    using TextureHandle = Ref<ITexture>;

    struct FramebufferDescription {
        i32 mWidth{};
        i32 mHeight{};

        Format mColorFormat{ Format::eRGBA8_UNORM };
        Format mDepthFormat{ Format::eRGBA8_UNORM };

        eastl::vector<TextureHandle> mDepthAttachment{};
        eastl::vector<TextureHandle> mColorAttachments{};

        // Can optionally pass in spec info
        eastl::any mNativeHandleSpec{};

        auto WithSpecInfo(eastl::any nativeSpec) -> FramebufferDescription&;
        auto AddAttachment( TextureHandle color ) -> FramebufferDescription&;
        auto AddDepthAttachment( TextureHandle depth ) -> FramebufferDescription&;

        auto WithWidth( i32 width ) -> FramebufferDescription&;
        auto WithHeight( i32 height ) -> FramebufferDescription&;
    };

    class IFramebuffer : public DeviceObject  {
    public:
        MKT_NODISCARD auto GetWidth() const -> i32 { return mWidth; }
        MKT_NODISCARD auto GetHeight() const -> i32 { return mHeight; }

        MKT_NODISCARD auto GetColorAttachments() const -> const eastl::vector<TextureHandle>& { return mColorAttachments; }
        MKT_NODISCARD auto GetDepthAttachments() const -> const eastl::vector<TextureHandle>& { return mDepthAttachment; }

        MKT_NODISCARD auto HasDepthAttachment() const -> bool { return !mDepthAttachment.empty(); }
        MKT_NODISCARD auto HasColorAttachment() const -> bool { return !mColorAttachments.empty(); }

        MKT_NODISCARD auto GetNativeHandleSpec() const -> const eastl::any& { return mNativeHandleSpec; }

    protected:
        explicit IFramebuffer( const FramebufferDescription& desc)
            : mWidth{ desc.mWidth },
            mHeight{ desc.mHeight },
            mColorFormat{ desc.mColorFormat },
            mDepthFormat{ desc.mDepthFormat },
            mDepthAttachment{ eastl::move( desc.mDepthAttachment) },
            mColorAttachments{ eastl::move( desc.mColorAttachments ) }
        {}

    protected:
        i32 mWidth{};
        i32 mHeight{};

        Format mColorFormat{ Format::eRGBA8_UNORM };
        Format mDepthFormat{ Format::eRGBA8_UNORM };

        eastl::vector<TextureHandle> mDepthAttachment{};
        eastl::vector<TextureHandle> mColorAttachments{};

        // Can optionally pass in spec info
        eastl::any mNativeHandleSpec{};
    };

    using FramebufferHandle = Ref<IFramebuffer>;

    struct SamplerCreateDescription {
        float mMipLevels{ 1.0f };

        SamplerFilter mMinFilter{ SamplerFilter::eNearest };
        SamplerFilter mMagFilter{ SamplerFilter::eNearest };
        SamplerWrapMode mWrapU{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapV{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapW{ SamplerWrapMode::eRepeat };

        Color mBorderColor{ 0.0f, 0.0f, 0.0f, 1.0f };

        auto SetMipLevels( float mipLevels) -> SamplerCreateDescription&;

        auto SetBorderColor( const Color& color ) -> SamplerCreateDescription&;

        auto SetFilter( SamplerFilter filter ) -> SamplerCreateDescription&;
        auto SetMinFilter( SamplerFilter filter ) -> SamplerCreateDescription&;
        auto SetMagFilter( SamplerFilter filter ) -> SamplerCreateDescription&;

        auto SetWrap( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
        auto SetWrapU( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
        auto SetWrapV( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
        auto SetWrapW( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
    };

    class ISampler : public DeviceObject {
    public:

        MKT_NODISCARD auto GetMinFilter() const -> SamplerFilter { return mMinFilter; }
        MKT_NODISCARD auto GetMagFilter() const -> SamplerFilter { return mMagFilter; }

        MKT_NODISCARD auto GetWrapU() const -> SamplerWrapMode { return mWrapU; }
        MKT_NODISCARD auto GetWrapV() const -> SamplerWrapMode { return mWrapV; }
        MKT_NODISCARD auto GetWrapW() const -> SamplerWrapMode { return mWrapW; }

        MKT_NODISCARD auto GetMipLevels() const -> float { return mMipLevels; }

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;

        explicit ISampler( const SamplerCreateDescription& desc )
            : mMipLevels{ desc.mMipLevels},
            mMinFilter{ desc.mMinFilter},
            mMagFilter{ desc.mMagFilter},
            mWrapU{ desc.mWrapU},
            mWrapV{ desc.mWrapV},
            mWrapW{ desc.mWrapW},
            mBorderColor{ desc.mBorderColor }
        {}

    protected:
        float mMipLevels{ 1.0f };

        SamplerFilter mMinFilter{ SamplerFilter::eNearest };
        SamplerFilter mMagFilter{ SamplerFilter::eNearest };
        SamplerWrapMode mWrapU{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapV{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapW{ SamplerWrapMode::eRepeat };

        Color mBorderColor{};
    };

    using SamplerHandle = Ref<ISampler>;

    struct VertexBufferBinding {
        IBuffer* mBuffer{};
        u32 mSlot{};
        u64 mOffset{};
        u64 mElementStride{};

        auto operator==( const VertexBufferBinding& b ) const -> bool {
            return mBuffer == b.mBuffer && mSlot == b.mSlot && mOffset == b.mOffset;
        }
        auto operator!=( const VertexBufferBinding& b ) const -> bool { return !( *this == b ); }

        auto SetBuffer(IBuffer* value)-> VertexBufferBinding& { mBuffer = value; return *this; }
        auto SetBufferBinding(u32 value) -> VertexBufferBinding& { mSlot = value; return *this; }
        auto SetOffset(u64 value) -> VertexBufferBinding& { mOffset = value; return *this; }
        auto SetElementStride(u64 value) -> VertexBufferBinding& { mElementStride = value; return *this; }
    };

    struct IndexBufferBinding {
        IBuffer* mBuffer{};
        Format mFormat{};
        u32 mOffset{};

        auto operator==( const IndexBufferBinding& b ) const -> bool {
            return mBuffer == b.mBuffer && mFormat == b.mFormat && mOffset == b.mOffset;
        }
        auto operator!=( const IndexBufferBinding& b ) const -> bool { return !( *this == b ); }

        auto SetBuffer(IBuffer* value)-> IndexBufferBinding& { mBuffer = value; return *this; }
        auto SetFormat(Format value) -> IndexBufferBinding&{ mFormat = value; return *this; }
        auto SetOffset(u32 value)-> IndexBufferBinding& { mOffset = value; return *this; }
    };

    struct BindingLayoutItem {
        u32 mSlot{};

        Format mFormat{ Format::eUnknown };
        ResourceType mType{ ResourceType::eInvalid };
        TextureDimension mDimension{ TextureDimension::eInvalid };

        MKT_NODISCARD static auto Sampler( u32 slot ) -> BindingLayoutItem;
        MKT_NODISCARD static auto Texture_SRV( u32 slot ) -> BindingLayoutItem;
        MKT_NODISCARD static auto ConstantBuffer( u32 slot ) -> BindingLayoutItem;

        MKT_NODISCARD static auto StructuredBuffer_SRV(u32 slot) -> BindingLayoutItem;
        MKT_NODISCARD static auto StructuredBuffer_UAV(u32 slot) -> BindingLayoutItem;
    };

    struct BindingSetItem {
        IResource* mResource{};

        u32 mSlot{};
        BufferRange mRange{};

        ResourceType mType{ ResourceType::eInvalid };
        Format mFormat{ Format::eUnknown };
        TextureDimension mDimension{ TextureDimension::eInvalid };
        TextureSubresourceSet mSubResourceSet{};

        static auto None(u32 slot = 0) -> BindingSetItem;

        static auto ConstantBuffer(u32 slot, IBuffer* buffer, BufferRange range = EntireBuffer ) -> BindingSetItem;

        static auto Texture_SRV(u32 slot, ITexture* texture, Format format = Format::eUnknown,
            TextureSubresourceSet subResources = AllSubResources, TextureDimension dimension = TextureDimension::eInvalid) -> BindingSetItem;
        static auto Texture_UAV(u32 slot, ITexture* texture, Format format = Format::eUnknown,
            TextureSubresourceSet subResources = TextureSubresourceSet(0, 1, 0, TextureSubresourceSet::AllArraySlices),
            TextureDimension dimension = TextureDimension::eInvalid) -> BindingSetItem;

        static auto TypedBuffer_SRV(u32 slot, IBuffer* buffer, BufferRange range = EntireBuffer) -> BindingSetItem;
        static auto TypedBuffer_UAV(u32 slot, IBuffer* buffer, BufferRange range = EntireBuffer) -> BindingSetItem;

        static auto Sampler(u32 slot, ISampler* sampler) -> BindingSetItem;

        static auto StructuredBuffer_SRV(u32 slot, IBuffer* buffer, BufferRange range = EntireBuffer) -> BindingSetItem;
        static auto StructuredBuffer_UAV(u32 slot, IBuffer* buffer, BufferRange range = EntireBuffer) -> BindingSetItem;

        static auto RawBuffer_SRV(u32 slot, IBuffer* buffer, BufferRange range = EntireBuffer) -> BindingSetItem;
        static auto RawBuffer_UAV(u32 slot, IBuffer* buffer, BufferRange range = EntireBuffer) -> BindingSetItem;
    };

    struct BindingLayoutDescription {
        // For Vulkan this maps to descriptor sets
        u32 mRegisterSpace{};

        eastl::vector<BindingLayoutItem> mBindings{};
        ShaderStage mStageVisibility{ ShaderFlagsBits::kVertex };

        auto SetRegisterSpace( u32 group ) -> BindingLayoutDescription&;
        auto AddItem( const BindingLayoutItem& item ) -> BindingLayoutDescription&;
        auto SetShaderVisibility( ShaderStage visibility ) -> BindingLayoutDescription&;
    };

    struct BindlessLayoutItem {
        u32 mSlot{};
        u32 mMaxCapacity{};

        ResourceType mType{ ResourceType::eInvalid };

        // --- Samplers ---
        MKT_NODISCARD static auto Samplers(u32 slot, u32 maxCapacity) -> BindlessLayoutItem;

        // --- Textures ---
        MKT_NODISCARD static auto Texture_SRV(u32 slot, u32 maxCapacity ) -> BindlessLayoutItem;
        MKT_NODISCARD static auto Texture_UAV(u32 slot, u32 maxCapacity ) -> BindlessLayoutItem;

        // --- Constant Buffers ---
        MKT_NODISCARD static auto ConstantBuffer(u32 slot, u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto ConstantBuffer_UAV(u32 slot, u32 maxCapacity) -> BindlessLayoutItem;

        // --- Raw / ByteAddress Buffers ---
        MKT_NODISCARD static auto Buffer_SRV(u32 slot, u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto Buffer_UAV(u32 slot, u32 maxCapacity) -> BindlessLayoutItem;

        // --- Structured Buffers ---
        MKT_NODISCARD static auto StructuredBuffer_SRV(u32 slot, u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto StructuredBuffer_UAV(u32 slot, u32 maxCapacity) -> BindlessLayoutItem;

        // --- Acceleration Structures ---
        MKT_NODISCARD static auto AccelerationStructures(u32 slot, u32 maxCapacity) -> BindlessLayoutItem;
    };

    // For Graphics APIs that support natively bindless design
    struct BindlessLayoutDescription {
        eastl::string mName{};
        u32 mRegisterSpace{};
        ShaderStage mStageVisibility{ ShaderFlagsBits::kVertex };

        eastl::fixed_vector<BindlessLayoutItem, kMaxBindlessRegisterSpaces> mSlots{};

        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        auto SetDebugName( eastl::string_view name ) -> BindlessLayoutDescription&;
        auto SetRegisterSpace( u32 registerSpace ) -> BindlessLayoutDescription&;
        auto SetVisibility( ShaderStage visibility ) -> BindlessLayoutDescription&;
        auto AddBindlessItem( const BindlessLayoutItem& item ) -> BindlessLayoutDescription&;

        auto AddShader( ShaderModuleHandle shader ) -> BindlessLayoutDescription&;
    };

    struct BindingSetDescription {
        eastl::vector<BindingSetItem> mBindings{};

        // Backends offer support for shader reflection
        // which simplifies binding layout creation
        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        auto AddItem(const BindingSetItem& value) -> BindingSetDescription&;
        auto AddShader( ShaderModuleHandle shader ) -> BindingSetDescription&;
    };

    struct InputLayoutCreateDescription {
        ShaderModuleHandle mShaderModule{};
        eastl::fixed_vector<VertexBindingDescription, kMaxVertexBindings> mVertexBindingDescriptions{};
        eastl::fixed_vector<VertexAttributeDescription, kMaxVertexAttributes> mVertexAttributeDescriptions{};

        // Backends offer support for shader reflection
        // which simplifies binding layout creation
        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        auto SetShader( ShaderModuleHandle shader ) -> InputLayoutCreateDescription&;

        auto SetBindings( eastl::span<const VertexBindingDescription> items ) -> InputLayoutCreateDescription&;
        auto SetAttributes( eastl::span<const VertexAttributeDescription> items ) -> InputLayoutCreateDescription&;

        auto PushBinding( const VertexBindingDescription& desc ) -> InputLayoutCreateDescription&;
        auto PushAttribute( const VertexAttributeDescription& desc ) -> InputLayoutCreateDescription&;
    };

    class IBindingLayout : public DeviceObject {
    public:

        MKT_NODISCARD virtual auto GetRegisterSpace() const -> u32 = 0;
        MKT_NODISCARD virtual auto IsBindless() const -> bool = 0;

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using BindingLayoutHandle = Ref<IBindingLayout>;

    class IPipelineLayout : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using PipelineLayoutHandle = Ref<IPipelineLayout>;

    // Upon creation, its contents cannot mutate
    class IBindingSet : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using BindingSetHandle = Ref<IBindingSet>;

    // A resizable BindingSet
    class IDescriptorTable : public IBindingSet {
    public:
        // How many indices it holds for instance on Vulkan when we
        // say the descriptor set size for bindless descriptor indexing
        MKT_NODISCARD virtual auto GetCapacity( u32 slot ) const -> u32 = 0;
    };

    using DescriptorTableHandle = Ref<IDescriptorTable>;

    struct AccelStructureCreateDescription {

    };

    class AccelStructure : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    };

    using AccelStructureHandle = Ref<AccelStructure>;

    struct ShaderModuleCreateDescription {
        filesystem::FileHandle mFile{};
        ShaderType mType{ ShaderType::eInvalid };

        eastl::string mEntryPoint{ "main" };

        // We default to SLANG shader compiler.
        bool mIsSlangShader{ true };

        auto SetFile( filesystem::FileHandle file ) -> ShaderModuleCreateDescription&;
        auto SetStage( ShaderType stage ) -> ShaderModuleCreateDescription&;
        auto SetEntryPoint( eastl::string_view name ) -> ShaderModuleCreateDescription&;

        auto SetIsSlang( bool value ) -> ShaderModuleCreateDescription&;
    };

    struct PipelineLayoutCreateDescription {
        ShaderStage mPushConstantsVisibility{ ShaderFlagsBits::kAll };
        eastl::fixed_vector<BindingLayoutHandle, kMaxBindingLayouts> mBindingLayouts;

        auto SetPushConstantsVisibility( ShaderStage stage ) -> PipelineLayoutCreateDescription&;
        auto AddBindingLayout( BindingLayoutHandle layout ) -> PipelineLayoutCreateDescription&;
    };

    class IPipeline : public DeviceObject {
    public:
        ~IPipeline() override = default;

        MKT_NODISCARD auto GetPipelineType() const -> PipelineType;

        MKT_NODISCARD virtual auto GetPipelineLayout() const -> PipelineLayoutHandle = 0;

    protected:
        explicit IPipeline(const PipelineType pipelineType);

    protected:
        const PipelineType mPipelineType{ PipelineType::eInvalid };
    };

    using PipelineHandle = Ref<IPipeline>;

    struct ComputePipelineDescription {
        ShaderModuleHandle mStage{};
        PipelineLayoutHandle mPipelineLayout{};

        bool mUseReflection{ false };

        auto SetUseReflection( bool value ) -> ComputePipelineDescription&;
        auto SetComputeStage( ShaderModuleHandle handle ) -> ComputePipelineDescription&;
        auto SetPipelineLayout( PipelineLayoutHandle handle ) -> ComputePipelineDescription&;
    };

    struct GraphicsPipelineDescription {
        bool mEnableDepthTest{ true };
        bool mEnableDepthWrite{ true };
        bool mEnableStencilTest{ false };

        bool mEnableAlphaBlending{ true };
        bool mEnableSampleRateShading{ false };

        bool mUseReflection{ false };

        f32 mPolygonLineWidth{ 1.0f };

        Multisampling mMultisampling{ Multisampling::eMsaaX1 };
        CullMode mCullMode{ CullMode::eNone };
        PolygonMode mPolygonMode{ PolygonMode::eFill };
        PrimitiveTopology mPrimitiveTopology{ PrimitiveTopology::eTriangleList };
        WindingOrder mWindingOrder{ WindingOrder::eCounterClockwise };
        DepthCompareOp mDepthCompareOp{ DepthCompareOp::eLessOrEqual };

        // Resources
        InputLayoutHandle mInputLayout{};
        PipelineLayoutHandle mPipelineLayout{};

        // Shaders
        eastl::fixed_hash_map<ShaderType, ShaderModuleHandle, kMaxShaders> mShaders{};

        Format mDepthFormat{};
        eastl::fixed_vector<Format, kMaxColorFormats> mColorFormats{};

        auto SetPipelineLayout( PipelineLayoutHandle handle ) -> GraphicsPipelineDescription&;
        auto SetInputLayout( InputLayoutHandle handle ) -> GraphicsPipelineDescription&;

        auto SetPolygonMode( PolygonMode mode ) -> GraphicsPipelineDescription&;
        auto SetTopology( PrimitiveTopology topology ) -> GraphicsPipelineDescription&;

        auto SetUseReflection( bool value ) -> GraphicsPipelineDescription&;

        auto AddShader( ShaderModuleHandle handle ) -> GraphicsPipelineDescription&;

        auto SetCullMode( CullMode mode ) -> GraphicsPipelineDescription&;

        auto SetBlendEnable( bool value ) -> GraphicsPipelineDescription&;

        auto SetDepthTest( bool value ) -> GraphicsPipelineDescription&;
        auto SetDepthWrite( bool value ) -> GraphicsPipelineDescription&;
        auto SetDepthFormat( Format format ) -> GraphicsPipelineDescription&;

        auto AddColorFormat( Format format ) -> GraphicsPipelineDescription&;
        auto SetWindingOrder( WindingOrder order ) -> GraphicsPipelineDescription&;
    };

    // Pipelines are immutable objects, this is because Vulkan
    // and DirectX12 do not behave like DirecX11 where you set the state on
    // the fly. That is why there are no setters except fot certain features
    // that we know for sure all backends support
    class IGraphicsPipeline : public IPipeline {
    public:
        explicit IGraphicsPipeline(const GraphicsPipelineDescription& desc);

        MKT_NODISCARD auto GetDescription() const noexcept -> const GraphicsPipelineDescription&;

        MKT_NODISCARD auto GetPipelineLayout() const -> PipelineLayoutHandle override;

    protected:
        GraphicsPipelineDescription mDesc{};
    };

    class IComputePipeline : public IPipeline {
    public:
        explicit IComputePipeline(const ComputePipelineDescription& desc);

        MKT_NODISCARD auto GetDescription() const noexcept -> const ComputePipelineDescription&;

        MKT_NODISCARD auto GetPipelineLayout() const -> PipelineLayoutHandle override;

    protected:
        ComputePipelineDescription mDesc{};
    };

    struct GraphicsState {
        struct RenderTargetState {
            Color mClearColor{ kColorWhite };
            LoadOp mLoadOp{ LoadOp::eLoad };
            TextureHandle mRenderTarget{};
            TextureSubresourceSet mSubresourceSet{};
        };

        eastl::string mName{};

        Rect mRenderArea{};
        RenderTargetState mDepthTarget{};
        eastl::fixed_vector<RenderTargetState, kMaxRenderTargets> mCurrentRenderTargets{};

        auto SetScopeName( eastl::string_view name ) -> GraphicsState&;
        auto SetRenderArea( const Rect& rec ) -> GraphicsState&;
        auto AddDepthTarget(TextureHandle target, LoadOp op = LoadOp::eClear ) -> GraphicsState&;
        auto AddRenderTarget(TextureHandle target, const Color& c, LoadOp op = LoadOp::eClear, TextureSubresourceSet set = AllSubResources) -> GraphicsState&;
    };

    struct BindResourcesDescription {
        size_t mPushConstantSize{ 0 };
        ShaderStage mPushConstantVisibility{};
        eastl::fixed_vector<byte_t, kMaxPushConstantSize> mPushConstants{};

        static constexpr u32 kMaxResourceSets{32};

        // key = binding index (set index)
        eastl::fixed_hash_map<u32, IBindingSet*, kMaxResourceSets> mResourceSets{};

        IPipelineLayout* mPipelineLayout{};
        PipelineType     mBindPoint{};

        auto SetPipelineLayout(IPipelineLayout* layout) -> BindResourcesDescription&;
        auto SetBindPoint(PipelineType bindPoint) -> BindResourcesDescription&;

        auto SetPushConstants( const void* ptr, size_t sizeBytes, ShaderStage stage ) -> BindResourcesDescription&;

        auto AddResourceSet(u32 bindingIndex, IBindingSet* set) -> BindResourcesDescription&;
    };

    struct BufferBarrierDescription {
        IBuffer*           mBuffer       { nullptr };
        u64                mOffset       { 0 };
        u64                mSize         { 0xFFFFFFFFFFFFFFFFULL };

        // Previous State
        ResourceStates     mStateBefore  { ResourceStates::eUnknown };
        PipelineStageFlags mStageBefore  { PipelineStageFlagsBits::kNone };
        AccessFlags        mAccessBefore { AccessFlagsBits::kNone };

        // New State
        ResourceStates     mStateAfter   { ResourceStates::eUnknown };
        PipelineStageFlags mStageAfter   { PipelineStageFlagsBits::kNone };
        AccessFlags        mAccessAfter  { AccessFlagsBits::kNone };
    };

    struct TextureBarrierDescription {
        ITexture*          mTexture      { nullptr };

        TextureSubresourceSet   mSubresourceSet{ AllSubResources };

        // Previous State
        ResourceStates     mStateBefore  { ResourceStates::eUnknown };
        TextureLayout      mLayoutBefore { TextureLayoutBits::kUnknown };
        PipelineStageFlags mStageBefore  { PipelineStageFlagsBits::kNone };
        AccessFlags        mAccessBefore { AccessFlagsBits::kNone };

        // New State
        ResourceStates     mStateAfter   { ResourceStates::eUnknown };
        TextureLayout      mLayoutAfter  { TextureLayoutBits::kUnknown };
        PipelineStageFlags mStageAfter   { PipelineStageFlagsBits::kNone };
        AccessFlags        mAccessAfter  { AccessFlagsBits::kNone };
    };

    struct CommandListBeginDescription {
        eastl::string mScopeName{};
    };

    // When creating the command buffer we will specify parameters
    // methods that can be called from multiple thread will be specified
    class ICommandList : public DeviceObject {
    public:
        explicit ICommandList( QueueType queueType )
            : mQueueType{ queueType } {}

        virtual auto Begin( const CommandListBeginDescription& desc ) -> void = 0;
        virtual auto End() -> void = 0;

        virtual auto BeginParallel() -> void = 0;
        virtual auto EndParallel() -> void = 0;

        // More relaxed versions of SetResourceState (PushBarrier needs
        // CommitBarrier() called, SetBarrier() automatically flushes the barrier)
        virtual auto PushBarrier( const BufferBarrierDescription& barrier ) -> void = 0;
        virtual auto PushBarrier( const TextureBarrierDescription& barrier ) -> void = 0;

        virtual auto SetBarrier( const BufferBarrierDescription& barrier ) -> void = 0;
        virtual auto SetBarrier( const TextureBarrierDescription& barrier ) -> void = 0;

        virtual auto SetResourceState(IBuffer* buffer, ResourceStates stateBits) -> void = 0;
        virtual auto SetResourceState(ITexture* buffer, ResourceStates stateBits) -> void = 0;

        virtual auto BeginTrackingState(IBuffer* buffer, ResourceStates stateBits) -> void = 0;
        virtual auto BeginTrackingState(ITexture* buffer, ResourceStates stateBits) -> void = 0;

        virtual auto CommitBarriers() -> void = 0;

        virtual auto SetEnableAutomaticBarriers(  bool enable  ) -> void = 0;

        virtual auto SetClearColor( FramebufferHandle frameBuffer, Color color ) -> void = 0;
        virtual auto SetClearColor( TextureHandle renderTargets, Color color ) -> void = 0;

        virtual auto Write( IBuffer* src, ITexture* dest, u32 mipLevel ) -> void = 0;
        virtual auto Write( ITexture* target, u32 mipLevel, const void* data, size_t byteSize ) -> void = 0;
        virtual auto Copy( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void = 0;

        virtual auto WriteVolatile( IBuffer* target, size_t dstOffset, const void* data, size_t byteSize ) -> void = 0;

        virtual auto Write( IBuffer* target, const void* data, size_t byteSize ) -> void = 0;
        virtual auto Write( IBuffer* target, size_t destOffset, const void* data, size_t byteSize ) -> void = 0;

        virtual auto Copy( IBuffer* src, IBuffer* dest ) -> void = 0;
        virtual auto Copy( IBuffer* src, IBuffer* dest, size_t destOffset ) -> void = 0;

        virtual auto Copy( IBuffer* dest, ITexture* src ) -> void = 0;

        virtual auto BeginRendering( GraphicsState& state ) -> void = 0;
        virtual auto EndRendering() -> void = 0;

        virtual auto BindPipeline( IPipeline* pipeline ) -> void = 0;

        // I am not sure if I wanna have these because the viewport is set on the images we
        // render to so it makes more sense to tie them to the graphics state when we specify the render targets
        virtual auto SetViewportState( const ViewportState& vs ) -> void = 0;
        virtual auto SetViewport( eastl::span<const Viewport> viewports ) -> void = 0;
        virtual auto SetScissors( eastl::span<const Rect> scissorRects ) -> void = 0;

        virtual auto BindIndexBuffer( IBuffer* buffer ) -> void = 0;
        virtual auto BindIndirectBuffer( IBuffer* buffer ) -> void = 0;
        virtual auto BindVertexBuffer( const VertexBufferBinding& binding ) -> void = 0;
        virtual auto BindVertexBuffer( eastl::span<const VertexBufferBinding> binding ) -> void = 0;

        virtual auto BindPipelineResources( const BindResourcesDescription& desc ) -> void = 0;

        virtual auto Draw( const DrawArguments& args ) -> void = 0;
        virtual auto DrawIndexed( const DrawArguments& args ) -> void = 0;

        // Use previously bound indirect buffer
        virtual auto DrawIndirect( u32 offset, u32 drawCount ) -> void = 0;
        virtual auto DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void = 0;

        virtual auto Dispatch( u32 groupsX, u32 groupsY, u32 groupsZ ) -> void = 0;

        virtual auto SetPushConstants( IPipelineLayout* pipelineLayout, const void* data, size_t byteSize, ShaderStage stageVisibility ) -> void = 0;

        MKT_NODISCARD auto GetQueueType() const -> QueueType { return mQueueType; }

        ~ICommandList() override = default;

        using DeviceObject::Initialize;

    protected:
        QueueType mQueueType{ QueueType::eInvalid };
    };

    using CommandListHandle = Ref<ICommandList>;

    class IQueue : public DeviceObject {
    public:
        MKT_NODISCARD auto GetType() const -> QueueType;
        MKT_NODISCARD auto GetOpSupportFlags() const -> QueueOpSupportFlags;

        virtual auto Wait( IFence* fence, u64 value ) -> void = 0;
        virtual auto Signal( IFence* fence, u64 value ) -> void = 0;

        virtual auto ExecuteCommandLists( eastl::span<CommandListHandle> commands ) -> void = 0;

        ~IQueue() override = default;

        using DeviceObject::Initialize;

    protected:
        explicit IQueue( QueueType type, QueueOpSupportFlags flags )
            : mType{ type }, mOpSupportFlags{ flags } {}

    protected:
        QueueType mType{ QueueType::eInvalid };
        QueueOpSupportFlags mOpSupportFlags{ QueueOpSupportFlagsBits::kGraphics };
    };

    using QueueHandle = Ref<IQueue>;

    class ISwapchain : public ReferenceCounted {
    public:

        ~ISwapchain() override = default;
    };

    using SwapchainHandle = Ref<ISwapchain>;
}// namespace mikoto::renderer::rhi

#endif// MIKOTO_RHI_HH
