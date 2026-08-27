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

#ifndef MIKOTO_RHI_TYPES_HH
#define MIKOTO_RHI_TYPES_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Flag.hh>
#include <Core/Types.hh>

namespace mikoto::renderer::rhi {

    using MipLevel = core::u32;
    using ArraySlice = core::u32;

    enum class GpuDeviceType {
        eInvalid = -1,
        eDiscrete,
        eIntegrated,
        eSoftwareRasterizer
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
        eVideoEncode,
        eVideoDecode,
        ePresent,
    };

    enum class ShaderLanguage {
        eInvalid,
        eSPIRV,
        eGLSL,
        eHLSL_5_0,
        eHLSL_5_1,
        eDXIL,
        eSlang,
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

    enum class RefreshRate {
        eSync,
        eUnlimited
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

    // Buffer usage flags
    struct BufferUsageFlagsProperties {
        using Data = core::u32;
    };

    using BufferUsageFlags = core::Flags<BufferUsageFlagsProperties>;

    struct BufferUsageFlagsBits {
        static constexpr BufferUsageFlags kNone{ 0 };
        static constexpr BufferUsageFlags kVertex{ BIT_SET(0) };
        static constexpr BufferUsageFlags kIndex{ BIT_SET(1) };
        static constexpr BufferUsageFlags kConstant{ BIT_SET(2) };
        static constexpr BufferUsageFlags kStorage{ BIT_SET(3) };
        static constexpr BufferUsageFlags kIndirectDraw{ BIT_SET(5) };
        static constexpr BufferUsageFlags kCopySrc{ BIT_SET(6) };
        static constexpr BufferUsageFlags kCopyDst{ BIT_SET(7) };
    };

    // Texture usage flags
    struct TextureFlagsProperties {
        using Data = core::u32;
    };

    using TextureUsageFlags = core::Flags<TextureFlagsProperties>;

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

    // Shader usage flags
    struct ShaderFlagsProperties {
        using Data = core::u32;
    };

    using ShaderFlags = core::Flags<ShaderFlagsProperties>;

    struct ShaderFlagsBits {
        static constexpr ShaderFlags kVertex{ BIT_SET( 0 ) };
        static constexpr ShaderFlags kPixel{ BIT_SET( 1 ) };
        static constexpr ShaderFlags kCompute{ BIT_SET( 2 ) };

        static constexpr ShaderFlags kGeometry{ BIT_SET( 3 ) };

        static constexpr ShaderFlags kHull{ BIT_SET( 4 ) };
        static constexpr ShaderFlags kDomain{ BIT_SET( 5 ) };

        static constexpr ShaderFlags kRayGeneration{ BIT_SET( 6 ) };
        static constexpr ShaderFlags kIntersection{ BIT_SET( 7 ) };
        static constexpr ShaderFlags kAnyHit{ BIT_SET( 8 ) };
        static constexpr ShaderFlags kClosestHit{ BIT_SET( 9 ) };
        static constexpr ShaderFlags kMiss{ BIT_SET( 10 ) };

        static constexpr ShaderFlags kAll{ ~0U };
    };

    // Queue support flags
    struct QueueOpSupportFlagsProperties {
        using Data = core::u32;
    };

    using QueueOpSupportFlags = core::Flags<QueueOpSupportFlagsProperties>;

    struct QueueOpSupportFlagsBits {
        static constexpr QueueOpSupportFlags kGraphics{ BIT_SET( 0 ) };
        static constexpr QueueOpSupportFlags kTransfer{ BIT_SET( 1 ) };
        static constexpr QueueOpSupportFlags kCompute{ BIT_SET( 2 ) };
        static constexpr QueueOpSupportFlags kPresentation{ BIT_SET( 3 ) };
    };

    // Pipeline stage flags
    struct PipelineStageFlagsProperties {
        using Data = core::u64;
    };
    using PipelineStageFlags = core::Flags<PipelineStageFlagsProperties>;

    struct PipelineStageFlagsBits {
        static constexpr PipelineStageFlags kNone{ BIT_SET( 0 ) };

