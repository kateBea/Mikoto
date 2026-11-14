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

#include <EASTL/string_view.h>

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    auto GetBlendFactor( rhi::BlendFactor factor ) -> VkBlendFactor {
        MKT_BEGIN_PROFILER();

        switch ( factor ) {
            case rhi::BlendFactor::eZero:
                return VK_BLEND_FACTOR_ZERO;
            case rhi::BlendFactor::eOne:
                return VK_BLEND_FACTOR_ONE;
            case rhi::BlendFactor::eSrcColor:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case rhi::BlendFactor::eOneMinusSrcColor:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case rhi::BlendFactor::eDstColor:
                return VK_BLEND_FACTOR_DST_COLOR;
            case rhi::BlendFactor::eOneMinusDstColor:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case rhi::BlendFactor::eSrcAlpha:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case rhi::BlendFactor::eOneMinusSrcAlpha:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case rhi::BlendFactor::eDstAlpha:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case rhi::BlendFactor::eOneMinusDstAlpha:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case rhi::BlendFactor::eSrcAlphaSaturate:
                return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid blend factor." );
        }
    }

    auto GetBlendOp( rhi::BlendOperation operation ) -> VkBlendOp {
        MKT_BEGIN_PROFILER();

        switch ( operation ) {
            case rhi::BlendOperation::eAdd:
                return VK_BLEND_OP_ADD;
            case rhi::BlendOperation::eSubtract:
                return VK_BLEND_OP_SUBTRACT;
            case rhi::BlendOperation::eReverseSubtract:
                return VK_BLEND_OP_REVERSE_SUBTRACT;
            case rhi::BlendOperation::eMin:
                return VK_BLEND_OP_MIN;
            case rhi::BlendOperation::eMax:
                return VK_BLEND_OP_MAX;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid blend operation." );
        }
    }

    auto GetColorWriteMask( rhi::ColorWriteMask writeMask ) -> VkColorComponentFlags {
        MKT_BEGIN_PROFILER();
        const auto bits{ static_cast<core::u8>( writeMask ) };

        if ( bits > 0x0F ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid color write mask." );
        }

        VkColorComponentFlags mask{};

        if ( bits & 0x01 ) {
            mask |= VK_COLOR_COMPONENT_R_BIT;
        }

        if ( bits & 0x02 ) {
            mask |= VK_COLOR_COMPONENT_G_BIT;
        }

        if ( bits & 0x04 ) {
            mask |= VK_COLOR_COMPONENT_B_BIT;
        }

        if ( bits & 0x08 ) {
            mask |= VK_COLOR_COMPONENT_A_BIT;
        }

        return mask;
    }

    auto GetGpuDeviceType( renderer::rhi::GpuDeviceType type ) -> VkPhysicalDeviceType {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::GpuDeviceType::eDiscrete:
                return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            case renderer::rhi::GpuDeviceType::eIntegrated:
                return VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
            case renderer::rhi::GpuDeviceType::eSoftwareRasterizer:
                return VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
            default:;
        }

        return VK_PHYSICAL_DEVICE_TYPE_OTHER;
    }

    auto GetResultString( VkResult result ) -> const char* {
        MKT_BEGIN_PROFILER();

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

    auto GetFormat( renderer::rhi::Format format ) -> VkFormat {
        MKT_BEGIN_PROFILER();

        switch ( format ) {
            case renderer::rhi::Format::eUnknown:
                return VK_FORMAT_UNDEFINED;

            // 8-bit UNORM
            case renderer::rhi::Format::eR8_UNORM:
                return VK_FORMAT_R8_UNORM;
            case renderer::rhi::Format::eRG8_UNORM:
                return VK_FORMAT_R8G8_UNORM;
            case renderer::rhi::Format::eRGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case renderer::rhi::Format::eBGRA8_UNORM:
                return VK_FORMAT_B8G8R8A8_UNORM;

            // 8-bit SNORM
            case renderer::rhi::Format::eR8_SNORM:
                return VK_FORMAT_R8_SNORM;
            case renderer::rhi::Format::eRG8_SNORM:
                return VK_FORMAT_R8G8_SNORM;
            case renderer::rhi::Format::eRGBA8_SNORM:
                return VK_FORMAT_R8G8B8A8_SNORM;

            // 8-bit UINT / SINT
            case renderer::rhi::Format::eR8_UINT:
                return VK_FORMAT_R8_UINT;
            case renderer::rhi::Format::eR8_SINT:
                return VK_FORMAT_R8_SINT;
            case renderer::rhi::Format::eRG8_UINT:
                return VK_FORMAT_R8G8_UINT;
            case renderer::rhi::Format::eRG8_SINT:
                return VK_FORMAT_R8G8_SINT;
            case renderer::rhi::Format::eRGBA8_UINT:
                return VK_FORMAT_R8G8B8A8_UINT;
            case renderer::rhi::Format::eRGBA8_SINT:
                return VK_FORMAT_R8G8B8A8_SINT;

            // 16-bit UNORM / SNORM
            case renderer::rhi::Format::eR16_UNORM:
                return VK_FORMAT_R16_UNORM;
            case renderer::rhi::Format::eRG16_UNORM:
                return VK_FORMAT_R16G16_UNORM;
            case renderer::rhi::Format::eRGBA16_UNORM:
                return VK_FORMAT_R16G16B16A16_UNORM;

            case renderer::rhi::Format::eR16_SNORM:
                return VK_FORMAT_R16_SNORM;
            case renderer::rhi::Format::eRG16_SNORM:
                return VK_FORMAT_R16G16_SNORM;
            case renderer::rhi::Format::eRGBA16_SNORM:
                return VK_FORMAT_R16G16B16A16_SNORM;

            // 16-bit UINT / SINT
            case renderer::rhi::Format::eR16_UINT:
                return VK_FORMAT_R16_UINT;
            case renderer::rhi::Format::eR16_SINT:
                return VK_FORMAT_R16_SINT;
            case renderer::rhi::Format::eRG16_UINT:
                return VK_FORMAT_R16G16_UINT;
            case renderer::rhi::Format::eRG16_SINT:
                return VK_FORMAT_R16G16_SINT;
            case renderer::rhi::Format::eRGBA16_UINT:
                return VK_FORMAT_R16G16B16A16_UINT;
            case renderer::rhi::Format::eRGBA16_SINT:
                return VK_FORMAT_R16G16B16A16_SINT;

            // 16-bit FLOAT
            case renderer::rhi::Format::eR16_FLOAT:
                return VK_FORMAT_R16_SFLOAT;
            case renderer::rhi::Format::eRG16_FLOAT:
                return VK_FORMAT_R16G16_SFLOAT;
            case renderer::rhi::Format::eRGBA16_FLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;

            // 32-bit FLOAT
            case renderer::rhi::Format::eR32_FLOAT:
                return VK_FORMAT_R32_SFLOAT;
            case renderer::rhi::Format::eRG32_FLOAT:
                return VK_FORMAT_R32G32_SFLOAT;
            case renderer::rhi::Format::eRGB32_FLOAT:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case renderer::rhi::Format::eRGBA32_FLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;

            // 32-bit UINT / SINT
            case renderer::rhi::Format::eR32_UINT:
                return VK_FORMAT_R32_UINT;
            case renderer::rhi::Format::eR32_SINT:
                return VK_FORMAT_R32_SINT;
            case renderer::rhi::Format::eRG32_UINT:
                return VK_FORMAT_R32G32_UINT;
            case renderer::rhi::Format::eRG32_SINT:
                return VK_FORMAT_R32G32_SINT;
            case renderer::rhi::Format::eRGB32_UINT:
                return VK_FORMAT_R32G32B32_UINT;
            case renderer::rhi::Format::eRGB32_SINT:
                return VK_FORMAT_R32G32B32_SINT;
            case renderer::rhi::Format::eRGBA32_UINT:
                return VK_FORMAT_R32G32B32A32_UINT;
            case renderer::rhi::Format::eRGBA32_SINT:
                return VK_FORMAT_R32G32B32A32_SINT;

            // Packed formats
            case renderer::rhi::Format::eR10G10B10A2_UNORM:
                return VK_FORMAT_A2B10G10R10_UNORM_PACK32;

            case renderer::rhi::Format::eR11G11B10_FLOAT:
                return VK_FORMAT_B10G11R11_UFLOAT_PACK32;

            // Depth formats
            case renderer::rhi::Format::eD16:
                return VK_FORMAT_D16_UNORM;
            case renderer::rhi::Format::eD24S8:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case renderer::rhi::Format::eD32:
                return VK_FORMAT_D32_SFLOAT;
            case renderer::rhi::Format::eD32S8:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;

            // BC compressed
            case renderer::rhi::Format::eBC1_UNORM:
                return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            case renderer::rhi::Format::eBC1_UNORM_SRGB:
                return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
            case renderer::rhi::Format::eBC2_UNORM:
                return VK_FORMAT_BC2_UNORM_BLOCK;
            case renderer::rhi::Format::eBC2_UNORM_SRGB:
                return VK_FORMAT_BC2_SRGB_BLOCK;
            case renderer::rhi::Format::eBC3_UNORM:
                return VK_FORMAT_BC3_UNORM_BLOCK;
            case renderer::rhi::Format::eBC3_UNORM_SRGB:
                return VK_FORMAT_BC3_SRGB_BLOCK;
            case renderer::rhi::Format::eBC4_UNORM:
                return VK_FORMAT_BC4_UNORM_BLOCK;
            case renderer::rhi::Format::eBC4_SNORM:
                return VK_FORMAT_BC4_SNORM_BLOCK;
            case renderer::rhi::Format::eBC5_UNORM:
                return VK_FORMAT_BC5_UNORM_BLOCK;
            case renderer::rhi::Format::eBC5_SNORM:
                return VK_FORMAT_BC5_SNORM_BLOCK;
            case renderer::rhi::Format::eBC6H_UFLOAT:
                return VK_FORMAT_BC6H_UFLOAT_BLOCK;
            case renderer::rhi::Format::eBC6H_SFLOAT:
                return VK_FORMAT_BC6H_SFLOAT_BLOCK;
            case renderer::rhi::Format::eBC7_UNORM:
                return VK_FORMAT_BC7_UNORM_BLOCK;
            case renderer::rhi::Format::eBC7_UNORM_SRGB:
                return VK_FORMAT_BC7_SRGB_BLOCK;
            case renderer::rhi::Format::eNV12:
                return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

            case renderer::rhi::Format::eCount:
            default:
                return VK_FORMAT_UNDEFINED;
        }
    }

    auto GetAspectMask( VkFormat format ) -> VkImageAspectFlags {
        MKT_BEGIN_PROFILER();

        switch ( format ) {
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

    auto GetStageMask( renderer::rhi::ResourceStates state ) -> VkPipelineStageFlags2 {
        MKT_BEGIN_PROFILER();

        switch ( state ) {
            case renderer::rhi::ResourceStates::eRenderTarget:
                return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

            case renderer::rhi::ResourceStates::eDepthWrite:
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                       VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

            case renderer::rhi::ResourceStates::eDepthRead:
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                       VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

            case renderer::rhi::ResourceStates::eShaderResource:
                // Shader-readable buffers are also consumed by vertex pulling and ray tracing.
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

            case renderer::rhi::ResourceStates::eUnorderedAccess:
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

            case renderer::rhi::ResourceStates::eIndirectArgument:
                return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;

                // https://github.com/KhronosGroup/Vulkan-Docs/issues/2575
                // VK_PIPELINE_STAGE_2_TRANSFER_BIT is an alias for VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
                // for backwards compatibility

            case renderer::rhi::ResourceStates::eCopySource:
            case renderer::rhi::ResourceStates::eResolveSource:

            case renderer::rhi::ResourceStates::eCopyDest:
            case renderer::rhi::ResourceStates::eResolveDest:
                return VK_PIPELINE_STAGE_2_TRANSFER_BIT;

            case renderer::rhi::ResourceStates::ePresent:
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

            default:
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }
    }

    auto GetStageMask( renderer::rhi::PipelineStageFlags stages ) -> VkPipelineStageFlags2 {
        MKT_BEGIN_PROFILER();
        // All means all queue commands, not every optional extension stage bit.
        if ( stages == renderer::rhi::PipelineStageFlagsBits::All ) {
            return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }

        VkPipelineStageFlags2 result{ VK_PIPELINE_STAGE_2_NONE };

        if ( stages & rhi::PipelineStageFlagsBits::VideoDecode ) {
            result |= VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::Top ) {
            result |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::DrawIndirect ) {
            result |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::VertexInput ) {
            result |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::IndexInput ) {
            result |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::VertexShader ) {
            result |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::HullShader ) {
            result |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::DomainShader ) {
            result |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::GeometryShader ) {
            result |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::PixelShader ) {
            result |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::EarlyFragmentTests ) {
            result |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::LateFragmentTests ) {
            result |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::RenderTarget ) {
            result |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::ComputeShader ) {
            result |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::Transfer ) {
            result |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::Bottom ) {
            result |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::Host ) {
            result |= VK_PIPELINE_STAGE_2_HOST_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::AllGraphics ) {
            result |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::AllCommands ) {
            result |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::Copy ) {
            result |= VK_PIPELINE_STAGE_2_COPY_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::Resolve ) {
            result |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::Blit ) {
            result |= VK_PIPELINE_STAGE_2_BLIT_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::Clear ) {
            result |= VK_PIPELINE_STAGE_2_CLEAR_BIT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::TaskShader ) {
            result |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::MeshShader ) {
            result |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::AccelerationStructureBuild ) {
            result |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        }

        if ( stages & renderer::rhi::PipelineStageFlagsBits::RayTracingShader ) {
            result |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        }

        return result;
    }

    auto GetAccessMask( renderer::rhi::BarrierAccessFlags accesses ) -> VkAccessFlags2 {
        MKT_BEGIN_PROFILER();
        VkAccessFlags2 result{ VK_ACCESS_2_NONE };

        if ( accesses & rhi::BarrierAccessFlagsBits::VideoDecodeRead ) {
            result |= VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR;
        }

        if ( accesses & rhi::BarrierAccessFlagsBits::VideoDecodeWrite ) {
            result |= VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::StructuredBufferRead ) {
            result |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::IndirectCommandRead ) {
            result |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::IndexRead ) {
            result |= VK_ACCESS_2_INDEX_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::VertexAttributeRead ) {
            result |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::ConstantBuffer ) {
            result |= VK_ACCESS_2_UNIFORM_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::ShaderSampledRead ) {
            result |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::ShaderStorageRead ) {
            result |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::ShaderStorageWrite ) {
            result |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::ColorAttachmentRead ) {
            result |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::ColorAttachmentWrite ) {
            result |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::DepthStencilRead ) {
            result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::DepthStencilWrite ) {
            result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::TransferRead ) {
            result |= VK_ACCESS_2_TRANSFER_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::TransferWrite ) {
            result |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::ResolveRead ) {
            result |= VK_ACCESS_2_TRANSFER_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::ResolveWrite ) {
            result |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::HostRead ) {
            result |= VK_ACCESS_2_HOST_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::HostWrite ) {
            result |= VK_ACCESS_2_HOST_WRITE_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::MemoryRead ) {
            result |= VK_ACCESS_2_MEMORY_READ_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::MemoryWrite ) {
            result |= VK_ACCESS_2_MEMORY_WRITE_BIT;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::AccelerationStructureRead ) {
            result |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::AccelerationStructureWrite ) {
            result |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        }

        if ( accesses & renderer::rhi::BarrierAccessFlagsBits::AccelerationStructureBuildInputRead ) {
            result |= VK_ACCESS_2_SHADER_READ_BIT;
        }

        return result;
    }

    auto GetMipmapMode( rhi::SamplerMipmapMode mode ) -> VkSamplerMipmapMode {
        MKT_BEGIN_PROFILER();

        if ( mode == rhi::SamplerMipmapMode::eNearest ) {
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }

        if ( mode == rhi::SamplerMipmapMode::eLinear ) {
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }

        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    auto GetAccessMask( renderer::rhi::ResourceStates state ) -> VkAccessFlags2 {
        MKT_BEGIN_PROFILER();

        switch ( state ) {
            case renderer::rhi::ResourceStates::eRenderTarget:
                return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

            case renderer::rhi::ResourceStates::eDepthWrite:
                return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                       VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            case renderer::rhi::ResourceStates::eDepthRead:
                return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

            case renderer::rhi::ResourceStates::eAccelStructBuildInput:
                return VK_ACCESS_2_SHADER_READ_BIT;
            case renderer::rhi::ResourceStates::eAccelStructRead:
                return VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            case renderer::rhi::ResourceStates::eAccelStructWrite:
            case renderer::rhi::ResourceStates::eAccelStructBuildBlas:
                return VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            case renderer::rhi::ResourceStates::eShaderResource:
                return VK_ACCESS_2_SHADER_READ_BIT;

            case renderer::rhi::ResourceStates::eUnorderedAccess:
                return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

            case renderer::rhi::ResourceStates::eIndirectArgument:
                return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

            case renderer::rhi::ResourceStates::eCopySource:
            case renderer::rhi::ResourceStates::eResolveSource:
                return VK_ACCESS_2_TRANSFER_READ_BIT;

            case renderer::rhi::ResourceStates::eCopyDest:
            case renderer::rhi::ResourceStates::eResolveDest:
                return VK_ACCESS_2_TRANSFER_WRITE_BIT;

            case renderer::rhi::ResourceStates::ePresent:
                // Presentation is synchronized by acquire/present semaphores;
                // PRESENT_SRC_KHR has no pipeline memory-access scope.
                return VK_ACCESS_2_NONE;

            default:
                return 0;
        }
    }

    auto GetIndexType( renderer::rhi::Format format ) -> VkIndexType {
        MKT_BEGIN_PROFILER();

        switch ( format ) {
            case renderer::rhi::Format::eR16_UINT:
                return VK_INDEX_TYPE_UINT16;

            case renderer::rhi::Format::eR32_UINT:
                return VK_INDEX_TYPE_UINT32;

#if defined( VK_EXT_index_type_uint8 ) || defined( VK_VERSION_1_3 )
            case renderer::rhi::Format::eR8_UINT:
                return VK_INDEX_TYPE_UINT8_EXT;
#endif

            default:
                // You should never hit this if your API is correct
                MKT_ASSERT( false, "Invalid format for index buffer" );
                return VK_INDEX_TYPE_UINT32;
        }
    }

    auto GetPolygonMode( renderer::rhi::PolygonMode mode ) -> VkPolygonMode {
        MKT_BEGIN_PROFILER();

        switch ( mode ) {
            case renderer::rhi::PolygonMode::eLines:
                return VK_POLYGON_MODE_LINE;
            case renderer::rhi::PolygonMode::ePoint:
                return VK_POLYGON_MODE_POINT;
            case renderer::rhi::PolygonMode::eFill:
                return VK_POLYGON_MODE_FILL;
            default:;
        }

        return VK_POLYGON_MODE_MAX_ENUM;
    }

    auto GetAspectMask( renderer::rhi::Format format ) -> VkImageAspectFlags {
        MKT_BEGIN_PROFILER();

        switch ( format ) {
            case renderer::rhi::Format::eD16:
            case renderer::rhi::Format::eD32:
                return VK_IMAGE_ASPECT_DEPTH_BIT;

            case renderer::rhi::Format::eD24S8:
            case renderer::rhi::Format::eD32S8:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

            case renderer::rhi::Format::eX24G8_UINT:
            case renderer::rhi::Format::eX32G8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;

            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    auto GetShaderModuleStage( renderer::rhi::ShaderType stage ) -> VkShaderStageFlagBits {
        MKT_BEGIN_PROFILER();

        switch ( stage ) {
            case renderer::rhi::ShaderType::eVertex:
                return VK_SHADER_STAGE_VERTEX_BIT;

            case renderer::rhi::ShaderType::ePixel:
                return VK_SHADER_STAGE_FRAGMENT_BIT;

            case renderer::rhi::ShaderType::eCompute:
                return VK_SHADER_STAGE_COMPUTE_BIT;

            case renderer::rhi::ShaderType::eGeometry:
                return VK_SHADER_STAGE_GEOMETRY_BIT;

            case renderer::rhi::ShaderType::eDomain:
                return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

            case renderer::rhi::ShaderType::eHull:
                return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

            case renderer::rhi::ShaderType::eRayGeneration:
                return VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            case renderer::rhi::ShaderType::eMiss:
                return VK_SHADER_STAGE_MISS_BIT_KHR;

            case renderer::rhi::ShaderType::eClosestHit:
                return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            default:
                return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        }
    }

    auto GetShaderModuleStage( VkShaderStageFlagBits stage ) -> renderer::rhi::ShaderType {
        MKT_BEGIN_PROFILER();

        switch ( stage ) {
            case VK_SHADER_STAGE_VERTEX_BIT:
                return renderer::rhi::ShaderType::eVertex;

            case VK_SHADER_STAGE_FRAGMENT_BIT:
                return renderer::rhi::ShaderType::ePixel;

            case VK_SHADER_STAGE_COMPUTE_BIT:
                return renderer::rhi::ShaderType::eCompute;

            case VK_SHADER_STAGE_GEOMETRY_BIT:
                return renderer::rhi::ShaderType::eGeometry;

            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                return renderer::rhi::ShaderType::eHull;

            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                return renderer::rhi::ShaderType::eDomain;

            case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
                return renderer::rhi::ShaderType::eRayGeneration;

            case VK_SHADER_STAGE_MISS_BIT_KHR:
                return renderer::rhi::ShaderType::eMiss;

            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                return renderer::rhi::ShaderType::eClosestHit;

            default:
                return renderer::rhi::ShaderType::eInvalid;
        }
    }

    auto GetCompareOp( renderer::rhi::CompareOp op ) -> VkCompareOp {
        MKT_BEGIN_PROFILER();

        switch ( op ) {
            case renderer::rhi::CompareOp::eNever:
                return VK_COMPARE_OP_NEVER;

            case renderer::rhi::CompareOp::eLess:
                return VK_COMPARE_OP_LESS;

            case renderer::rhi::CompareOp::eEqual:
                return VK_COMPARE_OP_EQUAL;

            case renderer::rhi::CompareOp::eLessOrEqual:
                return VK_COMPARE_OP_LESS_OR_EQUAL;

            case renderer::rhi::CompareOp::eGreater:
                return VK_COMPARE_OP_GREATER;

            case renderer::rhi::CompareOp::eNotEqual:
                return VK_COMPARE_OP_NOT_EQUAL;

            case renderer::rhi::CompareOp::eGreaterOrEqual:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;

            case renderer::rhi::CompareOp::eAlways:
                return VK_COMPARE_OP_ALWAYS;

            default:
                return VK_COMPARE_OP_ALWAYS;
        }
    }

    auto GetInputRate( renderer::rhi::InputRate rate ) -> VkVertexInputRate {
        MKT_BEGIN_PROFILER();

        switch ( rate ) {
            case renderer::rhi::InputRate::ePerVertex:
                return VK_VERTEX_INPUT_RATE_VERTEX;
            case renderer::rhi::InputRate::ePerInstance:
                return VK_VERTEX_INPUT_RATE_INSTANCE;
            default:
                return VK_VERTEX_INPUT_RATE_VERTEX;// fallback
        }
    }

    auto GetCullMode( renderer::rhi::CullMode mode ) -> VkCullModeFlags {
        MKT_BEGIN_PROFILER();

        switch ( mode ) {
            case renderer::rhi::CullMode::eNone:
                return VK_CULL_MODE_NONE;
            case renderer::rhi::CullMode::eCullFront:
                return VK_CULL_MODE_FRONT_BIT;
            case renderer::rhi::CullMode::eCullBack:
                return VK_CULL_MODE_FRONT_AND_BACK;
            default:;
        }

        return VK_CULL_MODE_NONE;
    }

    auto GetWindingOrder( renderer::rhi::WindingOrder order ) -> VkFrontFace {
        MKT_BEGIN_PROFILER();

        switch ( order ) {
            case renderer::rhi::WindingOrder::eClockwise:
                return VK_FRONT_FACE_CLOCKWISE;
            case renderer::rhi::WindingOrder::eCounterClockwise:
                return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        }

        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    auto GetSampleCount( renderer::rhi::Multisampling msaa ) -> VkSampleCountFlagBits {
        MKT_BEGIN_PROFILER();

        switch ( msaa ) {
            case renderer::rhi::Multisampling::eMsaaX1:
                return VK_SAMPLE_COUNT_1_BIT;

            case renderer::rhi::Multisampling::eMsaaX2:
                return VK_SAMPLE_COUNT_2_BIT;

            case renderer::rhi::Multisampling::eMsaaX4:
                return VK_SAMPLE_COUNT_4_BIT;

            case renderer::rhi::Multisampling::eMsaaX8:
                return VK_SAMPLE_COUNT_8_BIT;

            case renderer::rhi::Multisampling::eMsaaX16:
                return VK_SAMPLE_COUNT_16_BIT;

            default:
                return VK_SAMPLE_COUNT_1_BIT;
        }
    }

    auto GetTopology( renderer::rhi::PrimitiveTopology topology ) -> VkPrimitiveTopology {
        MKT_BEGIN_PROFILER();

        switch ( topology ) {
            case renderer::rhi::PrimitiveTopology::ePointList:
                return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

            case renderer::rhi::PrimitiveTopology::eLineList:
                return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

            case renderer::rhi::PrimitiveTopology::eLineStrip:
                return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;

            case renderer::rhi::PrimitiveTopology::eTriangleList:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            case renderer::rhi::PrimitiveTopology::eTriangleStrip:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

            case renderer::rhi::PrimitiveTopology::eTriangleFan:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

            default:
                return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
        }
    }

    auto GetDescriptorType( renderer::rhi::ResourceType type ) -> VkDescriptorType {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ResourceType::eTexture_SRV:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

            case renderer::rhi::ResourceType::eTexture_UAV:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

            case renderer::rhi::ResourceType::eTypedBuffer_SRV:
                return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

            case renderer::rhi::ResourceType::eTypedBuffer_UAV:
                return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

            case renderer::rhi::ResourceType::eStructuredBuffer_SRV:
            case renderer::rhi::ResourceType::eRawBuffer_SRV:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            case renderer::rhi::ResourceType::eStructuredBuffer_UAV:
            case renderer::rhi::ResourceType::eRawBuffer_UAV:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            case renderer::rhi::ResourceType::eConstantBuffer:
            case renderer::rhi::ResourceType::eVolatileConstantBuffer:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

            case renderer::rhi::ResourceType::eSampler:
                return VK_DESCRIPTOR_TYPE_SAMPLER;

            case renderer::rhi::ResourceType::eRayTracingAccelStruct:
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

            case renderer::rhi::ResourceType::ePushConstants:
            case renderer::rhi::ResourceType::eInvalid:
            case renderer::rhi::ResourceType::eSamplerFeedbackTexture_UAV:
            default:
                MKT_ASSERT( false, "Invalid or unsupported ResourceType for descriptor" );
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    auto GetArraLayerCount( renderer::rhi::TextureDimension dimension, core::u32 requestedLayers ) -> core::u32 {
        MKT_BEGIN_PROFILER();

        switch ( dimension ) {
            case renderer::rhi::TextureDimension::eTexture1D:
            case renderer::rhi::TextureDimension::eTexture2D:
            case renderer::rhi::TextureDimension::eTexture2DMS:
            case renderer::rhi::TextureDimension::eTexture3D:
                return 1;

            case renderer::rhi::TextureDimension::eTextureCube:
                return 6;

            case renderer::rhi::TextureDimension::eTexture1DArray:
            case renderer::rhi::TextureDimension::eTexture2DArray:
            case renderer::rhi::TextureDimension::eTexture2DMSArray:
                return requestedLayers;

            case renderer::rhi::TextureDimension::eTextureCubeArray:
                // Must be a multiple of 6 for cubemap arrays
                return requestedLayers * 6;

            case renderer::rhi::TextureDimension::eInvalid:
            default:
                return 0;
        }
    }

    auto GetImageUsage( renderer::rhi::TextureUsageFlags flags ) -> VkImageUsageFlags {
        MKT_BEGIN_PROFILER();
        VkImageUsageFlags result{};

        if ( flags & renderer::rhi::TextureUsageFlagsBits::UnorderedAccess ) {
            result |= VK_IMAGE_USAGE_STORAGE_BIT;
        }

        if ( flags & renderer::rhi::TextureUsageFlagsBits::RenderTarget ) {
            result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        if ( flags & renderer::rhi::TextureUsageFlagsBits::DepthTarget ) {
            result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }

        if ( flags & renderer::rhi::TextureUsageFlagsBits::ShaderResource ) {
            result |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }

        if ( flags & renderer::rhi::TextureUsageFlagsBits::CopySource ) {
            result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        if ( flags & renderer::rhi::TextureUsageFlagsBits::CopyDest ) {
            result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        return result;
    }

    auto GetSamplerFilter( renderer::rhi::SamplerFilter filter ) -> VkFilter {
        MKT_BEGIN_PROFILER();

        switch ( filter ) {
            case renderer::rhi::SamplerFilter::eNearest:
                return VK_FILTER_NEAREST;
            case renderer::rhi::SamplerFilter::eLinear:
                return VK_FILTER_LINEAR;
            default:
                // Empty
                ;
        }

        return VK_FILTER_NEAREST;
    }

    auto GetSamplerWrap( renderer::rhi::SamplerWrapMode wrap ) -> VkSamplerAddressMode {
        MKT_BEGIN_PROFILER();

        switch ( wrap ) {
            case renderer::rhi::SamplerWrapMode::eClampToEdge:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case renderer::rhi::SamplerWrapMode::eClampToBorder:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case renderer::rhi::SamplerWrapMode::eRepeat:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            default:
                // Empty
                ;
        }

        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    auto GetQueueName( renderer::rhi::QueueType type ) -> eastl::string_view {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::QueueType::eGraphics:
                return "Graphics";
            case renderer::rhi::QueueType::eCompute:
                return "Compute";
            case renderer::rhi::QueueType::eTransfer:
                return "Transfer";
            case renderer::rhi::QueueType::ePresent:
                return "Present";
            case renderer::rhi::QueueType::eVideoDecode:
                return "Video decode";
            default:;
        }

        return eastl::string_view{};
    }

    auto GetViewType( renderer::rhi::TextureDimension dimensions ) -> VkImageViewType {
        MKT_BEGIN_PROFILER();

        switch ( dimensions ) {
            case renderer::rhi::TextureDimension::eTexture1D:
                return VK_IMAGE_VIEW_TYPE_1D;

            case renderer::rhi::TextureDimension::eTextureCube:
                return VK_IMAGE_VIEW_TYPE_CUBE;

            case renderer::rhi::TextureDimension::eTexture2D:
                return VK_IMAGE_VIEW_TYPE_2D;

            case renderer::rhi::TextureDimension::eTexture3D:
                return VK_IMAGE_VIEW_TYPE_3D;

            case renderer::rhi::TextureDimension::eTexture1DArray:
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;

            case renderer::rhi::TextureDimension::eTexture2DArray:
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;

            case renderer::rhi::TextureDimension::eTextureCubeArray:
                return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            default:;
        }

        MKT_ASSERT( false, "Unknown TextureDimension" );
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    auto GetTextureType( renderer::rhi::TextureDimension dimensions ) -> VkImageType {
        MKT_BEGIN_PROFILER();

        switch ( dimensions ) {
            case renderer::rhi::TextureDimension::eTexture1D:
            case renderer::rhi::TextureDimension::eTexture1DArray:
                return VK_IMAGE_TYPE_1D;

            case renderer::rhi::TextureDimension::eTexture2D:
            case renderer::rhi::TextureDimension::eTexture2DArray:
            case renderer::rhi::TextureDimension::eTextureCube:
            case renderer::rhi::TextureDimension::eTextureCubeArray:
            case renderer::rhi::TextureDimension::eTexture2DMS:
            case renderer::rhi::TextureDimension::eTexture2DMSArray:
                return VK_IMAGE_TYPE_2D;

            case renderer::rhi::TextureDimension::eTexture3D:
                return VK_IMAGE_TYPE_3D;
            default:;
        }

        return VK_IMAGE_TYPE_2D;
    }

    auto GetImageLayout( renderer::rhi::ResourceStates state ) -> VkImageLayout {
        MKT_BEGIN_PROFILER();

        switch ( state ) {
            case rhi::ResourceStates::eVideoDecodeOutput:
                return VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR;
            case rhi::ResourceStates::eVideoDecodeReference:
                return VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
            case renderer::rhi::ResourceStates::eUnknown:
                return VK_IMAGE_LAYOUT_UNDEFINED;

            case renderer::rhi::ResourceStates::eCommon:
                return VK_IMAGE_LAYOUT_GENERAL;

            case renderer::rhi::ResourceStates::eRenderTarget:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            case renderer::rhi::ResourceStates::eDepthWrite:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            case renderer::rhi::ResourceStates::eDepthRead:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            case renderer::rhi::ResourceStates::eShaderResource:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            case renderer::rhi::ResourceStates::eUnorderedAccess:
                return VK_IMAGE_LAYOUT_GENERAL;

            case renderer::rhi::ResourceStates::eCopyDest:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            case renderer::rhi::ResourceStates::eCopySource:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            case renderer::rhi::ResourceStates::eResolveDest:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            case renderer::rhi::ResourceStates::eResolveSource:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            case renderer::rhi::ResourceStates::ePresent:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            case renderer::rhi::ResourceStates::eConstantBuffer:
            case renderer::rhi::ResourceStates::eVertexBuffer:
            case renderer::rhi::ResourceStates::eIndexBuffer:
            case renderer::rhi::ResourceStates::eIndirectArgument:
                return VK_IMAGE_LAYOUT_UNDEFINED;

            case renderer::rhi::ResourceStates::eAccelStructRead:
            case renderer::rhi::ResourceStates::eAccelStructWrite:
            case renderer::rhi::ResourceStates::eAccelStructBuildInput:
            case renderer::rhi::ResourceStates::eAccelStructBuildBlas:
                return VK_IMAGE_LAYOUT_GENERAL;
        }

        MKT_ASSERT( false, "Unhandled ResourceState" );
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    auto GetImageLayout( rhi::TextureLayoutFlags layout ) -> VkImageLayout {
        if ( layout == rhi::TextureLayoutBits::VideoDecodeOutput ) {
            return VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR;
        }

        if ( layout == rhi::TextureLayoutBits::VideoDecodeReference ) {
            return VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::Unknown ) {
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::Present ) {
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::RenderTarget ) {
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::ShaderResource ) {
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::DepthStencil ) {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::DepthStencilReadOnly ) {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::General || layout == renderer::rhi::TextureLayoutBits::UnorderedAccess ) {
            return VK_IMAGE_LAYOUT_GENERAL;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::CopyDest || layout == renderer::rhi::TextureLayoutBits::ResolveDest ) {
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }

        if ( layout == renderer::rhi::TextureLayoutBits::CopySource || layout == renderer::rhi::TextureLayoutBits::ResolveSource ) {
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }

        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    auto GetResourceState( VkImageLayout layout ) -> renderer::rhi::ResourceStates {
        MKT_BEGIN_PROFILER();

        switch ( layout ) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                return renderer::rhi::ResourceStates::eUnknown;

            case VK_IMAGE_LAYOUT_GENERAL:
                // Could be UAV, ray tracing, or "common"
                return renderer::rhi::ResourceStates::eUnorderedAccess;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return renderer::rhi::ResourceStates::eRenderTarget;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return renderer::rhi::ResourceStates::eDepthWrite;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                return renderer::rhi::ResourceStates::eDepthRead;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return renderer::rhi::ResourceStates::eShaderResource;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return renderer::rhi::ResourceStates::eCopyDest;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return renderer::rhi::ResourceStates::eCopySource;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                return renderer::rhi::ResourceStates::ePresent;

            case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:
                return renderer::rhi::ResourceStates::eVideoDecodeOutput;

            case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR:
                return renderer::rhi::ResourceStates::eVideoDecodeReference;

            default:
                MKT_ASSERT( false, "Unhandled VkImageLayout" );
                return renderer::rhi::ResourceStates::eUnknown;
        }
    }

    auto GetShaderStageFlags( renderer::rhi::ShaderFlags visibility ) -> VkShaderStageFlags {
        MKT_BEGIN_PROFILER();
        // Fast path
        if ( visibility == renderer::rhi::ShaderFlagsBits::All ) {
            return VK_SHADER_STAGE_ALL;
        }

        VkShaderStageFlags flags{};

        if ( visibility & renderer::rhi::ShaderFlagsBits::Vertex ) {
            flags |= VK_SHADER_STAGE_VERTEX_BIT;
        }

        if ( visibility & renderer::rhi::ShaderFlagsBits::Pixel ) {
            flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        if ( visibility & renderer::rhi::ShaderFlagsBits::Compute ) {
            flags |= VK_SHADER_STAGE_COMPUTE_BIT;
        }

        if ( visibility & renderer::rhi::ShaderFlagsBits::Geometry ) {
            flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        }

        if ( visibility & renderer::rhi::ShaderFlagsBits::Hull ) {
            flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }

        if ( visibility & renderer::rhi::ShaderFlagsBits::RayGeneration ) {
            flags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        }

        if ( visibility & renderer::rhi::ShaderFlagsBits::Miss ) {
            flags |= VK_SHADER_STAGE_MISS_BIT_KHR;
        }

        if ( visibility & renderer::rhi::ShaderFlagsBits::ClosestHit ) {
            flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        }

        return flags;
    }

    // ==================================================================================================================
    // Initializers
    // ==================================================================================================================

    auto initializers::ApplicationInfo() -> VkApplicationInfo {
        MKT_BEGIN_PROFILER();
        VkApplicationInfo ret{ VK_STRUCTURE_TYPE_APPLICATION_INFO };

        return ret;
    }

    auto initializers::DebugUtilsMessengerCreateInfoEXT() -> VkDebugUtilsMessengerCreateInfoEXT {
        MKT_BEGIN_PROFILER();
        VkDebugUtilsMessengerCreateInfoEXT ret{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        return ret;
    }

    auto initializers::DynamicRenderingFeature() -> VkPhysicalDeviceDynamicRenderingFeatures {
        MKT_BEGIN_PROFILER();
        VkPhysicalDeviceDynamicRenderingFeatures ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
        return ret;
    }

    auto initializers::PhysicalDeviceFeatures2() -> VkPhysicalDeviceFeatures2 {
        MKT_BEGIN_PROFILER();
        VkPhysicalDeviceFeatures2 ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        return ret;
    }

    auto initializers::DeviceCreateInfo() -> VkDeviceCreateInfo {
        MKT_BEGIN_PROFILER();
        VkDeviceCreateInfo ret{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        return ret;
    }

    auto initializers::PhysicalDeviceVulkan13Features() -> VkPhysicalDeviceVulkan13Features {
        MKT_BEGIN_PROFILER();
        VkPhysicalDeviceVulkan13Features ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        return ret;
    }

    auto initializers::PhysicalDeviceVulkan12Features() -> VkPhysicalDeviceVulkan12Features {
        MKT_BEGIN_PROFILER();
        VkPhysicalDeviceVulkan12Features ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        return ret;
    }

    auto initializers::PhysicalDeviceVulkan11Features() -> VkPhysicalDeviceVulkan11Features {
        MKT_BEGIN_PROFILER();
        VkPhysicalDeviceVulkan11Features ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
        return ret;
    }

    auto initializers::DeviceQueueCreateInfo() -> VkDeviceQueueCreateInfo {
        MKT_BEGIN_PROFILER();
        VkDeviceQueueCreateInfo ret{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        return ret;
    }

    auto initializers::DescriptorPoolCreateInfo() -> VkDescriptorPoolCreateInfo {
        MKT_BEGIN_PROFILER();
        VkDescriptorPoolCreateInfo ret{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        return ret;
    }

    auto initializers::SwapchainCreateInfoKHR() -> VkSwapchainCreateInfoKHR {
        MKT_BEGIN_PROFILER();
        VkSwapchainCreateInfoKHR ret{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        return ret;
    }

    auto initializers::ImageViewCreateInfo() -> VkImageViewCreateInfo {
        MKT_BEGIN_PROFILER();
        VkImageViewCreateInfo ret{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        return ret;
    }

    auto initializers::RenderingAttachmentInfo() -> VkRenderingAttachmentInfo {
        MKT_BEGIN_PROFILER();
        VkRenderingAttachmentInfo ret{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        return ret;
    }

    auto initializers::PresentInfoKHR() -> VkPresentInfoKHR {
        MKT_BEGIN_PROFILER();
        VkPresentInfoKHR ret{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        return ret;
    }

    auto initializers::SamplerCreateInfo() -> VkSamplerCreateInfo {
        MKT_BEGIN_PROFILER();
        VkSamplerCreateInfo ret{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        return ret;
    }

    auto initializers::ImageCreateInfo() -> VkImageCreateInfo {
        MKT_BEGIN_PROFILER();
        VkImageCreateInfo ret{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        return ret;
    }

    auto initializers::RenderingInfo() -> VkRenderingInfo {
        MKT_BEGIN_PROFILER();
        VkRenderingInfo ret{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        return ret;
    }

    auto initializers::CommandBufferBeginInfo() -> VkCommandBufferBeginInfo {
        MKT_BEGIN_PROFILER();
        VkCommandBufferBeginInfo ret{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        return ret;
    }

    auto initializers::SemaphoreTypeCreateInfo() -> VkSemaphoreTypeCreateInfo {
        MKT_BEGIN_PROFILER();
        VkSemaphoreTypeCreateInfo ret{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        };
        return ret;
    }

    auto initializers::ImageBlit2() -> VkImageBlit2 {
        MKT_BEGIN_PROFILER();
        VkImageBlit2 ret{ VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
        return ret;
    }

    auto initializers::ImageCopy2() -> VkImageCopy2 {
        MKT_BEGIN_PROFILER();
        VkImageCopy2 ret{ VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
        return ret;
    }

    auto initializers::ShaderModuleCreateInfo() -> VkShaderModuleCreateInfo {
        MKT_BEGIN_PROFILER();
        VkShaderModuleCreateInfo ret{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        return ret;
    }

    auto initializers::PipelineShaderStageCreateInfo() -> VkPipelineShaderStageCreateInfo {
        MKT_BEGIN_PROFILER();
        VkPipelineShaderStageCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        return ret;
    }

    auto initializers::FramebufferCreateInfo() -> VkFramebufferCreateInfo {
        MKT_BEGIN_PROFILER();
        VkFramebufferCreateInfo ret{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        return ret;
    }

    auto initializers::PipelineRenderingCreateInfo() -> VkPipelineRenderingCreateInfo {
        MKT_BEGIN_PROFILER();
        VkPipelineRenderingCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        return ret;
    }

    auto initializers::GraphicsPipelineCreateInfo() -> VkGraphicsPipelineCreateInfo {
        MKT_BEGIN_PROFILER();
        VkGraphicsPipelineCreateInfo ret{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        return ret;
    }

    auto initializers::PipelineVertexInputStateCreateInfo() -> VkPipelineVertexInputStateCreateInfo {
        MKT_BEGIN_PROFILER();
        VkPipelineVertexInputStateCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        return ret;
    }

    auto initializers::PipelineLayoutCreateInfo() -> VkPipelineLayoutCreateInfo {
        MKT_BEGIN_PROFILER();
        VkPipelineLayoutCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        return ret;
    }

    auto initializers::ComputePipelineCreateInfo() -> VkComputePipelineCreateInfo {
        MKT_BEGIN_PROFILER();
        VkComputePipelineCreateInfo ret{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        return ret;
    }

    auto initializers::BufferCreateInfo() -> VkBufferCreateInfo {
        MKT_BEGIN_PROFILER();
        VkBufferCreateInfo ret{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        return ret;
    }

    auto initializers::WriteDescriptorSet() -> VkWriteDescriptorSet {
        MKT_BEGIN_PROFILER();
        VkWriteDescriptorSet ret{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        return ret;
    }

    auto initializers::PipelineCacheCreateInfo() -> VkPipelineCacheCreateInfo {
        MKT_BEGIN_PROFILER();
        VkPipelineCacheCreateInfo ret{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
        return ret;
    }

    auto initializers::CommandPoolCreateInfo() -> VkCommandPoolCreateInfo {
        MKT_BEGIN_PROFILER();
        VkCommandPoolCreateInfo ret{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        return ret;
    }

    auto initializers::FenceCreateInfo( VkFenceCreateFlags flags ) -> VkFenceCreateInfo {
        MKT_BEGIN_PROFILER();
        VkFenceCreateInfo ret{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = flags,
        };

        return ret;
    }

    auto initializers::CommandBufferAllocateInfo() -> VkCommandBufferAllocateInfo {
        MKT_BEGIN_PROFILER();
        VkCommandBufferAllocateInfo ret{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

        return ret;
    }

    auto initializers::InstanceCreateInfo() -> VkInstanceCreateInfo {
        MKT_BEGIN_PROFILER();
        VkInstanceCreateInfo ret{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };

        return ret;
    }

    auto initializers::SemaphoreCreateInfo() -> VkSemaphoreCreateInfo {
        MKT_BEGIN_PROFILER();
        VkSemaphoreCreateInfo ret{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        return ret;
    }

    // Allocate image memory and describe compatible views.
    auto initializers::ImageFormatListCreateInfo() -> VkImageFormatListCreateInfo {
        MKT_BEGIN_PROFILER();
        VkImageFormatListCreateInfo ret{ VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO };
        return ret;
    }

    auto initializers::MemoryDedicatedAllocateInfo() -> VkMemoryDedicatedAllocateInfo {
        MKT_BEGIN_PROFILER();
        VkMemoryDedicatedAllocateInfo ret{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
        return ret;
    }

    auto initializers::MemoryAllocateInfo() -> VkMemoryAllocateInfo {
        MKT_BEGIN_PROFILER();
        VkMemoryAllocateInfo ret{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        return ret;
    }

    auto initializers::ImageViewUsageCreateInfo() -> VkImageViewUsageCreateInfo {
        MKT_BEGIN_PROFILER();
        VkImageViewUsageCreateInfo ret{ VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
        return ret;
    }

    // Describe video profiles and query device support.
    auto initializers::QueueFamilyProperties2() -> VkQueueFamilyProperties2 {
        MKT_BEGIN_PROFILER();
        VkQueueFamilyProperties2 ret{ VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 };
        return ret;
    }

    auto initializers::QueueFamilyVideoPropertiesKHR() -> VkQueueFamilyVideoPropertiesKHR {
        MKT_BEGIN_PROFILER();
        VkQueueFamilyVideoPropertiesKHR ret{ VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR };
        return ret;
    }

    auto initializers::VideoProfileListInfoKHR() -> VkVideoProfileListInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoProfileListInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR };
        return ret;
    }

    auto initializers::PhysicalDeviceVideoFormatInfoKHR() -> VkPhysicalDeviceVideoFormatInfoKHR {
        MKT_BEGIN_PROFILER();
        VkPhysicalDeviceVideoFormatInfoKHR ret{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR };
        return ret;
    }

    auto initializers::VideoFormatPropertiesKHR() -> VkVideoFormatPropertiesKHR {
        MKT_BEGIN_PROFILER();
        VkVideoFormatPropertiesKHR ret{ VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR };
        return ret;
    }

    auto initializers::VideoDecodeH264ProfileInfoKHR() -> VkVideoDecodeH264ProfileInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoDecodeH264ProfileInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR };
        return ret;
    }

    auto initializers::VideoProfileInfoKHR() -> VkVideoProfileInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoProfileInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR };
        return ret;
    }

    auto initializers::VideoDecodeH264CapabilitiesKHR() -> VkVideoDecodeH264CapabilitiesKHR {
        MKT_BEGIN_PROFILER();
        VkVideoDecodeH264CapabilitiesKHR ret{ VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR };
        return ret;
    }

    auto initializers::VideoDecodeCapabilitiesKHR() -> VkVideoDecodeCapabilitiesKHR {
        MKT_BEGIN_PROFILER();
        VkVideoDecodeCapabilitiesKHR ret{ VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR };
        return ret;
    }

    auto initializers::VideoCapabilitiesKHR() -> VkVideoCapabilitiesKHR {
        MKT_BEGIN_PROFILER();
        VkVideoCapabilitiesKHR ret{ VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR };
        return ret;
    }

    // Create video sessions and their parameter bindings.
    auto initializers::VideoSessionCreateInfoKHR() -> VkVideoSessionCreateInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoSessionCreateInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR };
        return ret;
    }

    auto initializers::VideoSessionMemoryRequirementsKHR() -> VkVideoSessionMemoryRequirementsKHR {
        MKT_BEGIN_PROFILER();
        VkVideoSessionMemoryRequirementsKHR ret{ VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR };
        return ret;
    }

    auto initializers::BindVideoSessionMemoryInfoKHR() -> VkBindVideoSessionMemoryInfoKHR {
        MKT_BEGIN_PROFILER();
        VkBindVideoSessionMemoryInfoKHR ret{ VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR };
        return ret;
    }

    auto initializers::VideoDecodeH264SessionParametersAddInfoKHR() -> VkVideoDecodeH264SessionParametersAddInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoDecodeH264SessionParametersAddInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR };
        return ret;
    }

    auto initializers::VideoDecodeH264SessionParametersCreateInfoKHR() -> VkVideoDecodeH264SessionParametersCreateInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoDecodeH264SessionParametersCreateInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR };
        return ret;
    }

    auto initializers::VideoSessionParametersCreateInfoKHR() -> VkVideoSessionParametersCreateInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoSessionParametersCreateInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR };
        return ret;
    }

    // Record video pictures and coding scopes.
    auto initializers::VideoPictureResourceInfoKHR() -> VkVideoPictureResourceInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoPictureResourceInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
        return ret;
    }

    auto initializers::VideoDecodeH264DpbSlotInfoKHR() -> VkVideoDecodeH264DpbSlotInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoDecodeH264DpbSlotInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR };
        return ret;
    }

    auto initializers::VideoReferenceSlotInfoKHR() -> VkVideoReferenceSlotInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoReferenceSlotInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR };
        return ret;
    }

    auto initializers::VideoBeginCodingInfoKHR() -> VkVideoBeginCodingInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoBeginCodingInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR };
        return ret;
    }

    auto initializers::VideoCodingControlInfoKHR() -> VkVideoCodingControlInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoCodingControlInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR };
        return ret;
    }

    auto initializers::VideoDecodeH264PictureInfoKHR() -> VkVideoDecodeH264PictureInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoDecodeH264PictureInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR };
        return ret;
    }

    auto initializers::VideoDecodeInfoKHR() -> VkVideoDecodeInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoDecodeInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR };
        return ret;
    }

    auto initializers::VideoEndCodingInfoKHR() -> VkVideoEndCodingInfoKHR {
        MKT_BEGIN_PROFILER();
        VkVideoEndCodingInfoKHR ret{ VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR };
        return ret;
    }

}// namespace mikoto::renderer::vulkan
