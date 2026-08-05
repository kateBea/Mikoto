//    Copyright 2025 ケイト
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

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

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

            case ResourceStates::eCopySource:
            case ResourceStates::eCopyDest:
            case ResourceStates::eResolveSource:
            case ResourceStates::eResolveDest:
                return VK_PIPELINE_STAGE_2_TRANSFER_BIT;

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

    auto GetShaderStageFlags( ShaderStage visibility ) -> VkShaderStageFlags {
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

// #ifndef VULKAN_HELPERS_CC_INCLUDED
// #define VULKAN_HELPERS_CC_INCLUDED
// //
// #include <set>
// #include <stdexcept>
//
// #include <spirv_reflect.h>
//
// #include "vk_mem_alloc.h"
// #include "volk.h"
//
// #include <Renderer/Vulkan/VulkanDevice.hh>
//
// #include "Renderer/Vulkan/VulkanContext.hh"
// #include "Renderer/Vulkan/VulkanGraphicsContext.hh"
// #include "Renderer/Vulkan/VulkanHelpers.hh"
//
// namespace mikoto::VulkanHelpers {
//
//     // Converts VkImageLayout → readable string
//     auto ImageLayoutToString( Texture* texture ) -> void {
//         const auto src{ dynamic_cast<VulkanTexture*>( texture ) };
//         VkImageLayout layout{ src->GetCurrentLayout() };
//         const char* layoutName = "UNKNOWN_LAYOUT";
//
//         switch ( layout ) {
//             case VK_IMAGE_LAYOUT_UNDEFINED:
//                 layoutName = "VK_IMAGE_LAYOUT_UNDEFINED";
//                 break;
//             case VK_IMAGE_LAYOUT_GENERAL:
//                 layoutName = "VK_IMAGE_LAYOUT_GENERAL";
//                 break;
//             case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_PREINITIALIZED:
//                 layoutName = "VK_IMAGE_LAYOUT_PREINITIALIZED";
//                 break;
//             case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
//                 layoutName = "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR";
//                 break;
//             case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL";
//                 break;
//             case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
//                 layoutName = "VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL";
//                 break;
//             default:
//                 layoutName = "UNKNOWN_LAYOUT";
//                 break;
//         }
//
//         MKT_CORE_LOGGER_DEBUG( "Image layout: {} for texture: {}", layoutName, texture->GetDebugName() );
//     }
//
//     auto SetObjectDebugName( VkDevice device, VkObjectType objectType, UInt64 objectHandle, const char* name ) -> void {
//         VkDebugUtilsObjectNameInfoEXT nameInfo{};
//         nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
//         nameInfo.objectType = objectType;
//         nameInfo.objectHandle = objectHandle;
//         nameInfo.pObjectName = name;
//
//         if ( vkSetDebugUtilsObjectNameEXT ) {
//             vkSetDebugUtilsObjectNameEXT( device, std::addressof( nameInfo ) );
//         } else {
//             MKT_CORE_LOGGER_WARN( "VulkanHelpers::SetObjectDebugName - vkGetDeviceProcAddr is null, cannot set debug name." );
//         }
//     }
//
//
//     auto GetUniformBufferPadding( const VkDeviceSize bufferOriginalSize, const VkDeviceSize deviceMinOffsetAlignment ) -> VkDeviceSize {
//         VkDeviceSize alignedSize{ bufferOriginalSize };
//
//         if ( deviceMinOffsetAlignment > 0 )
//             alignedSize = ( alignedSize + deviceMinOffsetAlignment - 1 ) & ~( deviceMinOffsetAlignment - 1 );
//
//         return alignedSize;
//     }
//
//     auto GetStorageBufferPadding( VkDeviceSize bufferOriginalSize, VkDeviceSize deviceMinOffsetAlignment ) -> VkDeviceSize {
//         VkDeviceSize alignedSize{ bufferOriginalSize };
//
//         if ( deviceMinOffsetAlignment > 0 )
//             alignedSize = ( alignedSize + deviceMinOffsetAlignment - 1 ) & ~( deviceMinOffsetAlignment - 1 );
//
//         return alignedSize;
//     }
//
//     auto InferVulkanIndexType( const BufferDataType format ) -> VkIndexType {
//         switch ( format ) {
//             case BufferDataType::BUFFER_DATA_UINT16:
//                 return VK_INDEX_TYPE_UINT16;
//             case BufferDataType::BUFFER_DATA_UINT32:
//                 return VK_INDEX_TYPE_UINT32;
//             default:
//                 MKT_CORE_LOGGER_ERROR( "VulkanHelpers::InferVulkanIndexType - Invalid index type." );
//                 return VK_INDEX_TYPE_MAX_ENUM;
//         }
//
//         return VK_INDEX_TYPE_MAX_ENUM;
//     }
//
//     auto ImageUsageFlagsToString( Texture* texture ) -> void {
//         const auto src{ dynamic_cast<VulkanTexture*>( texture ) };
//         VkImageUsageFlags flags{ src->GetCreateInfo().usage };
//
//         std::ostringstream oss;
//         bool first = true;
//
//         auto append = [&]( const char* name ) {
//             if ( !first ) oss << " | ";
//             oss << name;
//             first = false;
//         };
//
//         if ( flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) append( "TRANSFER_SRC" );
//         if ( flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) append( "TRANSFER_DST" );
//         if ( flags & VK_IMAGE_USAGE_SAMPLED_BIT ) append( "SAMPLED" );
//         if ( flags & VK_IMAGE_USAGE_STORAGE_BIT ) append( "STORAGE" );
//         if ( flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) append( "COLOR_ATTACHMENT" );
//         if ( flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) append( "DEPTH_STENCIL_ATTACHMENT" );
//         if ( flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) append( "TRANSIENT_ATTACHMENT" );
//         if ( flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ) append( "INPUT_ATTACHMENT" );
//         if ( flags & VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR ) append( "FRAGMENT_SHADING_RATE_ATTACHMENT_KHR" );
//         if ( flags & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT ) append( "FRAGMENT_DENSITY_MAP_EXT" );
//
//         if ( first )
//             oss << "NONE";
//
//         MKT_CORE_LOGGER_DEBUG( "Image usage flags: {} for texture: {}", oss.str(), texture->GetDebugName() );
//     }
//
//     auto ToVkImageUsage( const TextureUsage usage ) -> VkImageUsageFlags {
//         switch ( usage ) {
//             case TextureUsage::COLOR:
//                 // Textures that I will print to and can copy them to toehr textures like the color image from ImGui Backend
//                 return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;// | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//
//             case TextureUsage::DEPTH:
//                 return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
//                        VK_IMAGE_USAGE_SAMPLED_BIT |
//                        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//
//             case TextureUsage::NORMAL:
//                 // For example textures that I load from disc
//                 return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//
//             case TextureUsage::STORAGE:
//                 return VK_IMAGE_USAGE_STORAGE_BIT |
//                        VK_IMAGE_USAGE_SAMPLED_BIT |
//                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
//                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//
//             case TextureUsage::CUBE:
//                 return VK_IMAGE_USAGE_SAMPLED_BIT |
//                        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//
//             case TextureUsage::RENDER_TARGET:
//                 return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
//                        VK_IMAGE_USAGE_SAMPLED_BIT |
//                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
//                        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//
//             default:;
//         }
//
//         return VK_IMAGE_USAGE_SAMPLED_BIT;
//     }
//
//     auto ToVkRasterSamples( Multisampling samples ) -> VkSampleCountFlagBits {
//         switch ( samples ) {
//             case Multisampling::MSAA_X1:  return VK_SAMPLE_COUNT_1_BIT;
//             case Multisampling::MSAA_X2:  return VK_SAMPLE_COUNT_2_BIT;
//             case Multisampling::MSAA_X4:  return VK_SAMPLE_COUNT_4_BIT;
//             case Multisampling::MSAA_X8:  return VK_SAMPLE_COUNT_8_BIT;
//             case Multisampling::MSAA_X16: return VK_SAMPLE_COUNT_16_BIT;
//             default:                      return VK_SAMPLE_COUNT_1_BIT;
//         }
//     }
//
//     auto GetAspectMask(VkFormat format) -> VkImageAspectFlags {
//         switch (format) {
//             // Color formats
//             case VK_FORMAT_R8_UNORM:
//             case VK_FORMAT_R8G8B8A8_UNORM:
//             case VK_FORMAT_B8G8R8A8_UNORM:
//             case VK_FORMAT_R16G16B16A16_SFLOAT:
//             case VK_FORMAT_R32G32B32A32_SFLOAT:
//                 return VK_IMAGE_ASPECT_COLOR_BIT;
//
//                 // Depth-only formats
//             case VK_FORMAT_D16_UNORM:
//             case VK_FORMAT_D32_SFLOAT:
//                 return VK_IMAGE_ASPECT_DEPTH_BIT;
//
//                 // Depth + Stencil formats
//             case VK_FORMAT_D24_UNORM_S8_UINT:
//             case VK_FORMAT_D32_SFLOAT_S8_UINT:
//                 return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
//
//                 // Stencil-only formats (rare)
//             case VK_FORMAT_S8_UINT:
//                 return VK_IMAGE_ASPECT_STENCIL_BIT;
//
//             default:
//                 return VK_IMAGE_ASPECT_COLOR_BIT;
//         }
//     }
//
//     auto SetupDeviceQueueCreateInfo( const std::set<UInt32>& uniqueQueueFamilies ) -> std::vector<VkDeviceQueueCreateInfo> {
//         std::vector<VkDeviceQueueCreateInfo> result{};
//
//         // static because pQueuePriorities is a pointer to a float
//         static constexpr float queuePriority{ 1.0f };
//         for ( const UInt32 queueFamily: uniqueQueueFamilies ) {
//             VkDeviceQueueCreateInfo queueCreateInfo{ Initializers::DeviceQueueCreateInfo() };
//
//             queueCreateInfo.queueFamilyIndex = queueFamily;
//             queueCreateInfo.queueCount = 1;
//             queueCreateInfo.pQueuePriorities = std::addressof( queuePriority );
//
//             result.push_back( queueCreateInfo );
//         }
//
//         return result;
//     }
//
//     auto ToVkStage( const ShaderStage stage ) -> VkShaderStageFlagBits {
//         switch ( stage ) {
//             case ShaderStage::eVertex:
//                 return VK_SHADER_STAGE_VERTEX_BIT;
//             case ShaderStage::eCompute:
//                 return VK_SHADER_STAGE_COMPUTE_BIT;
//             case ShaderStage::eFragment:
//                 return VK_SHADER_STAGE_FRAGMENT_BIT;
//             default:
//                 return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
//         }
//     }
//
//     auto FromVkStage( VkShaderStageFlagBits stage ) -> ShaderStage {
//         switch ( stage ) {
//             case VK_SHADER_STAGE_VERTEX_BIT: return ShaderStage::eVertex;
//             case VK_SHADER_STAGE_COMPUTE_BIT: return ShaderStage::eCompute;
//             case VK_SHADER_STAGE_FRAGMENT_BIT: return ShaderStage::eFragment;
//             default: return ShaderStage::eInvalid;
//         }
//     }
//
//     auto ToVkFormat( const TextureFormat format ) -> VkFormat {
//         switch ( format ) {
//             case TextureFormat::RGBA8_SNORM:
//                 return VK_FORMAT_R8G8B8A8_SNORM;
//             case TextureFormat::R8_UNORM:
//                 return VK_FORMAT_R8_UNORM;
//             case TextureFormat::RG8_UNORM:
//                 return VK_FORMAT_R8G8_UNORM;
//             case TextureFormat::RGB8_UNORM:
//                 return VK_FORMAT_R8G8B8_UNORM;
//             case TextureFormat::RGBA8_UNORM:
//                 return VK_FORMAT_R8G8B8A8_UNORM;
//             case TextureFormat::RG8_SNORM:
//                 return VK_FORMAT_R8G8B8_SNORM;
//             case TextureFormat::SRGB8:
//                 return VK_FORMAT_R8G8B8_SRGB;
//             case TextureFormat::SRGB8_ALPHA8:
//                 return VK_FORMAT_R8G8B8A8_SRGB;
//             case TextureFormat::R16_FLOAT:
//                 return VK_FORMAT_R16_SFLOAT;
//             case TextureFormat::RG16_FLOAT:
//                 return VK_FORMAT_R16G16_SFLOAT;
//             case TextureFormat::RGB16_FLOAT:
//                 return VK_FORMAT_R16G16B16_SFLOAT;
//             case TextureFormat::RGBA16_FLOAT:
//                 return VK_FORMAT_R16G16B16A16_SFLOAT;
//             case TextureFormat::R32_FLOAT:
//                 return VK_FORMAT_R32_SFLOAT;
//             case TextureFormat::RG32_FLOAT:
//                 return VK_FORMAT_R32G32_SFLOAT;
//             case TextureFormat::RGB32_FLOAT:
//                 return VK_FORMAT_R32G32B32_SFLOAT;
//             case TextureFormat::RGBA32_FLOAT:
//                 return VK_FORMAT_R32G32B32A32_SFLOAT;
//             case TextureFormat::D16_UNORM:
//                 return VK_FORMAT_D16_UNORM;
//             case TextureFormat::D24_UNORM_S8_UINT:
//                 return VK_FORMAT_D24_UNORM_S8_UINT;
//             case TextureFormat::D32_FLOAT:
//                 return VK_FORMAT_D32_SFLOAT;
//             case TextureFormat::D32_FLOAT_S8_UINT:
//                 return VK_FORMAT_D32_SFLOAT_S8_UINT;
//             default:
//                 return VK_FORMAT_UNDEFINED;
//         }
//     }
//
//     auto ToTextureFormat( VkFormat format ) -> TextureFormat {
//         switch ( format ) {
//             // --- 8-bit normalized ---
//             case VK_FORMAT_R8_UNORM:
//                 return TextureFormat::R8_UNORM;
//             case VK_FORMAT_R8G8_UNORM:
//                 return TextureFormat::RG8_UNORM;
//             case VK_FORMAT_R8G8B8_UNORM:
//                 return TextureFormat::RGB8_UNORM;
//             case VK_FORMAT_R8G8B8A8_UNORM:
//                 return TextureFormat::RGBA8_UNORM;
//
//             // --- 8-bit signed normalized ---
//             case VK_FORMAT_R8_SNORM:
//                 return TextureFormat::R8_SNORM;
//             case VK_FORMAT_R8G8_SNORM:
//                 return TextureFormat::RG8_SNORM;
//             case VK_FORMAT_R8G8B8_SNORM:
//                 return TextureFormat::RGB8_SNORM;
//             case VK_FORMAT_R8G8B8A8_SNORM:
//                 return TextureFormat::RGBA8_SNORM;
//
//             // --- 16-bit normalized ---
//             case VK_FORMAT_R16_UNORM:
//                 return TextureFormat::R16_UNORM;
//             case VK_FORMAT_R16G16_UNORM:
//                 return TextureFormat::RG16_UNORM;
//             case VK_FORMAT_R16G16B16_UNORM:
//                 return TextureFormat::RGB16_UNORM;
//             case VK_FORMAT_R16G16B16A16_UNORM:
//                 return TextureFormat::RGBA16_UNORM;
//
//             // --- 16-bit float ---
//             case VK_FORMAT_R16_SFLOAT:
//                 return TextureFormat::R16_FLOAT;
//             case VK_FORMAT_R16G16_SFLOAT:
//                 return TextureFormat::RG16_FLOAT;
//             case VK_FORMAT_R16G16B16_SFLOAT:
//                 return TextureFormat::RGB16_FLOAT;
//             case VK_FORMAT_R16G16B16A16_SFLOAT:
//                 return TextureFormat::RGBA16_FLOAT;
//
//             // --- 32-bit float ---
//             case VK_FORMAT_R32_SFLOAT:
//                 return TextureFormat::R32_FLOAT;
//             case VK_FORMAT_R32G32_SFLOAT:
//                 return TextureFormat::RG32_FLOAT;
//             case VK_FORMAT_R32G32B32_SFLOAT:
//                 return TextureFormat::RGB32_FLOAT;
//             case VK_FORMAT_R32G32B32A32_SFLOAT:
//                 return TextureFormat::RGBA32_FLOAT;
//
//             // --- sRGB ---
//             case VK_FORMAT_R8G8B8_SRGB:
//                 return TextureFormat::SRGB8;
//             case VK_FORMAT_R8G8B8A8_SRGB:
//                 return TextureFormat::SRGB8_ALPHA8;
//
//             // --- Depth / Stencil ---
//             case VK_FORMAT_D16_UNORM:
//                 return TextureFormat::D16_UNORM;
//             case VK_FORMAT_D24_UNORM_S8_UINT:
//                 return TextureFormat::D24_UNORM_S8_UINT;
//             case VK_FORMAT_D32_SFLOAT:
//                 return TextureFormat::D32_FLOAT;
//             case VK_FORMAT_D32_SFLOAT_S8_UINT:
//                 return TextureFormat::D32_FLOAT_S8_UINT;
//
//             default:;
//         }
//
//         return TextureFormat::INVALID;
//     }
//
//     auto GetVkFormatFromTextureFormat( TextureFormat format, TextureUsage usage, VkPhysicalDevice device ) -> VkFormat {
//         VkFormat result{ ToVkFormat( format ) };
//
//         const std::initializer_list<const VkFormat> targetColorFormats{
//             VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB
//         };
//
//         const std::initializer_list<const VkFormat> targetDepthFormats{
//             VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT
//         };
//
//         if ( usage == TextureUsage::COLOR ) {
//             result = FindSupportedFormat(
//                     device,
//                     targetColorFormats,
//                     VK_IMAGE_TILING_OPTIMAL,
//                     VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT );
//         }
//
//         if ( usage == TextureUsage::DEPTH ) {
//             result = FindSupportedFormat(
//                     device,
//                     targetDepthFormats,
//                     VK_IMAGE_TILING_OPTIMAL,
//                     VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
//         }
//
//         return result;
//     }
//
//     auto GetSwapChainSupport( const VkPhysicalDevice& device, const VkSurfaceKHR& surface ) -> SwapChainSupportDetails {
//         SwapChainSupportDetails details{};
//         vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, std::addressof( details.Capabilities ) );
//
//         UInt32 formatCount{};
//         vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, std::addressof( formatCount ), nullptr );
//         if ( formatCount != 0 ) {
//             details.Formats.resize( formatCount );
//             vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, std::addressof( formatCount ), details.Formats.data() );
//         }
//
//         UInt32 presentModeCount{};
//         vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, std::addressof( presentModeCount ), nullptr );
//         if ( presentModeCount != 0 ) {
//             details.PresentModes.resize( presentModeCount );
//             vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, std::addressof( presentModeCount ), details.PresentModes.data() );
//         }
//
//         return details;
//     }
//
//     auto FindSupportedFormat( VkPhysicalDevice device, std::span<const VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features ) -> VkFormat {
//         for ( const VkFormat format: candidates ) {
//             VkFormatProperties props{};
//             vkGetPhysicalDeviceFormatProperties( device, format, std::addressof( props ) );
//
//             if ( ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) ||
//                  ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) ) {
//                 return format;
//             }
//         }
//
//         return VK_FORMAT_UNDEFINED;
//     }
//
//     auto HasGraphicsQueue( const VkQueueFamilyProperties& queueFamily ) -> bool {
//         return queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT;
//     }
//
//     auto HasComputeQueue( const VkQueueFamilyProperties& queueFamily ) -> bool {
//         return queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT;
//     }
//
//     auto HasPresentQueue( const VkPhysicalDevice& device, const UInt32 queueFamilyIndex, const VkSurfaceKHR& surface, const VkQueueFamilyProperties& queueFamilyProperties ) -> bool {
//         VkBool32 presentSupport{ VK_FALSE };
//         if ( vkGetPhysicalDeviceSurfaceSupportKHR( device, queueFamilyIndex, surface, std::addressof( presentSupport ) ) != VK_SUCCESS ) {
//             MKT_CORE_LOGGER_ERROR( "VulkanHelpers::HasPresentQueue - Failed to get Physical device surface support." );
//             return false;
//         }
//
//         // Has present queues and at least one of them available
//         if ( queueFamilyProperties.queueCount > 0 && presentSupport == VK_TRUE ) {
//             return true;
//         }
//
//         return false;
//     }
//
//     auto ToVkShaderDataType( const ShaderDataType type ) -> VkFormat {
//         switch ( type ) {
//             case ShaderDataType::FLOAT_TYPE:
//                 return VK_FORMAT_R32_SFLOAT;
//             case ShaderDataType::FLOAT2_TYPE:
//                 return VK_FORMAT_R32G32_SFLOAT;
//             case ShaderDataType::FLOAT3_TYPE:
//                 return VK_FORMAT_R32G32B32_SFLOAT;
//             case ShaderDataType::FLOAT4_TYPE:
//                 return VK_FORMAT_R32G32B32A32_SFLOAT;
//
//             case ShaderDataType::MAT3_TYPE:
//             case ShaderDataType::MAT4_TYPE:
//                 return VK_FORMAT_UNDEFINED;//temporary
//
//             case ShaderDataType::INT_TYPE:
//                 return VK_FORMAT_R32_SINT;
//             case ShaderDataType::UINT_TYPE:
//                 return VK_FORMAT_R32_UINT;
//             case ShaderDataType::INT2_TYPE:
//                 return VK_FORMAT_R32G32_SINT;
//             case ShaderDataType::INT3_TYPE:
//                 return VK_FORMAT_R32G32B32_SINT;
//             case ShaderDataType::INT4_TYPE:
//                 return VK_FORMAT_R32G32B32A32_SINT;
//             case ShaderDataType::BOOL_TYPE:
//                 return VK_FORMAT_R32_SINT;
//
//             case ShaderDataType::NONE:
//             case ShaderDataType::COUNT:
//                 [[fallthrough]];
//             default:
//                 MKT_ASSERT( false, "Invalid shader data type" );
//         }
//     }
//
//     auto CopyImageToImage( const VkCommandBuffer cmd, const VkImage source, const VkImage destination, const VkExtent3D srcSize, const VkExtent3D dstSize ) -> void {
//         VkImageBlit2 blitRegion{};
//         blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
//         blitRegion.pNext = nullptr;
//
//         // [0] zeroed so blit operation starts at the
//         // origin (top-left) of the image
//
//         // Source
//         blitRegion.srcOffsets[0] = { 0, 0, 0 };
//         blitRegion.srcOffsets[1].x = srcSize.width;
//         blitRegion.srcOffsets[1].y = srcSize.height;
//         blitRegion.srcOffsets[1].z = 1;
//
//         // destination
//         blitRegion.dstOffsets[0] = { 0, 0, 0 };
//         blitRegion.dstOffsets[1].x = dstSize.width;
//         blitRegion.dstOffsets[1].y = dstSize.height;
//         blitRegion.dstOffsets[1].z = 1;
//
//         blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         blitRegion.srcSubresource.baseArrayLayer = 0;
//         blitRegion.srcSubresource.layerCount = 1;
//         blitRegion.srcSubresource.mipLevel = 0;
//
//         blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         blitRegion.dstSubresource.baseArrayLayer = 0;
//         blitRegion.dstSubresource.layerCount = 1;
//         blitRegion.dstSubresource.mipLevel = 0;
//
//         VkBlitImageInfo2 blitInfo{};
//         blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
//         blitInfo.pNext = nullptr;
//         blitInfo.dstImage = destination;
//         blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//         blitInfo.srcImage = source;
//         blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//         blitInfo.filter = VK_FILTER_NEAREST;
//         blitInfo.regionCount = 1;
//         blitInfo.pRegions = &blitRegion;
//
//         vkCmdBlitImage2( cmd, &blitInfo );
//     }
//
//     auto CopyImageToImageMultiSampled( const VkCommandBuffer cmd, const VkImage source, const VkImage destination, const VkExtent3D srcSize ) -> void {
//         VkImageResolve2 resolve{};
//         resolve.sType = VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2;
//
//         resolve.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         resolve.srcSubresource.mipLevel = 0;
//         resolve.srcSubresource.baseArrayLayer = 0;
//         resolve.srcSubresource.layerCount = 1;
//
//         resolve.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         resolve.dstSubresource.mipLevel = 0;
//         resolve.dstSubresource.baseArrayLayer = 0;
//         resolve.dstSubresource.layerCount = 1;
//
//         resolve.extent = { srcSize.width, srcSize.height, 1 };
//
//         VkResolveImageInfo2 info{};
//         info.sType = VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2;
//         info.srcImage = source;
//         info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//         info.dstImage = destination;
//         info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//         info.regionCount = 1;
//         info.pRegions = &resolve;
//
//         vkCmdResolveImage2(cmd, &info);
//     }
//
//     auto CopyImage(VkCommandBuffer cmd, VkImage srcImage, VkImageLayout srcLayout, VkImage dstImage, VkImageLayout dstLayout, VkExtent3D extent ) -> void {
//         VkImageCopy copyRegion{};
//         copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         copyRegion.srcSubresource.mipLevel = 0;
//         copyRegion.srcSubresource.baseArrayLayer = 0;
//         copyRegion.srcSubresource.layerCount = 1;
//
//         copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         copyRegion.dstSubresource.mipLevel = 0;
//         copyRegion.dstSubresource.baseArrayLayer = 0;
//         copyRegion.dstSubresource.layerCount = 1;
//
//         copyRegion.srcOffset = { 0, 0, 0 };
//         copyRegion.dstOffset = { 0, 0, 0 };
//         copyRegion.extent = extent;// width, height, depth
//
//         vkCmdCopyImage(
//                 cmd,
//                 srcImage, srcLayout,
//                 dstImage, dstLayout,
//                 1, &copyRegion );
//     }
//
// }// namespace Mikoto::VulkanHelpers
//
// namespace mikoto::VulkanHelpers::Reflection {
//
//     auto ToVkDescriptorType( SpvReflectDescriptorType type ) -> VkDescriptorType {
//         switch ( type ) {
//             case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
//                 return VK_DESCRIPTOR_TYPE_SAMPLER;
//             case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
//                 return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//             case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
//                 return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
//             case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
//                 return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//             case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
//                 return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
//             case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
//                 return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
//
//             case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
//                 return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//             case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
//                 return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
//
//             case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
//                 return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//             case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
//                 return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
//
//             case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
//                 return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
//
//     #if defined( VK_KHR_acceleration_structure )
//             case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
//                 return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
//     #endif
//             default:
//                 return VK_DESCRIPTOR_TYPE_MAX_ENUM;
//         }
//     }
//
//     MKT_NODISCARD static auto IsBindlessEnabled() -> bool {
// #if defined( MKT_USE_VULKAN_BINDLESS )
//         return true;
// #else
//         return false;
// #endif
//     }
//
//     // NOTE: About bindless descriptors, for simplicity their name will contain "bindless", e.g., "bindless_textures"
//     // Helper: process descriptor sets for a single SPIR-V module and merge into `sets` and `out`
//     static void ProcessDescriptorSets(SpvReflectShaderModule& mod, VkShaderStageFlagBits stage, std::map<UInt32, std::unordered_map<UInt32, VkDescriptorSetLayoutBinding>>& sets, ReflectedData& out) {
//         UInt32 setCount{};
//         spvReflectEnumerateDescriptorSets(&mod, &setCount, nullptr);
//
//         std::vector<SpvReflectDescriptorSet*> reflectedSets(setCount);
//         spvReflectEnumerateDescriptorSets(&mod, &setCount, reflectedSets.data());
//
//         for (auto* reflectedDescriptorSet: reflectedSets) {
//             for (UInt32 setBinding{}; setBinding < reflectedDescriptorSet->binding_count; ++setBinding) {
//                 auto* reflectedBinding{ reflectedDescriptorSet->bindings[setBinding] };
//
//                 UInt32 setIndex{ reflectedDescriptorSet->set };
//                 auto& bindingMap{ sets[setIndex] };
//
//                 if (auto it{ bindingMap.find(reflectedBinding->binding) }; it == bindingMap.end()) {
//                     // If this set does not have this binding yet, add it
//
//                     VkDescriptorSetLayoutBinding bindingInfo{};
//                     bindingInfo.binding = reflectedBinding->binding;
//                     bindingInfo.descriptorType = ToVkDescriptorType(reflectedBinding->descriptor_type);
//
//                     // BUFFER_VIEWS_SET_INDEX set uses uniform and storage dynamics
//                     if (setIndex == BUFFER_VIEWS_SET_INDEX && reflectedBinding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
//                         bindingInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
//
//                         out.DynamicBuffersBindingCount++;
//                     }
//                     if (setIndex == BUFFER_VIEWS_SET_INDEX && reflectedBinding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
//                         bindingInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
//
//                         out.DynamicBuffersBindingCount++;
//                     }
//
//                     bool isBindless{ false };
//
//                     if ( setIndex == UNBOUNDED_IV_SAMPLERS_SET_INDEX || setIndex == UNBOUNDED_BV_SET_INDEX ) {
//                         isBindless = true;
//                     }
//
//                     // IMPORTANT: bindless textures need to be the last binding if they are sharing a SET with other bindings
//                     bindingInfo.descriptorCount = std::max( 1u, isBindless ? MAX_BINDLESS_GROUP_INDEX : reflectedBinding->count );
//
//                     bindingInfo.stageFlags = stage;
//                     bindingMap[bindingInfo.binding] = bindingInfo;
//
//                     out.bindingMap[{ setIndex, bindingInfo.binding }] = ReflectedBindingInfo{
//                         reflectedBinding->name,
//                         setIndex,
//                         bindingInfo.binding,
//                         bindingInfo.descriptorType,
//                         bindingInfo.descriptorCount,
//                         static_cast<VkShaderStageFlags>( stage ),
//                         isBindless
//                     };
//                 } else {
//                     // If this set already has this binding, just update stage flags
//                     it->second.stageFlags |= stage;
//                     out.bindingMap[{ reflectedDescriptorSet->set, it->second.binding }].stageFlags |= stage;
//                 }
//             }
//         }
//     }
//
//     // Helper: collect push constants from a single SPIR-V module
//     static void ProcessPushConstants(SpvReflectShaderModule& mod, VkShaderStageFlagBits stage, std::vector<VkPushConstantRange>& pushConstants) {
//
//         // Follow same structure as graphics context, global set of constants passed per draw
//         VkPushConstantRange psRange{
//             .stageFlags = VK_SHADER_STAGE_ALL,
//             .offset     = 0,
//             .size       = MINIMUM_REQUIRED_PUSH_CONSTANTS_SIZE
//         };
//
//         // Push constants are globals and declared once for a single pipeline
//         if (pushConstants.empty()) {
//             pushConstants.emplace_back( psRange );
//         }
//     }
//
//     static auto InferSizeInBytesSpirVAttribute( SpvReflectFormat format ) -> Size {
//         switch (format) {
//             case SPV_REFLECT_FORMAT_R32_SFLOAT:
//                 return 4;
//
//             case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
//                 return 8;
//
//             case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
//                 return 12;
//
//             case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
//                 return 16;
//
//             default:
//                 // If unsupported or unknown, return 0 or handle however your engine expects
//                 return 0;
//         }
//     }
//
//     // Helper: collect vertex inputs for vertex-stage modules
//     static void ProcessVertexInputs(SpvReflectShaderModule& mod, ReflectedData& out) {
//         UInt32 inputCount{};
//         spvReflectEnumerateInputVariables(&mod, &inputCount, nullptr);
//         std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
//         spvReflectEnumerateInputVariables(&mod, &inputCount, inputs.data());
//
//         Size stride{};
//         UInt32 binding{};
//         for (auto* v: inputs) {
//             if (v->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
//                 continue;
//             }
//
//             VkVertexInputAttributeDescription attr{};
//             attr.binding = binding;
//             attr.location = v->location;
//
//             // It is the user who will decide how
//             // they want to pass the data in and how
//             // the attributes are layout in the buffer they will upload to the GPU
//             attr.offset = 0;
//
//             switch (v->format) {
//                 case SPV_REFLECT_FORMAT_R32_SFLOAT:
//                     attr.format = VK_FORMAT_R32_SFLOAT;
//                     break;
//                 case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
//                     attr.format = VK_FORMAT_R32G32_SFLOAT;
//                     break;
//                 case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
//                     attr.format = VK_FORMAT_R32G32B32_SFLOAT;
//                     break;
//                 case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
//                     attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
//                     break;
//                 default:
//                     attr.format = VK_FORMAT_UNDEFINED;
//                     break;
//             }
//
//             stride += InferSizeInBytesSpirVAttribute(v->format);
//             out.vertexAttributes.push_back(attr);
//         }
//
//         if (!out.vertexAttributes.empty()) {
//             VkVertexInputBindingDescription bind{};
//             bind.binding = 0;
//             bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//
//             // This field will simply tell
//             // the minimum recorded byte offset between set of vertex attributes
//             bind.stride = stride;
//
//             out.vertexBindings.push_back(bind);
//         }
//     }
//
//     // Helper: create descriptor set layouts from collected `sets`
//     static VkResult CreateDescriptorSetLayouts(VkDevice device, const std::map<UInt32, std::unordered_map<UInt32, VkDescriptorSetLayoutBinding>>& sets, ReflectedData& out) {
//         for (const auto& [setIndex, bindings] : sets) {
//             std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
//             layoutBindings.reserve(bindings.size());
//
//             for (const auto& binding : bindings | std::ranges::views::values) {
//                 layoutBindings.push_back( binding );
//             }
//
//             VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
//             layoutInfo.bindingCount = static_cast<UInt32>( layoutBindings.size() );
//             layoutInfo.pBindings = layoutBindings.data();
//
//             std::vector<VkDescriptorBindingFlags> bindingFlags(layoutBindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
//
//             // BUFFER_VIEWS_SET_INDEX need no flags, dynamic Storage buffers and dynamic uniforms cannot have update after bind bit
//             if (setIndex == BUFFER_VIEWS_SET_INDEX) {
//                 for (auto& flag : bindingFlags) {
//                     flag = VK_FLAGS_NONE;
//                 }
//             }
//
//             VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
//             flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
//
//             if (IsBindlessEnabled()) {
//
//                 for (Size i{}; i < layoutBindings.size(); i++) {
//                     auto bindingNumber{ layoutBindings[i].binding };
//                     auto& bindingInfo{ out.bindingMap[{ setIndex, bindingNumber }] };
//
//                     if (bindingInfo.IsBindless) {
//                         bindingFlags[i] =
//                             VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
//                             VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
//                             VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
//                     }
//                 }
//
//                 layoutInfo.pNext = &flagsInfo;
//                 layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
//             }
//
//             flagsInfo.bindingCount = static_cast<UInt32>(bindingFlags.size());
//             flagsInfo.pBindingFlags = bindingFlags.data();
//
//             VkDescriptorSetLayout layoutHandle{};
//             if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layoutHandle) != VK_SUCCESS) {
//                 return VK_ERROR_INITIALIZATION_FAILED;
//             }
//
//             out.setLayouts.emplace(setIndex, layoutHandle);
//         }
//
//         return VK_SUCCESS;
//     }
//
//     static auto CreatePipelineLayout( VkDevice device, ReflectedData& out, std::vector<VkPushConstantRange>& pushConstants ) -> VkResult {
//         // IMPORTANT:
//         // In Vulkan, VkPipelineLayoutCreateInfo::pSetLayouts is an array where each element
//         // corresponds to a descriptor set index in order:
//         //    pSetLayouts[0] -> set = 0
//         //    pSetLayouts[1] -> set = 1
//         //    pSetLayouts[2] -> set = 2
//         // Vulkan does NOT sort or remap them automatically. If the layouts in out.setLayouts
//         // are not in the same order as the shader set indices, you will get validation errors.
//         // For example, if the fragment shader uses set = 1 but out.setLayouts[1] corresponds
//         // to set = 2, Vulkan will complain that the descriptor is missing.
//         // Here we are just filling not used slots with empty descriptor set layouts.
//         // TODO: VK_EXT_Pipeline library extension
//         // [11:59:23] STDERR LOG [thread 67676] Validation Error: Validation Error: [ VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753 ] |
//         // MessageID = 0x57ab6143 | vkCreatePipelineLayout(): pCreateInfo->pSetLayouts[0] is VK_NULL_HANDLE, but VK_EXT_graphics_pipeline_library is not enabled.
//         // The Vulkan spec states: If graphicsPipelineLibrary is not enabled, elements of pSetLayouts must be valid VkDescriptorSetLayout objects
//         // (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753)
//
//         // Find highest set index
//         UInt32 maxSet{ 0 };
//         for ( const auto& setIndex: out.setLayouts | std::views::keys ) {
//             maxSet = std::max(maxSet, setIndex);
//         }
//
//         // Allocate with holes
//         VulkanDevice* vulkanDevice{ TO_VK_DEVICE( RenderService::Get()->GetGpuDevice() ) };
//         DescriptorSetLayoutHandle emptySetLayout{ vulkanDevice->GetDummyDescriptorLayout() };
//
//         std::vector<VkDescriptorSetLayout> setLayouts(maxSet + 1, emptySetLayout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ));
//
//         // Place layouts at correct indices
//         for (const auto& [setIndex, layout] : out.setLayouts) {
//             setLayouts[setIndex] = layout;
//         }
//
//
//         // Add a Layout with no descriptors, not needed because we should not bind an empty Set
//         // for (Size setIndex{}; setIndex < setLayouts.size(); setIndex++) {
//         //     if (out.setLayouts[setIndex] == VK_NULL_HANDLE) {
//         //         out.setLayouts[setIndex] = setLayouts[setIndex];
//         //     }
//         // }
//
//         // Prepare list of sets for the pipeline layout
//         for (const auto &[layoutIndex, setLayout]: out.setLayouts) {
//             setLayouts[layoutIndex] = setLayout;
//         }
//
//         VkPipelineLayoutCreateInfo plInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
//         plInfo.setLayoutCount = static_cast<UInt32>(setLayouts.size());
//         plInfo.pSetLayouts = setLayouts.data();
//
//         plInfo.pushConstantRangeCount = static_cast<UInt32>(pushConstants.size());
//         plInfo.pPushConstantRanges = pushConstants.data();
//
//         if (vkCreatePipelineLayout(device, &plInfo, nullptr, &out.pipelineLayout) != VK_SUCCESS) {
//             return VK_ERROR_INITIALIZATION_FAILED;
//         }
//
//         out.pushConstantRanges = std::move(pushConstants);
//         return VK_SUCCESS;
//     }
//
//     auto ReflectSPIRV( VkDevice device, const std::vector<std::vector<UInt32>>& spirvModules, ReflectedData& out ) -> VkResult {
//         out = {};
//
//         std::vector<VkPushConstantRange> pushConstants{};
//         std::map<UInt32, std::unordered_map<UInt32, VkDescriptorSetLayoutBinding>> sets{};
//
//         for (const auto& moduleData : spirvModules) {
//             if ( moduleData.empty() ) {
//                 MKT_CORE_LOGGER_ERROR( "VulkanHelpers::Reflection::ReflectSPIRV - Empty SPIR-V module data." );
//                 continue;
//             }
//
//             SpvReflectShaderModule mod{};
//             if (spvReflectCreateShaderModule(moduleData.size() * sizeof(UInt32), moduleData.data(), &mod) != SPV_REFLECT_RESULT_SUCCESS) {
//                 return VK_ERROR_INITIALIZATION_FAILED;
//             }
//
//             VkShaderStageFlagBits stage{ static_cast<VkShaderStageFlagBits>( mod.shader_stage ) };
//
//             ProcessDescriptorSets(mod, stage, sets, out);
//             ProcessPushConstants(mod, stage, pushConstants);
//
//             if ( stage == VK_SHADER_STAGE_VERTEX_BIT ) {
//                 ProcessVertexInputs( mod, out );
//             }
//
//             // Cleanup
//             spvReflectDestroyShaderModule(&mod);
//         }
//
//         if ( CreateDescriptorSetLayouts( device, sets, out ) != VK_SUCCESS ) {
//             return VK_ERROR_INITIALIZATION_FAILED;
//         }
//
//         if ( CreatePipelineLayout( device, out, pushConstants ) != VK_SUCCESS ) {
//             return VK_ERROR_INITIALIZATION_FAILED;
//         }
//
//         return VK_SUCCESS;
//     }
//
//     auto DestroyReflectedPipeline( const VkDevice device, ReflectedData& reflected ) -> void {
//         if ( reflected.pipelineLayout ) {
//             vkDestroyPipelineLayout( device, reflected.pipelineLayout, nullptr );
//             reflected.pipelineLayout = VK_NULL_HANDLE;
//         }
//
//         for (const auto &dsLayout: reflected.setLayouts | std::views::values) {
//             vkDestroyDescriptorSetLayout( device, dsLayout, nullptr );
//         }
//
//         reflected.setLayouts.clear();
//         reflected.vertexBindings.clear();
//         reflected.vertexAttributes.clear();
//         reflected.bindingMap.clear();
//         reflected.pushConstantRanges.clear();
//     }
//
// } // namespace Mikoto::VulkanHelpers::Reflection
//
// #endif // VULKAN_HELPERS_CC_INCLUDED