        static constexpr PipelineStageFlags kTop{ BIT_SET( 0 ) };
        static constexpr PipelineStageFlags kDrawIndirect{ BIT_SET( 1 ) };
        static constexpr PipelineStageFlags kVertexInput{ BIT_SET( 2 ) };
        static constexpr PipelineStageFlags kVertexShader{ BIT_SET( 3 ) };
        static constexpr PipelineStageFlags kHullShader{ BIT_SET( 4 ) };
        static constexpr PipelineStageFlags kDomainShader{ BIT_SET( 5 ) };
        static constexpr PipelineStageFlags kGeometryShader{ BIT_SET( 6 ) };
        static constexpr PipelineStageFlags kPixelShader{ BIT_SET( 7 ) };

        static constexpr PipelineStageFlags kEarlyFragmentTests{ BIT_SET( 8 ) };
        static constexpr PipelineStageFlags kLateFragmentTests{ BIT_SET( 9 ) };
        static constexpr PipelineStageFlags kColorAttachment{ BIT_SET( 10 ) };

        static constexpr PipelineStageFlags kComputeShader{ BIT_SET( 11 ) };
        static constexpr PipelineStageFlags kTransfer{ BIT_SET( 12 ) };

        static constexpr PipelineStageFlags kBottom{ BIT_SET( 13 ) };
        static constexpr PipelineStageFlags kHost{ BIT_SET( 14 ) };

        static constexpr PipelineStageFlags kAllGraphics{ BIT_SET( 15 ) };
        static constexpr PipelineStageFlags kAllCommands{ BIT_SET( 16 ) };

        static constexpr PipelineStageFlags kCopy{ BIT_SET( 32 ) };
        static constexpr PipelineStageFlags kResolve{ BIT_SET( 33 ) };
        static constexpr PipelineStageFlags kBlit{ BIT_SET( 34 ) };
        static constexpr PipelineStageFlags kClear{ BIT_SET( 35 ) };
        static constexpr PipelineStageFlags kIndexInput{ BIT_SET( 36 ) };

        static constexpr PipelineStageFlags kPreRasterizationShaders{ BIT_SET( 38 ) };

        static constexpr PipelineStageFlags kTaskShader{ BIT_SET( 19 ) };
        static constexpr PipelineStageFlags kMeshShader{ BIT_SET( 20 ) };

        static constexpr PipelineStageFlags kAccelerationStructureBuild{ BIT_SET( 25 ) };
        static constexpr PipelineStageFlags kRayTracingShader{ BIT_SET( 21 ) };

        static constexpr PipelineStageFlags kAll{ 0xFFFFFFFF };
    };

    // Resource access flags
    struct AccessFlagsProperties {
        using Data = core::u32;
    };
    using AccessFlags = core::Flags<AccessFlagsProperties>;

    struct AccessFlagsBits {
        static constexpr AccessFlags kNone{ 0 };
        static constexpr AccessFlags kIndirectRead{ BIT_SET(0) };
        static constexpr AccessFlags kIndexRead{ BIT_SET(1) };
        static constexpr AccessFlags kVertexRead{ BIT_SET(2) };
        static constexpr AccessFlags kConstantRead{ BIT_SET(3) };
        static constexpr AccessFlags kShaderRead{ BIT_SET(4) };
        static constexpr AccessFlags kShaderWrite{ BIT_SET(5) };
        static constexpr AccessFlags kRenderTarget{ BIT_SET(6) };
        static constexpr AccessFlags kDepthStencilRead{ BIT_SET(7) };
        static constexpr AccessFlags kDepthStencilWrite{ BIT_SET(8) };
        static constexpr AccessFlags kCopyRead{ BIT_SET(9) };
        static constexpr AccessFlags kCopyWrite{ BIT_SET(10) };
    };

    // Texture layout flags
    struct TextureLayoutProperties {
        using Data = core::u32;
    };
    using TextureLayoutFlags = core::Flags<TextureLayoutProperties>;

    struct TextureLayoutBits {
        static constexpr TextureLayoutFlags kUnknown{ 0 };
        static constexpr TextureLayoutFlags kGeneral{ BIT_SET(1) };
        static constexpr TextureLayoutFlags kColorAttachment{ BIT_SET(2) };
        static constexpr TextureLayoutFlags kDepthStencilWrite{ BIT_SET(3) };
        static constexpr TextureLayoutFlags kDepthStencilRead{ BIT_SET(4) };
        static constexpr TextureLayoutFlags kShaderResource{ BIT_SET(5) };
        static constexpr TextureLayoutFlags kUnorderedAccess{ BIT_SET(6) }; // For RW textures
        static constexpr TextureLayoutFlags kCopySrc{ BIT_SET(7) };
        static constexpr TextureLayoutFlags kCopyDst{ BIT_SET(8) };
        static constexpr TextureLayoutFlags kPresent{ BIT_SET(9) };
    };

