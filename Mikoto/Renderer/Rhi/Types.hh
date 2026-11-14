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

#ifndef MIKOTO_RENDERER_RHI_TYPES_HH
#define MIKOTO_RENDERER_RHI_TYPES_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Flag.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

namespace mikoto::renderer::rhi {

    using MipLevel = core::u32;
    using ArraySlice = core::u32;

    using DescriptorTableIndex = core::u64;

    inline constexpr DescriptorTableIndex kInvalidBindingItemIndex{ ~0ULL };

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
        eHLSL,
        eDXIL,
        eSlang,
    };

    enum class BlendFactor : core::u8 {
        eZero,
        eOne,

        eSrcColor,
        eOneMinusSrcColor,
        eDstColor,
        eOneMinusDstColor,

        eSrcAlpha,
        eOneMinusSrcAlpha,
        eDstAlpha,
        eOneMinusDstAlpha,

        eSrcAlphaSaturate,
    };

    enum class BlendOperation : core::u8 {
        eAdd,
        eSubtract,
        eReverseSubtract,
        eMin,
        eMax,
    };

    enum class ColorWriteMask : core::u8 {
        eNone  = 0x00,
        eRed   = 0x01,
        eGreen = 0x02,
        eBlue  = 0x04,
        eAlpha = 0x08,

        eRGB   = 0x07,
        eRGBA  = 0x0F,
    };

    enum class ViewportConvention {
        eNone,
        eLeftHanded_OriginTopLeft,
        eLeftHanded_OriginBottomLeft,
        eRightHanded_OriginTopLeft,
        eRightHanded_OriginBottomLeft,
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
        D3D12_CommandList,

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
        GL_Buffer,
        GL_Texture,
        GL_Sampler,
    };

    enum class LoadOp {
        eLoad,
        eClear,
        eDontCare,
    };

    enum class SamplerMipmapMode {
        eNearest,
        eLinear
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

    enum class CompareOp {
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
        eOpenGL,
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
        eRayTracing,
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

    enum class ColorSpace : core::i32 {
        eSrgbNonlinear,
        eDisplayP3Nonlinear,
        eExtendedSrgbLinear,
        eDisplayP3Linear,
        eDciP3Nonlinear,
        eBt709Linear,
        eBt709Nonlinear,
        eBt2020Linear,
        eHdr10St2084,
        eHdr10Hlg,
        eAdobeRgbLinear,
        eAdobeRgbNonlinear,
        ePassThrough,
        eExtendedSrgbNonlinear,
        eDisplayNativeAmd
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

        eNV12,

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

    enum class AccessType {
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
        eVideoDecodeOutput,
        eVideoDecodeReference,
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

    /**
     * Bit storage for optional device capabilities.
     */
    struct GpuFeatureSupportProperties {
        using Data = core::u64;
    };

    using GpuFeatureSupportFlags = core::Flags<GpuFeatureSupportProperties>;

    struct GpuFeatureSupportFlagsBits {
        // Device with bare minimum support
        static constexpr GpuFeatureSupportFlags None{ 0 };

        static constexpr GpuFeatureSupportFlags AnisotropicFiltering{ BIT_SET(0) };
        static constexpr GpuFeatureSupportFlags HardwareWireframe{ BIT_SET(1) };
        static constexpr GpuFeatureSupportFlags WindowPresentation{ BIT_SET(2) };
        static constexpr GpuFeatureSupportFlags HardwareRayTracing{ BIT_SET(3) };
        static constexpr GpuFeatureSupportFlags MeshShaders{ BIT_SET(4) };
        static constexpr GpuFeatureSupportFlags BCTextureCompression{ BIT_SET(5) };
        static constexpr GpuFeatureSupportFlags BindlessDescriptorTables{ BIT_SET(6) };
        static constexpr GpuFeatureSupportFlags VideoEncoding{ BIT_SET(7) };
        static constexpr GpuFeatureSupportFlags VideoDecoding{ BIT_SET(8) };
    };

    enum class GpuDeviceType : core::i32 {
        eInvalid = -1,
        eDiscrete,
        eIntegrated,
        eSoftwareRasterizer
    };

    struct Extent2D {
        core::u32 mWidth{};
        core::u32 mHeight{};
    };

    struct Extent3D {
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mDepth{};
    };

    // Buffer usage flags
    struct BufferUsageFlagsProperties {
        using Data = core::u32;
    };

    using BufferUsageFlags = core::Flags<BufferUsageFlagsProperties>;

    struct BufferUsageFlagsBits {
        static constexpr BufferUsageFlags None{ 0 };
        static constexpr BufferUsageFlags Vertex{ BIT_SET(0) };
        static constexpr BufferUsageFlags Index{ BIT_SET(1) };
        static constexpr BufferUsageFlags Constant{ BIT_SET(2) };
        static constexpr BufferUsageFlags Structured{ BIT_SET(3) };
        static constexpr BufferUsageFlags IndirectDraw{ BIT_SET(5) };
        static constexpr BufferUsageFlags CopySource{ BIT_SET(6) };
        static constexpr BufferUsageFlags CopyDest{ BIT_SET(7) };
        static constexpr BufferUsageFlags AccelStructureBuildInput{ BIT_SET(8) };
        static constexpr BufferUsageFlags AccelStructureStorage{ BIT_SET(9) };
        static constexpr BufferUsageFlags AccelStructureScratch{ BIT_SET(10) };
        static constexpr BufferUsageFlags ShaderBindingTable{ BIT_SET(11) };
        static constexpr BufferUsageFlags VideoDecodeSource{ BIT_SET(12) };
    };

    // Texture usage flags
    struct TextureFlagsProperties {
        using Data = core::u32;
    };

    using TextureUsageFlags = core::Flags<TextureFlagsProperties>;

    struct TextureUsageFlagsBits {
        static constexpr TextureUsageFlags None{ 0 };

        static constexpr TextureUsageFlags RenderTarget{ BIT_SET( 0 ) };
        static constexpr TextureUsageFlags DepthTarget{ BIT_SET( 1 ) };
        static constexpr TextureUsageFlags StencilTarget{ BIT_SET( 2 ) };
        static constexpr TextureUsageFlags DepthStencilTarget{ BIT_SET( 3 ) };
        static constexpr TextureUsageFlags ShaderResource{ BIT_SET( 4 ) };
        static constexpr TextureUsageFlags CopySource{ BIT_SET( 5 ) };
        static constexpr TextureUsageFlags CopyDest{ BIT_SET( 6 ) };
        static constexpr TextureUsageFlags UnorderedAccess{ BIT_SET( 7 ) };
    };

    // Shader usage flags
    struct ShaderFlagsProperties {
        using Data = core::u32;
    };

    using ShaderFlags = core::Flags<ShaderFlagsProperties>;

    struct ShaderFlagsBits {
        static constexpr ShaderFlags Vertex{ BIT_SET( 0 ) };
        static constexpr ShaderFlags Pixel{ BIT_SET( 1 ) };
        static constexpr ShaderFlags Compute{ BIT_SET( 2 ) };

        static constexpr ShaderFlags Geometry{ BIT_SET( 3 ) };

        static constexpr ShaderFlags Hull{ BIT_SET( 4 ) };
        static constexpr ShaderFlags Domain{ BIT_SET( 5 ) };

        static constexpr ShaderFlags RayGeneration{ BIT_SET( 6 ) };
        static constexpr ShaderFlags Intersection{ BIT_SET( 7 ) };
        static constexpr ShaderFlags AnyHit{ BIT_SET( 8 ) };
        static constexpr ShaderFlags ClosestHit{ BIT_SET( 9 ) };
        static constexpr ShaderFlags Miss{ BIT_SET( 10 ) };

        static constexpr ShaderFlags All{ ~0U };
    };

    // Queue support flags
    struct QueueOpSupportFlagsProperties {
        using Data = core::u32;
    };

    using QueueOpSupportFlags = core::Flags<QueueOpSupportFlagsProperties>;

    struct QueueOpSupportFlagsBits {
        static constexpr QueueOpSupportFlags Graphics{ BIT_SET( 0 ) };
        static constexpr QueueOpSupportFlags Transfer{ BIT_SET( 1 ) };
        static constexpr QueueOpSupportFlags Compute{ BIT_SET( 2 ) };
        static constexpr QueueOpSupportFlags Presentation{ BIT_SET( 3 ) };
        static constexpr QueueOpSupportFlags VideoEncode{ BIT_SET( 4 ) };
        static constexpr QueueOpSupportFlags VideoDecode{ BIT_SET( 5 ) };
    };

    // Pipeline stage flags
    struct PipelineStageFlagsProperties {
        using Data = core::u64;
    };
    using PipelineStageFlags = core::Flags<PipelineStageFlagsProperties>;

    struct PipelineStageFlagsBits {
        static constexpr PipelineStageFlags None{ 0 };

        static constexpr PipelineStageFlags Top{ BIT_SET( 0 ) };
        static constexpr PipelineStageFlags DrawIndirect{ BIT_SET( 1 ) };
        static constexpr PipelineStageFlags VertexInput{ BIT_SET( 2 ) };
        static constexpr PipelineStageFlags VertexShader{ BIT_SET( 3 ) };
        static constexpr PipelineStageFlags HullShader{ BIT_SET( 4 ) };
        static constexpr PipelineStageFlags DomainShader{ BIT_SET( 5 ) };
        static constexpr PipelineStageFlags GeometryShader{ BIT_SET( 6 ) };
        static constexpr PipelineStageFlags PixelShader{ BIT_SET( 7 ) };

        static constexpr PipelineStageFlags EarlyFragmentTests{ BIT_SET( 8 ) };
        static constexpr PipelineStageFlags LateFragmentTests{ BIT_SET( 9 ) };
        static constexpr PipelineStageFlags RenderTarget{ BIT_SET( 10 ) };

        static constexpr PipelineStageFlags ComputeShader{ BIT_SET( 11 ) };
        static constexpr PipelineStageFlags Transfer{ BIT_SET( 12 ) };

        static constexpr PipelineStageFlags Bottom{ BIT_SET( 13 ) };
        static constexpr PipelineStageFlags Host{ BIT_SET( 14 ) };

        static constexpr PipelineStageFlags AllGraphics{ BIT_SET( 15 ) };
        static constexpr PipelineStageFlags AllCommands{ BIT_SET( 16 ) };

        static constexpr PipelineStageFlags Copy{ BIT_SET( 32 ) };
        static constexpr PipelineStageFlags Resolve{ BIT_SET( 33 ) };
        static constexpr PipelineStageFlags Blit{ BIT_SET( 34 ) };
        static constexpr PipelineStageFlags Clear{ BIT_SET( 35 ) };
        static constexpr PipelineStageFlags IndexInput{ BIT_SET( 36 ) };

        static constexpr PipelineStageFlags TaskShader{ BIT_SET( 19 ) };
        static constexpr PipelineStageFlags MeshShader{ BIT_SET( 20 ) };

        static constexpr PipelineStageFlags AccelerationStructureBuild{ BIT_SET( 25 ) };
        static constexpr PipelineStageFlags RayTracingShader{ BIT_SET( 21 ) };
        static constexpr PipelineStageFlags VideoDecode{ BIT_SET( 26 ) };

        static constexpr PipelineStageFlags All{ ~0ULL };
    };

    /** Explicit, combinable memory accesses used by enhanced barriers. */
    struct BarrierAccessProperties {
        using Data = core::u64;
    };

    using BarrierAccessFlags = core::Flags<BarrierAccessProperties>;

    struct BarrierAccessFlagsBits {
        static constexpr BarrierAccessFlags None{ 0 };
        static constexpr BarrierAccessFlags IndirectCommandRead{ 1ULL << 0 };
        static constexpr BarrierAccessFlags IndexRead{ 1ULL << 1 };
        static constexpr BarrierAccessFlags VertexAttributeRead{ 1ULL << 2 };
        static constexpr BarrierAccessFlags ConstantBuffer{ 1ULL << 3 };
        static constexpr BarrierAccessFlags ShaderSampledRead{ 1ULL << 4 };
        static constexpr BarrierAccessFlags ShaderStorageRead{ 1ULL << 5 };
        static constexpr BarrierAccessFlags ShaderStorageWrite{ 1ULL << 6 };
        static constexpr BarrierAccessFlags ColorAttachmentRead{ 1ULL << 7 };
        static constexpr BarrierAccessFlags ColorAttachmentWrite{ 1ULL << 8 };
        static constexpr BarrierAccessFlags DepthStencilRead{ 1ULL << 9 };
        static constexpr BarrierAccessFlags DepthStencilWrite{ 1ULL << 10 };
        static constexpr BarrierAccessFlags TransferRead{ 1ULL << 11 };
        static constexpr BarrierAccessFlags TransferWrite{ 1ULL << 12 };
        static constexpr BarrierAccessFlags ResolveRead{ 1ULL << 13 };
        static constexpr BarrierAccessFlags ResolveWrite{ 1ULL << 14 };
        static constexpr BarrierAccessFlags HostRead{ 1ULL << 15 };
        static constexpr BarrierAccessFlags HostWrite{ 1ULL << 16 };
        static constexpr BarrierAccessFlags MemoryRead{ 1ULL << 17 };
        static constexpr BarrierAccessFlags MemoryWrite{ 1ULL << 18 };
        static constexpr BarrierAccessFlags AccelerationStructureRead{ 1ULL << 19 };
        static constexpr BarrierAccessFlags AccelerationStructureWrite{ 1ULL << 20 };
        static constexpr BarrierAccessFlags AccelerationStructureBuildInputRead{ 1ULL << 21 };
        static constexpr BarrierAccessFlags StructuredBufferRead{ 1ULL << 22 };
        static constexpr BarrierAccessFlags VideoDecodeRead{ 1ULL << 23 };
        static constexpr BarrierAccessFlags VideoDecodeWrite{ 1ULL << 24 };

        static constexpr BarrierAccessFlags Writes{ ShaderStorageWrite | ColorAttachmentWrite |
            DepthStencilWrite | TransferWrite | ResolveWrite | HostWrite | MemoryWrite | AccelerationStructureWrite | VideoDecodeWrite };
    };

    // Texture layout flags
    struct TextureLayoutProperties {
        using Data = core::u32;
    };
    using TextureLayoutFlags = core::Flags<TextureLayoutProperties>;

    struct TextureLayoutBits {
        static constexpr TextureLayoutFlags Unknown{ 0 };
        static constexpr TextureLayoutFlags General{ BIT_SET(1) };
        static constexpr TextureLayoutFlags RenderTarget{ BIT_SET(2) };
        static constexpr TextureLayoutFlags DepthStencil{ BIT_SET(3) };
        static constexpr TextureLayoutFlags ShaderResource{ BIT_SET(4) };
        static constexpr TextureLayoutFlags UnorderedAccess{ BIT_SET(5) }; // For RW textures
        static constexpr TextureLayoutFlags CopySource{ BIT_SET(6) };
        static constexpr TextureLayoutFlags CopyDest{ BIT_SET(7) };
        static constexpr TextureLayoutFlags Present{ BIT_SET(8) };
        static constexpr TextureLayoutFlags ResolveSource{ BIT_SET(9) };
        static constexpr TextureLayoutFlags ResolveDest{ BIT_SET(10) };
        static constexpr TextureLayoutFlags DepthStencilReadOnly{ BIT_SET(11) };
        static constexpr TextureLayoutFlags VideoDecodeOutput{ BIT_SET(12) };
        static constexpr TextureLayoutFlags VideoDecodeReference{ BIT_SET(13) };
    };

    /**
     * Static properties of an RHI pixel or buffer format.
     */
    struct FormatInfo {
        Format mFormat{ Format::eUnknown };
        const char* mName{};
        core::u8 mBytesPerBlock{};
        core::u8 mBlockSize{};
        FormatKind mKind{};
        bool mHasRed{};
        bool mHasGreen{};
        bool mHasBlue{};
        bool mHasAlpha{};
        bool mHasDepth{};
        bool mHasStencil{};
        bool mIsSigned{};
        bool mIsSRGB{};
    };

    /**
     * Byte range within a buffer; @ref kEntireBuffer denotes the complete resource.
     */
    struct BufferRange {
        core::u64 mByteOffset{ 0 };
        core::u64 mByteSize{ 0 };

        BufferRange() = default;

        BufferRange( core::u64 byteOffset, core::u64 byteSize );

        /**
         * Returns the value produced by IsEntireBuffer.
         *
         * @param bufferByteSize Input value used by this operation.
         * @returns The result of IsEntireBuffer.
         */
        MKT_NODISCARD auto IsEntireBuffer( core::usize bufferByteSize ) const -> bool;

        /**
         * Performs the operation represented by operator.
         *
         * @param other Input value used by this operation.
         * @returns The result of operator.
         */
        MKT_NODISCARD auto operator==(const BufferRange& other) const -> bool;

        /**
         * Sets the value handled by SetByteOffset.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetByteOffset.
         */
        auto SetByteOffset( core::u64 value ) -> BufferRange&;

        /**
         * Sets the value handled by SetByteSize.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetByteSize.
         */
        auto SetByteSize( core::u64 value ) -> BufferRange&;

        /**
         * Performs the operation represented by Validate.
         *
         * @param bufferByteSize Input value used by this operation.
         * @returns The result of Validate.
         */
        auto Validate( core::usize bufferByteSize ) -> BufferRange&;
    };

    /**
     * Mip and array-slice subset used to create a texture view.
     */
    struct TextureSubresourceSet {
        static constexpr MipLevel kAllMipLevels{ MipLevel(-1) };
        static constexpr ArraySlice kAllArraySlices{ ArraySlice(-1) };

        MipLevel mBaseMipLevel{ 0 };
        MipLevel mNumMipLevels{ 1 };
        ArraySlice mBaseArraySlice{ 0 };
        ArraySlice mNumArraySlices{ 1 };

        TextureSubresourceSet() = default;

        TextureSubresourceSet(MipLevel baseMipLevel, MipLevel numMipLevels, ArraySlice baseArraySlice, ArraySlice numArraySlices);
    };

    /**
     * Four-component floating-point color.
     */
    struct Color {
        core::f32 mR{};
        core::f32 mG{};
        core::f32 mB{};
        core::f32 mA{};

        Color() = default;

        Color( core::f32 c );

        Color( const core::float4& c );

        Color( core::f32 r, core::f32 g, core::f32 b, core::f32 a );

        /**
         * Performs the operation represented by operator.
         *
         * @param other Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator==( const Color& other ) const -> bool;

        /**
         * Performs the operation represented by operator.
         *
         * @param other Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator!=( const Color& other ) const -> bool;

        /**
         * Divides each color component by a scalar.
         *
         * @param color Color to scale.
         * @param value Divisor.
         * @returns The scaled color.
         */
        friend auto operator/( const Color& color, float value ) -> Color {
            MKT_BEGIN_PROFILER();
            return { color.mR / value, color.mG / value, color.mB / value, color.mA / value };
        }

        /**
         * Adds a scalar to each color component.
         *
         * @param color Color to offset.
         * @param value Offset value.
         * @returns The offset color.
         */
        friend auto operator+( const Color& color, float value ) -> Color {
            MKT_BEGIN_PROFILER();
            return { color.mR + value, color.mG + value, color.mB + value, color.mA + value };
        }

        /**
         * Adds a scalar to each color component.
         *
         * @param value Offset value.
         * @param color Color to offset.
         * @returns The offset color.
         */
        friend auto operator+( float value, const Color& color ) -> Color {
            MKT_BEGIN_PROFILER();
            return { color.mR + value, color.mG + value, color.mB + value, color.mA + value };
        }

        /**
         * Divides a scalar by each color component.
         *
         * @param value Dividend.
         * @param color Per-component divisor.
         * @returns The component-wise quotient.
         */
        friend auto operator/( float value, const Color& color ) -> Color {
            MKT_BEGIN_PROFILER();
            return { color.mR / value, color.mG / value, color.mB / value, color.mA / value };
        }

        /**
         * Divides color components pairwise.
         *
         * @param lhs Dividend color.
         * @param rhs Divisor color.
         * @returns The component-wise quotient.
         */
        friend auto operator/( const Color& lhs, const Color& rhs ) -> Color {
            MKT_BEGIN_PROFILER();
            return { lhs.mR / rhs.mR, lhs.mG / rhs.mG, lhs.mB / rhs.mB, lhs.mA / rhs.mA };
        }

        operator core::float4() const;
    };

    /**
     * Rectangular viewport and depth range.
     */
    struct Viewport {
        core::f32 mMinX{};
        core::f32 mMaxX{};
        core::f32 mMinY{};
        core::f32 mMaxY{};
        core::f32 mMinZ{ 0.f };
        core::f32 mMaxZ{ 1.f };

        bool mFlip{ false };

        Viewport() = default;

        Viewport( core::f32 width, core::f32 height );

        Viewport( core::f32 minX, core::f32 maxX, core::f32 minY, core::f32 maxY, core::f32 minZ, core::f32 maxZ );

        /**
         * Performs the operation represented by operator.
         *
         * @param other Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator==( const Viewport& other ) const -> bool;

        /**
         * Performs the operation represented by operator.
         *
         * @param other Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator!=( const Viewport& other ) const -> bool;

        /**
         * Returns the value produced by GetWidth.
         *
         * @returns The result of GetWidth.
         */
        MKT_NODISCARD auto GetWidth() const -> float;

        /**
         * Returns the value produced by GetHeight.
         *
         * @returns The result of GetHeight.
         */
        MKT_NODISCARD auto GetHeight() const -> float;
    };

    /**
     * Integer render area, normally used as a scissor rectangle.
     */
    struct Rect {
        core::i32 mMinX{};
        core::i32 mMaxX{};
        core::i32 mMinY{};
        core::i32 mMaxY{};

        Rect() = default;

        Rect( core::i32 width, core::i32 height );

        Rect( core::i32 minX, core::i32 maxX, core::i32 minY, core::i32 maxY );

        /**
         * Performs the operation represented by Rect.
         *
         * @returns The result of Rect.
         */
        explicit Rect( const Viewport& viewport );

        /**
         * Performs the operation represented by operator.
         *
         * @param other Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator==( const Rect& other ) const -> bool;

        /**
         * Performs the operation represented by operator.
         *
         * @param other Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator!=( const Rect& other ) const -> bool;

        /**
         * Performs the operation represented by ComputeWidth.
         *
         * @returns The result of ComputeWidth.
         */
        MKT_NODISCARD auto ComputeWidth() const -> core::i32;

        /**
         * Performs the operation represented by ComputeHeight.
         *
         * @returns The result of ComputeHeight.
         */
        MKT_NODISCARD auto ComputeHeight() const -> core::i32;
    };

    /**
     * Stride and update rate for a vertex-buffer binding.
     */
    struct VertexBindingDescription {
        core::u32 mBinding{};
        core::u32 mStride{};
        InputRate mRate{}; // ePerVertex / ePerInstance

        /**
         * Sets the value handled by SetBinding.
         *
         * @param binding Input value used by this operation.
         * @returns The result of SetBinding.
         */
        auto SetBinding(core::u32 binding) -> VertexBindingDescription&;

        /**
         * Sets the value handled by SetStride.
         *
         * @param stride Input value used by this operation.
         * @returns The result of SetStride.
         */
        auto SetStride(core::u32 stride) -> VertexBindingDescription&;

        /**
         * Sets the value handled by SetInputRate.
         *
         * @param rate Input value used by this operation.
         * @returns The result of SetInputRate.
         */
        auto SetInputRate(InputRate rate) -> VertexBindingDescription&;
    };

    /**
     * Location, format and byte offset of one vertex shader input.
     */
    struct VertexAttributeDescription {
        eastl::string mName{};
        core::u32 mLocation{};
        core::u32 mBinding{}; // which buffer
        Format mFormat{};
        core::u32 mOffset{};

        /**
         * Sets the value handled by SetName.
         *
         * @param name Input value used by this operation.
         * @returns The result of SetName.
         */
        auto SetName(eastl::string name) -> VertexAttributeDescription&;

        /**
         * Sets the value handled by SetLocation.
         *
         * @param loc Input value used by this operation.
         * @returns The result of SetLocation.
         */
        auto SetLocation(core::u32 loc) -> VertexAttributeDescription&;

        /**
         * Sets the value handled by SetBinding.
         *
         * @param binding Input value used by this operation.
         * @returns The result of SetBinding.
         */
        auto SetBinding(core::u32 binding) -> VertexAttributeDescription&;

        /**
         * Sets the value handled by SetFormat.
         *
         * @param format Input value used by this operation.
         * @returns The result of SetFormat.
         */
        auto SetFormat(rhi::Format format) -> VertexAttributeDescription&;

        /**
         * Sets the value handled by SetOffset.
         *
         * @param offset Input value used by this operation.
         * @returns The result of SetOffset.
         */
        auto SetOffset(core::u32 offset) -> VertexAttributeDescription&;
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

    constexpr inline core::f32 kDefaultPolygonLineWidth{ 1.0f };

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

#endif// MIKOTO_RENDERER_RHI_TYPES_HH
