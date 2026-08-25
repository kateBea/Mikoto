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

#include <volk.h>

#include <EASTL/string_view.h>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    auto GetGpuDeviceType( GpuDeviceType type ) -> VkPhysicalDeviceType {
        switch (type) {
            case GpuDeviceType::eDiscrete: return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            case GpuDeviceType::eIntegrated: return VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
            case GpuDeviceType::eSoftwareRasterizer: return VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
            default:;
        }

        return VK_PHYSICAL_DEVICE_TYPE_OTHER;
    }

    auto GetResultString( VkResult result ) -> const char* {
        switch ( result ) {
#define MKT_CASE( x ) \
    case x:           \
        return #x;
            MKT_CASE( VK_SUCCESS )
            MKT_CASE( VK_NOT_READY )
            MKT_CASE( VK_TIMEOUT )
            MKT_CASE( VK_EVENT_SET )
            MKT_CASE( VK_EVENT_RESET )
            MKT_CASE( VK_INCOMPLETE )
            MKT_CASE( VK_ERROR_OUT_OF_HOST_MEMORY )
            MKT_CASE( VK_ERROR_OUT_OF_DEVICE_MEMORY )
            MKT_CASE( VK_ERROR_INITIALIZATION_FAILED )
            MKT_CASE( VK_ERROR_DEVICE_LOST )
            MKT_CASE( VK_ERROR_MEMORY_MAP_FAILED )
            MKT_CASE( VK_ERROR_LAYER_NOT_PRESENT )
            MKT_CASE( VK_ERROR_EXTENSION_NOT_PRESENT )
            MKT_CASE( VK_ERROR_FEATURE_NOT_PRESENT )
            MKT_CASE( VK_ERROR_INCOMPATIBLE_DRIVER )
            MKT_CASE( VK_ERROR_TOO_MANY_OBJECTS )
            MKT_CASE( VK_ERROR_FORMAT_NOT_SUPPORTED )
            MKT_CASE( VK_ERROR_FRAGMENTED_POOL )
            MKT_CASE( VK_ERROR_UNKNOWN )
#undef MKT_CASE
            default:
                return "UNKNOWN_VK_RESULT";
        }
    }

    auto GetFormat( Format format ) -> VkFormat {
        switch ( format ) {
            case Format::eUnknown:
                return VK_FORMAT_UNDEFINED;

            // 8-bit UNORM
            case Format::eR8_UNORM:
                return VK_FORMAT_R8_UNORM;
            case Format::eRG8_UNORM:
                return VK_FORMAT_R8G8_UNORM;
            case Format::eRGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case Format::eBGRA8_UNORM:
                return VK_FORMAT_B8G8R8A8_UNORM;

            // 8-bit SNORM
            case Format::eR8_SNORM:
                return VK_FORMAT_R8_SNORM;
            case Format::eRG8_SNORM:
                return VK_FORMAT_R8G8_SNORM;
            case Format::eRGBA8_SNORM:
                return VK_FORMAT_R8G8B8A8_SNORM;

            // 8-bit UINT / SINT
            case Format::eR8_UINT:
                return VK_FORMAT_R8_UINT;
            case Format::eR8_SINT:
                return VK_FORMAT_R8_SINT;
            case Format::eRG8_UINT:
                return VK_FORMAT_R8G8_UINT;
            case Format::eRG8_SINT:
                return VK_FORMAT_R8G8_SINT;
            case Format::eRGBA8_UINT:
                return VK_FORMAT_R8G8B8A8_UINT;
            case Format::eRGBA8_SINT:
                return VK_FORMAT_R8G8B8A8_SINT;

            // 16-bit UNORM / SNORM
            case Format::eR16_UNORM:
                return VK_FORMAT_R16_UNORM;
            case Format::eRG16_UNORM:
                return VK_FORMAT_R16G16_UNORM;
            case Format::eRGBA16_UNORM:
                return VK_FORMAT_R16G16B16A16_UNORM;

            case Format::eR16_SNORM:
                return VK_FORMAT_R16_SNORM;
            case Format::eRG16_SNORM:
                return VK_FORMAT_R16G16_SNORM;
            case Format::eRGBA16_SNORM:
                return VK_FORMAT_R16G16B16A16_SNORM;

            // 16-bit UINT / SINT
            case Format::eR16_UINT:
                return VK_FORMAT_R16_UINT;
            case Format::eR16_SINT:
                return VK_FORMAT_R16_SINT;
            case Format::eRG16_UINT:
                return VK_FORMAT_R16G16_UINT;
            case Format::eRG16_SINT:
                return VK_FORMAT_R16G16_SINT;
            case Format::eRGBA16_UINT:
                return VK_FORMAT_R16G16B16A16_UINT;
            case Format::eRGBA16_SINT:
                return VK_FORMAT_R16G16B16A16_SINT;

            // 16-bit FLOAT
            case Format::eR16_FLOAT:
                return VK_FORMAT_R16_SFLOAT;
            case Format::eRG16_FLOAT:
                return VK_FORMAT_R16G16_SFLOAT;
            case Format::eRGBA16_FLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;

            // 32-bit FLOAT
            case Format::eR32_FLOAT:
                return VK_FORMAT_R32_SFLOAT;
            case Format::eRG32_FLOAT:
                return VK_FORMAT_R32G32_SFLOAT;
            case Format::eRGB32_FLOAT:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case Format::eRGBA32_FLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;

            // 32-bit UINT / SINT
            case Format::eR32_UINT:
                return VK_FORMAT_R32_UINT;
            case Format::eR32_SINT:
                return VK_FORMAT_R32_SINT;
            case Format::eRG32_UINT:
                return VK_FORMAT_R32G32_UINT;
            case Format::eRG32_SINT:
                return VK_FORMAT_R32G32_SINT;
            case Format::eRGB32_UINT:
                return VK_FORMAT_R32G32B32_UINT;
            case Format::eRGB32_SINT:
                return VK_FORMAT_R32G32B32_SINT;
            case Format::eRGBA32_UINT:
                return VK_FORMAT_R32G32B32A32_UINT;
            case Format::eRGBA32_SINT:
                return VK_FORMAT_R32G32B32A32_SINT;

            // Packed formats
            case Format::eR10G10B10A2_UNORM:
                return VK_FORMAT_A2B10G10R10_UNORM_PACK32;

            case Format::eR11G11B10_FLOAT:
                return VK_FORMAT_B10G11R11_UFLOAT_PACK32;

            // Depth formats
            case Format::eD16:
                return VK_FORMAT_D16_UNORM;
            case Format::eD24S8:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case Format::eD32:
                return VK_FORMAT_D32_SFLOAT;
            case Format::eD32S8:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;

            // BC compressed
            case Format::eBC1_UNORM:
                return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            case Format::eBC1_UNORM_SRGB:
                return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
            case Format::eBC2_UNORM:
                return VK_FORMAT_BC2_UNORM_BLOCK;
            case Format::eBC2_UNORM_SRGB:
                return VK_FORMAT_BC2_SRGB_BLOCK;
            case Format::eBC3_UNORM:
                return VK_FORMAT_BC3_UNORM_BLOCK;
            case Format::eBC3_UNORM_SRGB:
                return VK_FORMAT_BC3_SRGB_BLOCK;
            case Format::eBC4_UNORM:
                return VK_FORMAT_BC4_UNORM_BLOCK;
            case Format::eBC4_SNORM:
                return VK_FORMAT_BC4_SNORM_BLOCK;
            case Format::eBC5_UNORM:
                return VK_FORMAT_BC5_UNORM_BLOCK;
            case Format::eBC5_SNORM:
                return VK_FORMAT_BC5_SNORM_BLOCK;
            case Format::eBC6H_UFLOAT:
                return VK_FORMAT_BC6H_UFLOAT_BLOCK;
            case Format::eBC6H_SFLOAT:
                return VK_FORMAT_BC6H_SFLOAT_BLOCK;
            case Format::eBC7_UNORM:
                return VK_FORMAT_BC7_UNORM_BLOCK;
            case Format::eBC7_UNORM_SRGB:
                return VK_FORMAT_BC7_SRGB_BLOCK;

            case Format::eCount:
            default:
                return VK_FORMAT_UNDEFINED;
        }
    }

    auto GetAspectMask( VkFormat format ) -> VkImageAspectFlags {
        switch (format) {
                // Color formats
                case VK_FORMAT_R8_UNORM:
                case VK_FORMAT_R8G8B8A8_UNORM:
                case VK_FORMAT_B8G8R8A8_UNORM:
                case VK_FORMAT_R16G16B16A16_SFLOAT:
                case VK_FORMAT_R32G32B32A32_SFLOAT:
                    return VK_IMAGE_ASPECT_COLOR_BIT;

                    // Depth-only formats
                case VK_FORMAT_D16_UNORM:
                case VK_FORMAT_D32_SFLOAT:
                    return VK_IMAGE_ASPECT_DEPTH_BIT;

                    // Depth + Stencil formats
                case VK_FORMAT_D24_UNORM_S8_UINT:
                case VK_FORMAT_D32_SFLOAT_S8_UINT:
                    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

                    // Stencil-only formats (rare)
                case VK_FORMAT_S8_UINT:
                    return VK_IMAGE_ASPECT_STENCIL_BIT;

                default:
                    return VK_IMAGE_ASPECT_COLOR_BIT;
            }
    }

    auto GetStageMask(ResourceStates state) -> VkPipelineStageFlags2 {
        switch (state) {
            case ResourceStates::eRenderTarget:
                return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

            case ResourceStates::eDepthWrite:
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                       VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

            case ResourceStates::eDepthRead:
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                       VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

            case ResourceStates::eShaderResource:
                return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

            case ResourceStates::eUnorderedAccess:
                return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

            case ResourceStates::eIndirectArgument:
                return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;

            case ResourceStates::eCopySource:
            case ResourceStates::eCopyDest:
                return VK_PIPELINE_STAGE_2_COPY_BIT;

            // https://github.com/KhronosGroup/Vulkan-Docs/issues/2575
            // VK_PIPELINE_STAGE_2_TRANSFER_BIT is an alias for VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
            // for backwards compatibility
            case ResourceStates::eResolveSource:
            case ResourceStates::eResolveDest:
                return VK_PIPELINE_STAGE_2_RESOLVE_BIT;

            case ResourceStates::ePresent:
                return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

            default:
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }
    }

    auto GetAccessMask(ResourceStates state) -> VkAccessFlags2 {
        switch (state) {
            case ResourceStates::eRenderTarget:
                return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

            case ResourceStates::eDepthWrite:
                return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            case ResourceStates::eDepthRead:
                return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

            case ResourceStates::eShaderResource:
                return VK_ACCESS_2_SHADER_READ_BIT;

            case ResourceStates::eUnorderedAccess:
                return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

            case ResourceStates::eIndirectArgument:
                return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

            case ResourceStates::eCopySource:
            case ResourceStates::eResolveSource:
                return VK_ACCESS_2_TRANSFER_READ_BIT;

            case ResourceStates::eCopyDest:
            case ResourceStates::eResolveDest:
                return VK_ACCESS_2_TRANSFER_WRITE_BIT;

            default:
                return 0;
        }
    }

    auto GetIndexType( Format format ) -> VkIndexType {
        switch (format)
        {
            case Format::eR16_UINT:
                return VK_INDEX_TYPE_UINT16;

            case Format::eR32_UINT:
                return VK_INDEX_TYPE_UINT32;

#if defined(VK_EXT_index_type_uint8) || defined(VK_VERSION_1_3)
            case Format::eR8_UINT:
                return VK_INDEX_TYPE_UINT8_EXT;
#endif

            default:
                // You should never hit this if your API is correct
                MKT_ASSERT(false, "Invalid format for index buffer");
                return VK_INDEX_TYPE_UINT32;
        }
    }

    auto GetPolygonMode( PolygonMode mode ) -> VkPolygonMode {
        switch (mode) {
            case PolygonMode::eLines: return VK_POLYGON_MODE_LINE;
            case PolygonMode::ePoint: return VK_POLYGON_MODE_POINT;
            case PolygonMode::eFill: return VK_POLYGON_MODE_FILL;
            default:;
        }

        return VK_POLYGON_MODE_MAX_ENUM;
    }

    auto GetAspectMask( Format format ) -> VkImageAspectFlags {
        switch (format) {
            case Format::eD16:
            case Format::eD32:
                return VK_IMAGE_ASPECT_DEPTH_BIT;

            case Format::eD24S8:
            case Format::eD32S8:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

            case Format::eX24G8_UINT:
            case Format::eX32G8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;

            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    auto GetShaderModuleStage(ShaderType stage) -> VkShaderStageFlagBits {
        switch (stage) {
            case ShaderType::eVertex:
                return VK_SHADER_STAGE_VERTEX_BIT;

            case ShaderType::ePixel:
                return VK_SHADER_STAGE_FRAGMENT_BIT;

            case ShaderType::eCompute:
                return VK_SHADER_STAGE_COMPUTE_BIT;

            case ShaderType::eGeometry:
                return VK_SHADER_STAGE_GEOMETRY_BIT;

            case ShaderType::eDomain:
                return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

            case ShaderType::eHull:
                return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

            case ShaderType::eRayGeneration:
                return VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            case ShaderType::eMiss:
                return VK_SHADER_STAGE_MISS_BIT_KHR;

            case ShaderType::eClosestHit:
                return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            default:
                return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        }
    }
    auto GetShaderModuleStage(VkShaderStageFlagBits stage) -> ShaderType {
        switch (stage) {
            case VK_SHADER_STAGE_VERTEX_BIT:
                return ShaderType::eVertex;

            case VK_SHADER_STAGE_FRAGMENT_BIT:
                return ShaderType::ePixel;

            case VK_SHADER_STAGE_COMPUTE_BIT:
                return ShaderType::eCompute;

            case VK_SHADER_STAGE_GEOMETRY_BIT:
                return ShaderType::eGeometry;

            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                return ShaderType::eHull;

            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                return ShaderType::eDomain;

            case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
                return ShaderType::eRayGeneration;

            case VK_SHADER_STAGE_MISS_BIT_KHR:
                return ShaderType::eMiss;

            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                return ShaderType::eClosestHit;

            default:
                return ShaderType::eInvalid;
        }
    }

    auto GetCompareOp(DepthCompareOp op) -> VkCompareOp {
        switch (op) {
            case DepthCompareOp::eNever:
                return VK_COMPARE_OP_NEVER;

            case DepthCompareOp::eLess:
                return VK_COMPARE_OP_LESS;

            case DepthCompareOp::eEqual:
                return VK_COMPARE_OP_EQUAL;

            case DepthCompareOp::eLessOrEqual:
                return VK_COMPARE_OP_LESS_OR_EQUAL;

            case DepthCompareOp::eGreater:
                return VK_COMPARE_OP_GREATER;

            case DepthCompareOp::eNotEqual:
                return VK_COMPARE_OP_NOT_EQUAL;

            case DepthCompareOp::eGreaterOrEqual:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;

            case DepthCompareOp::eAlways:
                return VK_COMPARE_OP_ALWAYS;

            default:
                return VK_COMPARE_OP_ALWAYS;
        }
    }

    auto GetInputRate( InputRate rate ) -> VkVertexInputRate {
        switch (rate) {
            case InputRate::ePerVertex:   return VK_VERTEX_INPUT_RATE_VERTEX;
            case InputRate::ePerInstance: return VK_VERTEX_INPUT_RATE_INSTANCE;
            default:                      return VK_VERTEX_INPUT_RATE_VERTEX; // fallback
        }
    }

    auto GetCullMode(CullMode mode) -> VkCullModeFlags {
        switch (mode) {
            case CullMode::eNone: return VK_CULL_MODE_NONE;
            case CullMode::eCullFront: return VK_CULL_MODE_FRONT_BIT;
            case CullMode::eCullBack: return VK_CULL_MODE_FRONT_AND_BACK;
            default:;
        }

        return VK_CULL_MODE_NONE;
    }

    auto GetWindingOrder( WindingOrder order ) -> VkFrontFace {
        switch (order ) {
            case WindingOrder::eClockwise: return VK_FRONT_FACE_CLOCKWISE;
            case WindingOrder::eCounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        }

        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    auto GetSampleCount(Multisampling msaa) -> VkSampleCountFlagBits {
        switch (msaa) {
            case Multisampling::eMsaaX1:
                return VK_SAMPLE_COUNT_1_BIT;

            case Multisampling::eMsaaX2:
                return VK_SAMPLE_COUNT_2_BIT;

            case Multisampling::eMsaaX4:
                return VK_SAMPLE_COUNT_4_BIT;

            case Multisampling::eMsaaX8:
                return VK_SAMPLE_COUNT_8_BIT;

            case Multisampling::eMsaaX16:
                return VK_SAMPLE_COUNT_16_BIT;

            default:
                return VK_SAMPLE_COUNT_1_BIT;
        }
    }

    auto GetTopology(PrimitiveTopology topology) -> VkPrimitiveTopology {
        switch (topology) {
            case PrimitiveTopology::ePointList:
                return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

            case PrimitiveTopology::eLineList:
                return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

            case PrimitiveTopology::eLineStrip:
                return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;

            case PrimitiveTopology::eTriangleList:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            case PrimitiveTopology::eTriangleStrip:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

            case PrimitiveTopology::eTriangleFan:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

            default:
                return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
        }
    }

    auto GetDescriptorType( ResourceType type ) -> VkDescriptorType {
        switch (type) {
            case ResourceType::eTexture_SRV:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

            case ResourceType::eTexture_UAV:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

            case ResourceType::eTypedBuffer_SRV:
                return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

            case ResourceType::eTypedBuffer_UAV:
                return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

            case ResourceType::eStructuredBuffer_SRV:
            case ResourceType::eRawBuffer_SRV:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            case ResourceType::eStructuredBuffer_UAV:
            case ResourceType::eRawBuffer_UAV:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            case ResourceType::eConstantBuffer:
            case ResourceType::eVolatileConstantBuffer:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

            case ResourceType::eSampler:
                return VK_DESCRIPTOR_TYPE_SAMPLER;

            case ResourceType::eRayTracingAccelStruct:
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

            case ResourceType::ePushConstants:
            case ResourceType::eInvalid:
            case ResourceType::eSamplerFeedbackTexture_UAV:
            default:
                MKT_ASSERT(false, "Invalid or unsupported ResourceType for descriptor");
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    auto GetArraLayerCount( TextureDimension dimension, u32 requestedLayers ) -> u32 {
        switch (dimension) {
            case TextureDimension::eTexture1D:
            case TextureDimension::eTexture2D:
            case TextureDimension::eTexture2DMS:
            case TextureDimension::eTexture3D:
                return 1;

            case TextureDimension::eTextureCube:
                return 6;

            case TextureDimension::eTexture1DArray:
            case TextureDimension::eTexture2DArray:
            case TextureDimension::eTexture2DMSArray:
                return requestedLayers;

            case TextureDimension::eTextureCubeArray:
                // Must be a multiple of 6 for cubemap arrays
                return requestedLayers * 6;

            case TextureDimension::eInvalid:
            default:
                return 0;
        }
    }

    auto GetImageUsage( TextureUsageFlags flags ) -> VkImageUsageFlags {
        VkImageUsageFlags result{};
        if (flags & TextureUsageFlagsBits::kRenderTarget) {
            result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        if (flags & TextureUsageFlagsBits::kDepthTarget) {
            result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }

        if (flags & TextureUsageFlagsBits::kShaderResource) {
            result |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }

        if (flags & TextureUsageFlagsBits::kCopySrc) {
            result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        if (flags & TextureUsageFlagsBits::kCopyDst) {
            result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        return result;
    }

    auto GetSamplerFilter( SamplerFilter filter ) -> VkFilter {
        switch ( filter ) {
            case SamplerFilter::eNearest:
                return VK_FILTER_NEAREST;
            case SamplerFilter::eLinear:
                return VK_FILTER_LINEAR;
            default:
                // Empty
                ;
        }

        return VK_FILTER_NEAREST;
    }

    auto GetSamplerWrap( SamplerWrapMode wrap ) -> VkSamplerAddressMode {
        switch ( wrap ) {
            case SamplerWrapMode::eClampToEdge:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case SamplerWrapMode::eClampToBorder:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case SamplerWrapMode::eRepeat:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            default:
                // Empty
                ;
        }

        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    auto GetQueueName( QueueType type ) -> eastl::string_view {
        switch ( type ) {
            case QueueType::eGraphics: return "Graphics";
            case QueueType::eCompute: return "Compute";
            case QueueType::eTransfer: return "Transfer";
            case QueueType::ePresent: return "Present";
            default:;
        }

        return eastl::string_view{};
    }

    auto GetViewType( TextureDimension dimensions ) -> VkImageViewType {
        switch (dimensions) {
            case TextureDimension::eTexture1D:
                return VK_IMAGE_VIEW_TYPE_1D;

            case TextureDimension::eTextureCube:
                return VK_IMAGE_VIEW_TYPE_CUBE;

            case TextureDimension::eTexture2D:
                return VK_IMAGE_VIEW_TYPE_2D;

            case TextureDimension::eTexture3D:
                return VK_IMAGE_VIEW_TYPE_3D;

            case TextureDimension::eTexture1DArray:
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;

            case TextureDimension::eTexture2DArray:
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;

            case TextureDimension::eTextureCubeArray:
                return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            default:;
        }

        MKT_ASSERT(false, "Unknown TextureDimension");
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    auto GetTextureType( TextureDimension dimensions ) -> VkImageType {
        switch (dimensions) {
            case TextureDimension::eTexture1D:
            case TextureDimension::eTexture1DArray:
                return VK_IMAGE_TYPE_1D;

            case TextureDimension::eTexture2D:
            case TextureDimension::eTexture2DArray:
            case TextureDimension::eTextureCube:
            case TextureDimension::eTextureCubeArray:
            case TextureDimension::eTexture2DMS:
            case TextureDimension::eTexture2DMSArray:
                return VK_IMAGE_TYPE_2D;

            case TextureDimension::eTexture3D:
                return VK_IMAGE_TYPE_3D;
            default:;
        }

        return VK_IMAGE_TYPE_2D;
    }

    auto GetImageLayout( ResourceStates state ) -> VkImageLayout {
        switch (state) {
            case ResourceStates::eUnknown:
                return VK_IMAGE_LAYOUT_UNDEFINED;

            case ResourceStates::eCommon:
                return VK_IMAGE_LAYOUT_GENERAL;

            case ResourceStates::eRenderTarget:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            case ResourceStates::eDepthWrite:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            case ResourceStates::eDepthRead:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            case ResourceStates::eShaderResource:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            case ResourceStates::eUnorderedAccess:
                return VK_IMAGE_LAYOUT_GENERAL;

            case ResourceStates::eCopyDest:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            case ResourceStates::eCopySource:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            case ResourceStates::eResolveDest:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            case ResourceStates::eResolveSource:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            case ResourceStates::ePresent:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            case ResourceStates::eConstantBuffer:
            case ResourceStates::eVertexBuffer:
            case ResourceStates::eIndexBuffer:
            case ResourceStates::eIndirectArgument:
                return VK_IMAGE_LAYOUT_UNDEFINED;

            case ResourceStates::eAccelStructRead:
            case ResourceStates::eAccelStructWrite:
            case ResourceStates::eAccelStructBuildInput:
            case ResourceStates::eAccelStructBuildBlas:
                return VK_IMAGE_LAYOUT_GENERAL;
        }

        MKT_ASSERT(false, "Unhandled ResourceState");
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    auto GetResourceState( VkImageLayout layout ) -> ResourceStates {
        switch (layout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                return ResourceStates::eUnknown;

            case VK_IMAGE_LAYOUT_GENERAL:
                // Could be UAV, ray tracing, or "common"
                return ResourceStates::eUnorderedAccess;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return ResourceStates::eRenderTarget;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return ResourceStates::eDepthWrite;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                return ResourceStates::eDepthRead;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return ResourceStates::eShaderResource;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return ResourceStates::eCopyDest;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return ResourceStates::eCopySource;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                return ResourceStates::ePresent;

            default:
                MKT_ASSERT(false, "Unhandled VkImageLayout");
                return ResourceStates::eUnknown;
        }
    }

    auto GetShaderStageFlags(  ShaderFlags visibility ) -> VkShaderStageFlags {
        // Fast path
        if ( visibility == ShaderFlagsBits::kAll ) {
            return VK_SHADER_STAGE_ALL;
        }

        VkShaderStageFlags flags{};

        if ( visibility & ShaderFlagsBits::kVertex ) {
            flags |= VK_SHADER_STAGE_VERTEX_BIT;
        }

        if ( visibility & ShaderFlagsBits::kPixel ) {
            flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        if ( visibility & ShaderFlagsBits::kCompute ) {
            flags |= VK_SHADER_STAGE_COMPUTE_BIT;
        }

        if ( visibility & ShaderFlagsBits::kGeometry ) {
            flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        }

        if ( visibility & ShaderFlagsBits::kHull ) {
            flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }

        if ( visibility & ShaderFlagsBits::kRayGeneration ) {
            flags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        }

        if ( visibility & ShaderFlagsBits::kMiss ) {
            flags |= VK_SHADER_STAGE_MISS_BIT_KHR;
        }

        if ( visibility & ShaderFlagsBits::kClosestHit ) {
            flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        }

        return flags;
    }

    // ==================================================================================================================
    // Initializers
    // ==================================================================================================================

    auto initializers::ApplicationInfo() -> VkApplicationInfo {
        VkApplicationInfo ret{ VK_STRUCTURE_TYPE_APPLICATION_INFO };

        return ret;
    }

    auto initializers::DebugUtilsMessengerCreateInfoEXT() -> VkDebugUtilsMessengerCreateInfoEXT {
        VkDebugUtilsMessengerCreateInfoEXT ret{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        return ret;
    }

    auto initializers::DynamicRenderingFeature() -> VkPhysicalDeviceDynamicRenderingFeatures {
        VkPhysicalDeviceDynamicRenderingFeatures ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
        return ret;
    }

    auto initializers::PhysicalDeviceFeatures2() -> VkPhysicalDeviceFeatures2 {
        VkPhysicalDeviceFeatures2 ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        return ret;
    }
    auto initializers::DeviceCreateInfo() -> VkDeviceCreateInfo {
        VkDeviceCreateInfo ret{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        return ret;
    }

    auto initializers::PhysicalDeviceVulkan13Features() -> VkPhysicalDeviceVulkan13Features {
        VkPhysicalDeviceVulkan13Features ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        return ret;
    }

    auto initializers::PhysicalDeviceVulkan12Features() -> VkPhysicalDeviceVulkan12Features {
        VkPhysicalDeviceVulkan12Features ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        return ret;
    }

    auto initializers::PhysicalDeviceVulkan11Features() -> VkPhysicalDeviceVulkan11Features {
        VkPhysicalDeviceVulkan11Features ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
        return ret;
    }

    auto initializers::DeviceQueueCreateInfo() -> VkDeviceQueueCreateInfo {
        VkDeviceQueueCreateInfo ret{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        return ret;
    }

    auto initializers::DescriptorPoolCreateInfo() -> VkDescriptorPoolCreateInfo {
        VkDescriptorPoolCreateInfo ret{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        return ret;
    }

    auto initializers::SwapchainCreateInfoKHR() -> VkSwapchainCreateInfoKHR {
        VkSwapchainCreateInfoKHR ret{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        return ret;
    }

    auto initializers::ImageViewCreateInfo() -> VkImageViewCreateInfo {
        VkImageViewCreateInfo ret{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        return ret;
    }

    auto initializers::RenderingAttachmentInfo() -> VkRenderingAttachmentInfo {
        VkRenderingAttachmentInfo ret{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        return ret;
    }

    auto initializers::PresentInfoKHR() -> VkPresentInfoKHR {
        VkPresentInfoKHR ret{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        return ret;
    }

    auto initializers::SamplerCreateInfo() -> VkSamplerCreateInfo {
        VkSamplerCreateInfo ret{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        return ret;
    }

    auto initializers::ImageCreateInfo() -> VkImageCreateInfo {
        VkImageCreateInfo ret{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        return ret;
    }

    auto initializers::RenderingInfo() -> VkRenderingInfo {
        VkRenderingInfo ret{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        return ret;
    }

    auto initializers::CommandBufferBeginInfo() -> VkCommandBufferBeginInfo {
        VkCommandBufferBeginInfo ret{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        return ret;
    }

    auto initializers::SemaphoreTypeCreateInfo() -> VkSemaphoreTypeCreateInfo {
        VkSemaphoreTypeCreateInfo ret{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        };
        return ret;
    }

    auto initializers::ImageBlit2() -> VkImageBlit2 {
        VkImageBlit2 ret{ VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
        return ret;
    }

    auto initializers::ImageCopy2() -> VkImageCopy2 {
        VkImageCopy2 ret{ VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
        return ret;
    }

    auto initializers::ShaderModuleCreateInfo() -> VkShaderModuleCreateInfo {
        VkShaderModuleCreateInfo ret{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        return ret;
    }
    auto initializers::PipelineShaderStageCreateInfo() -> VkPipelineShaderStageCreateInfo {
        VkPipelineShaderStageCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        return ret;
    }

    auto initializers::FramebufferCreateInfo() -> VkFramebufferCreateInfo {
        VkFramebufferCreateInfo ret{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        return ret;
    }

    auto initializers::PipelineRenderingCreateInfo() -> VkPipelineRenderingCreateInfo {
        VkPipelineRenderingCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        return ret;
    }

    auto initializers::GraphicsPipelineCreateInfo() -> VkGraphicsPipelineCreateInfo {
        VkGraphicsPipelineCreateInfo ret{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        return ret;
    }

    auto initializers::PipelineVertexInputStateCreateInfo() -> VkPipelineVertexInputStateCreateInfo {
        VkPipelineVertexInputStateCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        return ret;
    }

    auto initializers::PipelineLayoutCreateInfo() -> VkPipelineLayoutCreateInfo {
        VkPipelineLayoutCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        return ret;
    }

    auto initializers::ComputePipelineCreateInfo() -> VkComputePipelineCreateInfo {
        VkComputePipelineCreateInfo ret{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        return ret;
    }

    auto initializers::BufferCreateInfo() -> VkBufferCreateInfo {
        VkBufferCreateInfo ret{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        return ret;
    }
    auto initializers::WriteDescriptorSet() -> VkWriteDescriptorSet {
        VkWriteDescriptorSet ret{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        return ret;
    }

    auto initializers::PipelineCacheCreateInfo() -> VkPipelineCacheCreateInfo {
        VkPipelineCacheCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
        return ret;
    }

    auto initializers::CommandPoolCreateInfo() -> VkCommandPoolCreateInfo {
        VkCommandPoolCreateInfo ret{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        return ret;
    }

    auto initializers::FenceCreateInfo( VkFenceCreateFlags flags ) -> VkFenceCreateInfo {
        VkFenceCreateInfo ret{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = flags,
        };

        return ret;
    }

    auto initializers::CommandBufferAllocateInfo() -> VkCommandBufferAllocateInfo {
        VkCommandBufferAllocateInfo ret{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

        return ret;
    }

    auto initializers::InstanceCreateInfo() -> VkInstanceCreateInfo {
        VkInstanceCreateInfo ret{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };

        return ret;
    }

    auto initializers::SemaphoreCreateInfo() -> VkSemaphoreCreateInfo {
        VkSemaphoreCreateInfo ret{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        return ret;
    }

}// namespace mikoto::renderer::vulkan