    // Resource format info
    struct FormatInfo {
        Format mFormat{ Format::eUnknown };
        const char* mName{};
        core::u8 mBytesPerBlock{};
        core::u8 mBlockSize{};
        FormatKind mKind{};
        bool mHasRed : 1 {};
        bool mHasGreen : 1 {};
        bool mHasBlue : 1 {};
        bool mHasAlpha : 1 {};
        bool mHasDepth : 1 {};
        bool mHasStencil : 1 {};
        bool mIsSigned : 1 {};
        bool mIsSRGB : 1 {};
    };

    // Describes a piece of a buffer
    struct BufferRange {
        core::u64 mByteOffset = 0;
        core::u64 mByteSize = 0;

        BufferRange() = default;

        BufferRange( core::u64 byteOffset, core::u64 byteSize )
            : mByteOffset{ byteOffset }, mByteSize{ byteSize } {}

        MKT_NODISCARD auto IsEntireBuffer( core::size_t bufferByteSize ) const -> bool;
        MKT_NODISCARD auto operator==(const BufferRange& other) const -> bool;

        auto SetByteOffset( core::u64 value ) -> BufferRange&;
        auto SetByteSize( core::u64 value ) -> BufferRange&;

        auto Validate( core::size_t bufferByteSize ) -> BufferRange&;
    };

    // Similar to buffer range but for textures
    // Used normally by backends to create views to shader resources
    struct TextureSubresourceSet {
        static constexpr MipLevel kAllMipLevels{ MipLevel(-1) };
        static constexpr ArraySlice kAllArraySlices{ ArraySlice(-1) };

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

    // Represents a 4-Component color
    struct Color {
        core::f32 mR{};
        core::f32 mG{};
        core::f32 mB{};
        core::f32 mA{};

        Color() = default;

        Color( core::f32 c )
            : mR{ c }, mG{ c }, mB{ c }, mA{ c } {}

        Color( const core::float4& c )
            : mR{ c.r }, mG{ c.g }, mB{ c.b }, mA{ c.a } {}

        Color( core::f32 r, core::f32 g, core::f32 b, core::f32 a )
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

    // Represents a viewport
    struct Viewport {
        core::f32 mMinX{};
        core::f32 mMaxX{};
        core::f32 mMinY{};
        core::f32 mMaxY{};
        core::f32 mMinZ{ 0.f };
        core::f32 mMaxZ{ 1.f };

        bool mFlip{ false };

        Viewport() = default;

        Viewport( core::f32 width, core::f32 height )
            : mMinX{ 0.f }, mMaxX{ width }, mMinY{ 0.f }, mMaxY{ height }, mMinZ{ 0.f }, mMaxZ{ 1.f } {}

        Viewport( core::f32 minX, core::f32 maxX, core::f32 minY, core::f32 maxY, core::f32 minZ, core::f32 maxZ )
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

    // Represents a render area
    // Used normally to specify scissor rects
    struct Rect {
        core::i32 mMinX{};
        core::i32 mMaxX{};
        core::i32 mMinY{};
        core::i32 mMaxY{};

        Rect() = default;

        Rect( core::i32 width, core::i32 height )
            : mMinX{ 0 }, mMaxX{ width }, mMinY{ 0 }, mMaxY{ height } {}

        Rect( core::i32 minX, core::i32 maxX, core::i32 minY, core::i32 maxY )
            : mMinX{ minX }, mMaxX{ maxX }, mMinY{ minY }, mMaxY{ maxY } {}

        explicit Rect( const Viewport& viewport )
            : mMinX{ core::as<core::i32>( glm::floor( viewport.mMinX ) ) },
              mMaxX{ core::as<core::i32>( glm::ceil( viewport.mMaxX ) ) },
              mMinY{ core::as<core::i32>( glm::floor( viewport.mMinY ) ) },
              mMaxY{ core::as<core::i32>( glm::ceil( viewport.mMaxY ) ) } {}

        auto operator==( const Rect& other ) const -> bool {
            return mMinX == other.mMinX && mMinY == other.mMinY && mMaxX == other.mMaxX && mMaxY == other.mMaxY;
        }

        auto operator!=( const Rect& other ) const -> bool {
            return !( *this == other );
        }

        MKT_NODISCARD auto ComputeWidth() const -> core::i32 {
            return mMaxX - mMinX;
        }

        MKT_NODISCARD auto ComputeHeight() const -> core::i32 {
            return mMaxY - mMinY;
        }
    };

    struct VertexBindingDescription {
        core::u32 mBinding{};
        core::u32 mStride{};
        InputRate mRate{}; // ePerVertex / ePerInstance

        auto SetBinding(core::u32 binding) -> VertexBindingDescription&;
        auto SetStride(core::u32 stride) -> VertexBindingDescription&;
        auto SetInputRate(InputRate rate) -> VertexBindingDescription&;
    };

    struct VertexAttributeDescription {
        eastl::string mName{};
        core::u32 mLocation{};
        core::u32 mBinding{}; // which buffer
        Format mFormat{};
        core::u32 mOffset{};

        auto SetName(eastl::string name) -> VertexAttributeDescription&;
        auto SetLocation(uint32_t loc) -> VertexAttributeDescription&;
        auto SetBinding(uint32_t binding) -> VertexAttributeDescription&;
        auto SetFormat(rhi::Format format) -> VertexAttributeDescription&;
        auto SetOffset(uint32_t offset) -> VertexAttributeDescription&;
    };

    // Constants
    constexpr inline core::u32 kMaxViewports{ 16 };
    constexpr inline core::u32 kMaxScissors{ 10 };
    constexpr inline core::u32 kMaxShaders{ 15 };
    constexpr inline core::u32 kMaxSlotsPerTable{ 15 };
    constexpr inline core::u32 kMaxColorFormats{ 10 };
    constexpr inline core::u32 kMaxCubeFaces{ 6 };
    constexpr inline core::u32 kMaxRenderTargets{ 8 };
    constexpr inline core::u32 kMaxVertexAttributes{ 16 };
    constexpr inline core::u32 kMaxVertexBindings{ 16 };
    constexpr inline core::u32 kMaxVertexBuffers{ 8 };
    constexpr inline core::u32 kMaxBindingLayouts{ 8 };
    constexpr inline core::u32 kMaxBarriers{ 25 };
    constexpr inline core::u32 kMaxBindlessRegisterSpaces{ 16 };
    constexpr inline core::u32 kMaxPushConstantSize{ 128 };

    const inline Color kColorWhite{ 1.f };
    const inline Color kColorBlack{ 0.f };
    const inline Color kColorTransparent{ 0.f, 0.f, 0.f, 0.f };

    const inline Color kColorRed{ 1.f, 0.f, 0.f, 1.f };
    const inline Color kColorGreen{ 0.f, 1.f, 0.f, 1.f };
    const inline Color kColorBlue{ 0.f, 0.f, 1.f, 1.f };

    const inline Color kColorYellow{ 1.f, 1.f, 0.f, 1.f };
    const inline Color kColorCyan{ 0.f, 1.f, 1.f, 1.f };
    const inline Color kColorMagenta{ 1.f, 0.f, 1.f, 1.f };

    const inline Color kColorGray{ 0.5f, 0.5f, 0.5f, 1.f };
    const inline Color kColorLightGray{ 0.75f, 0.75f, 0.75f, 1.f };
    const inline Color kColorDarkGray{ 0.25f, 0.25f, 0.25f, 1.f };

    const inline Color kColorOrange{ 1.f, 0.5f, 0.f, 1.f };
    const inline Color kColorPink{ 1.f, 0.75f, 0.8f, 1.f };

    const inline BufferRange kEntireBuffer{ BufferRange(0, ~0ull) };
    const inline TextureSubresourceSet kAllSubResources{ TextureSubresourceSet(0, TextureSubresourceSet::kAllMipLevels, 0, TextureSubresourceSet::kAllArraySlices) };

}

#endif//MIKOTO_RHI_TYPES_HH
