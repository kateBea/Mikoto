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

#include <exception>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    auto ThrowIfFailed( HRESULT hr ) -> void {
        if (FAILED(hr)) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "D3D12 Error: {}", GetResultLabel( hr ) ) );
        }
    }

    auto ThrowIfFailed( HRESULT hr, IGpuDevice *gpuDevice ) -> void {
        if (FAILED(hr)) {
            Device* d3d12Device{ checked_cast<Device*>( gpuDevice ) };
            d3d12Device->DumpMessages();
            MKT_THROW_RUNTIME_ERROR( string::Format( "D3D12 Error: {}", GetResultLabel( hr ) ) );
        }
    }

    // https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error
    auto GetResultLabel( HRESULT value ) -> eastl::string {
        switch (value) {
            case S_OK:                           return "S_OK: Operation succeeded";
            case E_NOTIMPL:                      return "E_NOTIMPL: Not implemented";
            case E_NOINTERFACE:                  return "E_NOINTERFACE: Interface not supported";
            case E_POINTER:                      return "E_POINTER: Invalid pointer address";
            case E_ABORT:                        return "E_ABORT: Operation aborted";
            case E_FAIL:                         return "E_FAIL: Unspecified failure";
            case E_INVALIDARG:                   return "E_INVALIDARG: One or more arguments are invalid";
            case E_OUTOFMEMORY:                  return "E_OUTOFMEMORY: Failed to allocate necessary memory";

            case D3D12_ERROR_ADAPTER_NOT_FOUND:  return "D3D12_ERROR_ADAPTER_NOT_FOUND: The specified data adapter could not be found";

            case DXGI_ERROR_INVALID_CALL:        return "DXGI_ERROR_INVALID_CALL: The application made an invalid call sequence";
            case DXGI_ERROR_WAS_STILL_DRAWING:   return "DXGI_ERROR_WAS_STILL_DRAWING: The previous GPU operation is still processing";
            case DXGI_ERROR_UNSUPPORTED:         return "DXGI_ERROR_UNSUPPORTED: The requested feature or format is not supported";
            case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE: return "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE: The resource or target is temporarily unavailable";
            case DXGI_ERROR_MORE_DATA:           return "DXGI_ERROR_MORE_DATA: The buffer is too small to hold the requested data";

            case DXGI_ERROR_DEVICE_REMOVED:      return "DXGI_ERROR_DEVICE_REMOVED: The video card has been physically removed or a driver upgrade occurred";
            case DXGI_ERROR_DEVICE_HUNG:         return "DXGI_ERROR_DEVICE_HUNG: The GPU failed due to badly formed commands (Infinite loop / Timeout)";
            case DXGI_ERROR_DEVICE_RESET:        return "DXGI_ERROR_DEVICE_RESET: The hardware device was reset because of a poor command order";
            case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return "DXGI_ERROR_DRIVER_INTERNAL_ERROR: The driver encountered an unexpected internal breakdown";

            case DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE: return "DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE: Video present source is currently locked";
            case DXGI_ERROR_NOT_FOUND:           return "DXGI_ERROR_NOT_FOUND: The requested item or output window could not be found";
            case DXGI_ERROR_REMOTE_OUTOFMEMORY:  return "DXGI_ERROR_REMOTE_OUTOFMEMORY: The remote terminal encountered an out of memory state";
            case DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED: return "DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED: Remote rendering client terminated the connection";

            default: return string::Format("UNKNOWN_HRESULT: Code 0x{:08X}", as<UINT>(value));
        }
    }

    auto GetQueueType( rhi::QueueType type ) -> D3D12_COMMAND_LIST_TYPE {
        switch (type) {
            case rhi::QueueType::eGraphics:
            case rhi::QueueType::ePresent:
                return D3D12_COMMAND_LIST_TYPE_DIRECT;
            case rhi::QueueType::eCompute:
                return D3D12_COMMAND_LIST_TYPE_COMPUTE;
            case rhi::QueueType::eTransfer:
                return D3D12_COMMAND_LIST_TYPE_COPY;
            default:
                return D3D12_COMMAND_LIST_TYPE_NONE;
        }
    }

    auto GetShaderVisibility( rhi::ShaderFlags stage ) -> D3D12_SHADER_VISIBILITY {
        // 1. If it's visible to everything or multiple overlapping stages, default to ALL
        if (stage == rhi::ShaderFlagsBits::kAll) {
            return D3D12_SHADER_VISIBILITY_ALL;
        }

        // 2. Count the number of active stages using a simple helper or checking combinations.
        // If more than one bit is set, D3D12 requires using D3D12_SHADER_VISIBILITY_ALL.
        u32 bitCount{ 0 };
        if (stage & rhi::ShaderFlagsBits::kVertex)        bitCount++;
        if (stage & rhi::ShaderFlagsBits::kPixel)         bitCount++;
        if (stage & rhi::ShaderFlagsBits::kCompute)       bitCount++;
        if (stage & rhi::ShaderFlagsBits::kGeometry)      bitCount++;
        if (stage & rhi::ShaderFlagsBits::kHull)          bitCount++;
        if (stage & rhi::ShaderFlagsBits::kDomain)        bitCount++;
        if (stage & rhi::ShaderFlagsBits::kRayGeneration) bitCount++;
        if (stage & rhi::ShaderFlagsBits::kIntersection)  bitCount++;
        if (stage & rhi::ShaderFlagsBits::kAnyHit)        bitCount++;
        if (stage & rhi::ShaderFlagsBits::kClosestHit)    bitCount++;
        if (stage & rhi::ShaderFlagsBits::kMiss)          bitCount++;

        if (bitCount > 1) {
            return D3D12_SHADER_VISIBILITY_ALL;
        }

        // 3. If exactly one bit is set, map it to its dedicated hardware optimization slot
        if (stage & rhi::ShaderFlagsBits::kVertex)        return D3D12_SHADER_VISIBILITY_VERTEX;
        if (stage & rhi::ShaderFlagsBits::kPixel)         return D3D12_SHADER_VISIBILITY_PIXEL;
        if (stage & rhi::ShaderFlagsBits::kGeometry)      return D3D12_SHADER_VISIBILITY_GEOMETRY;
        if (stage & rhi::ShaderFlagsBits::kHull)          return D3D12_SHADER_VISIBILITY_HULL;
        if (stage & rhi::ShaderFlagsBits::kDomain)        return D3D12_SHADER_VISIBILITY_DOMAIN;

        // Note: Compute and Ray Tracing stages don't have separate explicit enum visibility values.
        // D3D12 mandates using _ALL for Compute PSOs and Ray Tracing state objects.
        if (stage & rhi::ShaderFlagsBits::kCompute)       return D3D12_SHADER_VISIBILITY_ALL;
        if (stage & rhi::ShaderFlagsBits::kRayGeneration) return D3D12_SHADER_VISIBILITY_ALL;
        if (stage & rhi::ShaderFlagsBits::kIntersection)  return D3D12_SHADER_VISIBILITY_ALL;
        if (stage & rhi::ShaderFlagsBits::kAnyHit)        return D3D12_SHADER_VISIBILITY_ALL;
        if (stage & rhi::ShaderFlagsBits::kClosestHit)    return D3D12_SHADER_VISIBILITY_ALL;
        if (stage & rhi::ShaderFlagsBits::kMiss)          return D3D12_SHADER_VISIBILITY_ALL;

        return D3D12_SHADER_VISIBILITY_ALL;
    }

    auto GetFormat( rhi::Format format ) -> DXGI_FORMAT {
        switch ( format ) {
            case rhi::Format::eUnknown:            return DXGI_FORMAT_UNKNOWN;

                // 8-bit UNORM
            case rhi::Format::eR8_UNORM:           return DXGI_FORMAT_R8_UNORM;
            case rhi::Format::eRG8_UNORM:          return DXGI_FORMAT_R8G8_UNORM;
            case rhi::Format::eRGBA8_UNORM:         return DXGI_FORMAT_R8G8B8A8_UNORM;
            case rhi::Format::eBGRA8_UNORM:         return DXGI_FORMAT_B8G8R8A8_UNORM;

                // 8-bit SNORM
            case rhi::Format::eR8_SNORM:           return DXGI_FORMAT_R8_SNORM;
            case rhi::Format::eRG8_SNORM:          return DXGI_FORMAT_R8G8_SNORM;
            case rhi::Format::eRGBA8_SNORM:         return DXGI_FORMAT_R8G8B8A8_SNORM;

                // 8-bit UINT / SINT
            case rhi::Format::eR8_UINT:            return DXGI_FORMAT_R8_UINT;
            case rhi::Format::eR8_SINT:            return DXGI_FORMAT_R8_SINT;
            case rhi::Format::eRG8_UINT:           return DXGI_FORMAT_R8G8_UINT;
            case rhi::Format::eRG8_SINT:           return DXGI_FORMAT_R8G8_SINT;
            case rhi::Format::eRGBA8_UINT:          return DXGI_FORMAT_R8G8B8A8_UINT;
            case rhi::Format::eRGBA8_SINT:          return DXGI_FORMAT_R8G8B8A8_SINT;

                // 16-bit UNORM / SNORM
            case rhi::Format::eR16_UNORM:          return DXGI_FORMAT_R16_UNORM;
            case rhi::Format::eRG16_UNORM:         return DXGI_FORMAT_R16G16_UNORM;
            case rhi::Format::eRGBA16_UNORM:        return DXGI_FORMAT_R16G16B16A16_UNORM;

            case rhi::Format::eR16_SNORM:          return DXGI_FORMAT_R16_SNORM;
            case rhi::Format::eRG16_SNORM:         return DXGI_FORMAT_R16G16_SNORM;
            case rhi::Format::eRGBA16_SNORM:        return DXGI_FORMAT_R16G16B16A16_SNORM;

                // 16-bit UINT / SINT
            case rhi::Format::eR16_UINT:           return DXGI_FORMAT_R16_UINT;
            case rhi::Format::eR16_SINT:           return DXGI_FORMAT_R16_SINT;
            case rhi::Format::eRG16_UINT:          return DXGI_FORMAT_R16G16_UINT;
            case rhi::Format::eRG16_SINT:          return DXGI_FORMAT_R16G16_SINT;
            case rhi::Format::eRGBA16_UINT:         return DXGI_FORMAT_R16G16B16A16_UINT;
            case rhi::Format::eRGBA16_SINT:         return DXGI_FORMAT_R16G16B16A16_SINT;

                // 16-bit FLOAT
            case rhi::Format::eR16_FLOAT:          return DXGI_FORMAT_R16_FLOAT;
            case rhi::Format::eRG16_FLOAT:         return DXGI_FORMAT_R16G16_FLOAT;
            case rhi::Format::eRGBA16_FLOAT:        return DXGI_FORMAT_R16G16B16A16_FLOAT;

                // 32-bit FLOAT
            case rhi::Format::eR32_FLOAT:          return DXGI_FORMAT_R32_FLOAT;
            case rhi::Format::eRG32_FLOAT:         return DXGI_FORMAT_R32G32_FLOAT;
            case rhi::Format::eRGB32_FLOAT:        return DXGI_FORMAT_R32G32B32_FLOAT;
            case rhi::Format::eRGBA32_FLOAT:        return DXGI_FORMAT_R32G32B32A32_FLOAT;

                // 32-bit UINT / SINT
            case rhi::Format::eR32_UINT:           return DXGI_FORMAT_R32_UINT;
            case rhi::Format::eR32_SINT:           return DXGI_FORMAT_R32_SINT;
            case rhi::Format::eRG32_UINT:          return DXGI_FORMAT_R32G32_UINT;
            case rhi::Format::eRG32_SINT:          return DXGI_FORMAT_R32G32_SINT;
            case rhi::Format::eRGB32_UINT:         return DXGI_FORMAT_R32G32B32_UINT;
            case rhi::Format::eRGB32_SINT:         return DXGI_FORMAT_R32G32B32_SINT;
            case rhi::Format::eRGBA32_UINT:         return DXGI_FORMAT_R32G32B32A32_UINT;
            case rhi::Format::eRGBA32_SINT:         return DXGI_FORMAT_R32G32B32A32_SINT;

                // Packed formats
            case rhi::Format::eR10G10B10A2_UNORM:  return DXGI_FORMAT_R10G10B10A2_UNORM;
            case rhi::Format::eR11G11B10_FLOAT:    return DXGI_FORMAT_R11G11B10_FLOAT;

                // Depth formats
            case rhi::Format::eD16:                return DXGI_FORMAT_D16_UNORM;
            case rhi::Format::eD24S8:              return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case rhi::Format::eD32:                return DXGI_FORMAT_D32_FLOAT;
            case rhi::Format::eD32S8:              return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

                // BC compressed
            case rhi::Format::eBC1_UNORM:          return DXGI_FORMAT_BC1_UNORM;
            case rhi::Format::eBC1_UNORM_SRGB:     return DXGI_FORMAT_BC1_UNORM_SRGB;
            case rhi::Format::eBC2_UNORM:          return DXGI_FORMAT_BC2_UNORM;
            case rhi::Format::eBC2_UNORM_SRGB:     return DXGI_FORMAT_BC2_UNORM_SRGB;
            case rhi::Format::eBC3_UNORM:          return DXGI_FORMAT_BC3_UNORM;
            case rhi::Format::eBC3_UNORM_SRGB:     return DXGI_FORMAT_BC3_UNORM_SRGB;
            case rhi::Format::eBC4_UNORM:          return DXGI_FORMAT_BC4_UNORM;
            case rhi::Format::eBC4_SNORM:          return DXGI_FORMAT_BC4_SNORM;
            case rhi::Format::eBC5_UNORM:          return DXGI_FORMAT_BC5_UNORM;
            case rhi::Format::eBC5_SNORM:          return DXGI_FORMAT_BC5_SNORM;
            case rhi::Format::eBC6H_UFLOAT:        return DXGI_FORMAT_BC6H_UF16;
            case rhi::Format::eBC6H_SFLOAT:        return DXGI_FORMAT_BC6H_SF16;
            case rhi::Format::eBC7_UNORM:          return DXGI_FORMAT_BC7_UNORM;
            case rhi::Format::eBC7_UNORM_SRGB:     return DXGI_FORMAT_BC7_UNORM_SRGB;

            case rhi::Format::eCount:
            default:                          return DXGI_FORMAT_UNKNOWN;
        }
    }

    auto GetFillMode( rhi::PolygonMode type ) -> D3D12_FILL_MODE {
        switch (type) {

            case PolygonMode::eLines: return D3D12_FILL_MODE_WIREFRAME;

            case PolygonMode::ePoint:
            case PolygonMode::eFill: return D3D12_FILL_MODE_SOLID;
        }

        return D3D12_FILL_MODE_SOLID;
    }

    auto GetCullMode( rhi::CullMode type ) -> D3D12_CULL_MODE {
        switch (type) {

            case CullMode::eNone: return D3D12_CULL_MODE_NONE;
            case CullMode::eCullFront: return D3D12_CULL_MODE_FRONT;
            case CullMode::eCullBack: return D3D12_CULL_MODE_BACK;
        }

        return D3D12_CULL_MODE_NONE;
    }

    auto GetDepthCompareOp( rhi::DepthCompareOp op ) -> D3D12_COMPARISON_FUNC {
        switch (op) {
            case rhi::DepthCompareOp::eNever:          return D3D12_COMPARISON_FUNC_NEVER;
            case rhi::DepthCompareOp::eLess:           return D3D12_COMPARISON_FUNC_LESS;
            case rhi::DepthCompareOp::eEqual:          return D3D12_COMPARISON_FUNC_EQUAL;
            case rhi::DepthCompareOp::eLessOrEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
            case rhi::DepthCompareOp::eGreater:        return D3D12_COMPARISON_FUNC_GREATER;
            case rhi::DepthCompareOp::eNotEqual:       return D3D12_COMPARISON_FUNC_NOT_EQUAL;
            case rhi::DepthCompareOp::eGreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            case rhi::DepthCompareOp::eAlways:         return D3D12_COMPARISON_FUNC_ALWAYS;
            default:                                   return D3D12_COMPARISON_FUNC_NEVER;
        }
    }

    auto GetTopologyType( rhi::PrimitiveTopology type ) -> D3D12_PRIMITIVE_TOPOLOGY_TYPE {
        switch (type) {
            case rhi::PrimitiveTopology::eInvalid:
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;

            case rhi::PrimitiveTopology::ePointList:
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

            case rhi::PrimitiveTopology::eLineList:
            case rhi::PrimitiveTopology::eLineStrip:
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

            case rhi::PrimitiveTopology::eTriangleList:
            case rhi::PrimitiveTopology::eTriangleStrip:
            case rhi::PrimitiveTopology::eTriangleFan:
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }

        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }

    auto GetCmdTopologyType( rhi::PrimitiveTopology type ) -> D3D_PRIMITIVE_TOPOLOGY {
        switch (type) {
            case rhi::PrimitiveTopology::eInvalid:
                return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

            case rhi::PrimitiveTopology::ePointList:
                return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

            case rhi::PrimitiveTopology::eLineList:
                return D3D_PRIMITIVE_TOPOLOGY_LINELIST;

            case rhi::PrimitiveTopology::eLineStrip:
                return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;

            case rhi::PrimitiveTopology::eTriangleList:
                return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

            case rhi::PrimitiveTopology::eTriangleStrip:
                return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

            case rhi::PrimitiveTopology::eTriangleFan:
                return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        }

        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    auto GetResourceState( rhi::ResourceStates state ) -> D3D12_RESOURCE_STATES {
        switch (state) {
            case rhi::ResourceStates::eCommon:                return D3D12_RESOURCE_STATE_COMMON;

            case rhi::ResourceStates::eConstantBuffer:
            case rhi::ResourceStates::eVertexBuffer:          return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

            case rhi::ResourceStates::eIndexBuffer:           return D3D12_RESOURCE_STATE_INDEX_BUFFER;
            case rhi::ResourceStates::eIndirectArgument:      return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            case rhi::ResourceStates::eShaderResource:        return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
            case rhi::ResourceStates::eUnorderedAccess:       return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case rhi::ResourceStates::eRenderTarget:          return D3D12_RESOURCE_STATE_RENDER_TARGET;
            case rhi::ResourceStates::eDepthWrite:            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            case rhi::ResourceStates::eDepthRead:             return D3D12_RESOURCE_STATE_DEPTH_READ;
            case rhi::ResourceStates::eCopyDest:              return D3D12_RESOURCE_STATE_COPY_DEST;
            case rhi::ResourceStates::eCopySource:            return D3D12_RESOURCE_STATE_COPY_SOURCE;
            case rhi::ResourceStates::eResolveDest:           return D3D12_RESOURCE_STATE_RESOLVE_DEST;
            case rhi::ResourceStates::eResolveSource:         return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            case rhi::ResourceStates::ePresent:               return D3D12_RESOURCE_STATE_PRESENT;

            case rhi::ResourceStates::eAccelStructRead:       return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            case rhi::ResourceStates::eAccelStructWrite:      return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case rhi::ResourceStates::eAccelStructBuildInput: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            case rhi::ResourceStates::eAccelStructBuildBlas:  return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

            case rhi::ResourceStates::eUnknown:
            default:
                return D3D12_RESOURCE_STATE_COMMON; // Resources are created by default in an unknown state which is Common, see the allocator
        }
    }

    auto GetBarrierSync( rhi::PipelineStageFlags flags ) -> D3D12_BARRIER_SYNC {
        D3D12_BARRIER_SYNC result{};

        if (flags & rhi::PipelineStageFlagsBits::kTop) result |= D3D12_BARRIER_SYNC_NONE;
        if (flags & rhi::PipelineStageFlagsBits::kDrawIndirect) result |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
        if (flags & rhi::PipelineStageFlagsBits::kVertexInput) result |= D3D12_BARRIER_SYNC_INDEX_INPUT | D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if (flags & rhi::PipelineStageFlagsBits::kVertexShader) result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if (flags & rhi::PipelineStageFlagsBits::kHullShader) result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if (flags & rhi::PipelineStageFlagsBits::kDomainShader) result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if (flags & rhi::PipelineStageFlagsBits::kGeometryShader) result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if (flags & rhi::PipelineStageFlagsBits::kPixelShader) result |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
        if (flags & rhi::PipelineStageFlagsBits::kComputeShader) result |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        if (flags & rhi::PipelineStageFlagsBits::kColorAttachment) result |= D3D12_BARRIER_SYNC_RENDER_TARGET;
        if (flags & rhi::PipelineStageFlagsBits::kEarlyFragmentTests) result |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        if (flags & rhi::PipelineStageFlagsBits::kCopy) result |= D3D12_BARRIER_SYNC_COPY;
        if (flags & rhi::PipelineStageFlagsBits::kBottom) result |= D3D12_BARRIER_SYNC_ALL;
        if (flags & rhi::PipelineStageFlagsBits::kAll) result |= D3D12_BARRIER_SYNC_ALL;

        return result;
    }

    auto GetBarrierAccess( rhi::AccessFlags flags ) -> D3D12_BARRIER_ACCESS {
        D3D12_BARRIER_ACCESS result{};

        if (flags & rhi::AccessFlagsBits::kIndirectRead) result |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        if (flags & rhi::AccessFlagsBits::kIndexRead) result |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        if (flags & rhi::AccessFlagsBits::kVertexRead) result |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
        if (flags & rhi::AccessFlagsBits::kConstantRead) result |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        if (flags & rhi::AccessFlagsBits::kShaderRead) result |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        if (flags & rhi::AccessFlagsBits::kShaderWrite) result |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        if (flags & rhi::AccessFlagsBits::kRenderTarget) result |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
        if (flags & rhi::AccessFlagsBits::kDepthStencilRead) result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        if (flags & rhi::AccessFlagsBits::kDepthStencilWrite) result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        if (flags & rhi::AccessFlagsBits::kCopyRead) result |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
        if (flags & rhi::AccessFlagsBits::kCopyWrite) result |= D3D12_BARRIER_ACCESS_COPY_DEST;

        return result;
    }

    auto GetBarrierLayout( rhi::TextureLayoutFlags layout ) -> D3D12_BARRIER_LAYOUT {
        if (layout == rhi::TextureLayoutBits::kUnknown) return D3D12_BARRIER_LAYOUT_UNDEFINED;
        if (layout == rhi::TextureLayoutBits::kGeneral) return D3D12_BARRIER_LAYOUT_COMMON;
        if (layout == rhi::TextureLayoutBits::kColorAttachment) return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        if (layout == rhi::TextureLayoutBits::kDepthStencilWrite) return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        if (layout == rhi::TextureLayoutBits::kDepthStencilRead) return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        if (layout == rhi::TextureLayoutBits::kShaderResource) return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
        if (layout == rhi::TextureLayoutBits::kUnorderedAccess) return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
        if (layout == rhi::TextureLayoutBits::kCopySrc) return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
        if (layout == rhi::TextureLayoutBits::kCopyDst) return D3D12_BARRIER_LAYOUT_COPY_DEST;
        if (layout == rhi::TextureLayoutBits::kPresent) return D3D12_BARRIER_LAYOUT_PRESENT;

        return D3D12_BARRIER_LAYOUT_UNDEFINED;
    }

    auto GetHeapType( rhi::HeapType type ) -> D3D12_HEAP_TYPE {
        switch (type) {
            case HeapType::eDeviceLocal: return D3D12_HEAP_TYPE_DEFAULT;
            case HeapType::eUpload:      return D3D12_HEAP_TYPE_UPLOAD;
            case HeapType::eReadback:     return D3D12_HEAP_TYPE_READBACK;
        }
        return D3D12_HEAP_TYPE_DEFAULT;
    }

    auto GetDescriptorHeapType( rhi::ResourceType type ) -> D3D12_DESCRIPTOR_HEAP_TYPE {
        switch (type) {
            case rhi::ResourceType::eTexture_SRV:
            case rhi::ResourceType::eTypedBuffer_SRV:
            case rhi::ResourceType::eStructuredBuffer_SRV:
            case rhi::ResourceType::eRawBuffer_SRV:
            case rhi::ResourceType::eRayTracingAccelStruct:
            case rhi::ResourceType::eTexture_UAV:
            case rhi::ResourceType::eTypedBuffer_UAV:
            case rhi::ResourceType::eStructuredBuffer_UAV:
            case rhi::ResourceType::eRawBuffer_UAV:
            case rhi::ResourceType::eSamplerFeedbackTexture_UAV:
            case rhi::ResourceType::eConstantBuffer:
            case rhi::ResourceType::eVolatileConstantBuffer:
                return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

            case rhi::ResourceType::eSampler:
                return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

            case rhi::ResourceType::ePushConstants:
            case rhi::ResourceType::eInvalid:
            default:
                MKT_ASSERT( false, "Invalid heap type requested. Push constants do not utilize descriptor heaps!" );
                return as<D3D12_DESCRIPTOR_HEAP_TYPE>(-1);
        }
    }

    auto GetDescriptorRangeType( rhi::ResourceType type ) -> D3D12_DESCRIPTOR_RANGE_TYPE {
        switch (type) {
            case rhi::ResourceType::eTexture_SRV:
            case rhi::ResourceType::eTypedBuffer_SRV:
            case rhi::ResourceType::eStructuredBuffer_SRV:
            case rhi::ResourceType::eRawBuffer_SRV:
            case rhi::ResourceType::eRayTracingAccelStruct:       return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

            case rhi::ResourceType::eTexture_UAV:
            case rhi::ResourceType::eTypedBuffer_UAV:
            case rhi::ResourceType::eStructuredBuffer_UAV:
            case rhi::ResourceType::eRawBuffer_UAV:
            case rhi::ResourceType::eSamplerFeedbackTexture_UAV:  return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

            case rhi::ResourceType::eConstantBuffer:
            case rhi::ResourceType::eVolatileConstantBuffer:     return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

            case rhi::ResourceType::eSampler:                    return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

            case rhi::ResourceType::ePushConstants:
            case rhi::ResourceType::eInvalid:
            default:                                              MKT_ASSERT( false, "Invalid range type or provided PS. Push constants belong in Root Constants, not a descriptor range!" );
        }
    }

    auto GetDimension( rhi::TextureDimension dimension ) -> D3D12_RESOURCE_DIMENSION {
        switch (dimension) {
            case rhi::TextureDimension::eTexture1D:
            case rhi::TextureDimension::eTexture1DArray:
                return D3D12_RESOURCE_DIMENSION_TEXTURE1D;

            case rhi::TextureDimension::eTexture2D:
            case rhi::TextureDimension::eTexture2DArray:
            case rhi::TextureDimension::eTextureCube:
            case rhi::TextureDimension::eTextureCubeArray:
            case rhi::TextureDimension::eTexture2DMS:
            case rhi::TextureDimension::eTexture2DMSArray:
                return D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            case rhi::TextureDimension::eTexture3D:
                return D3D12_RESOURCE_DIMENSION_TEXTURE3D;

            default:
                MKT_ASSERT(false, "Unsupported texture dimension");
                return D3D12_RESOURCE_DIMENSION_UNKNOWN;
        }
    }


    auto GetSampleCount( rhi::Multisampling multisampling ) -> UINT {
        switch (multisampling) {
            case rhi::Multisampling::eMsaaX1: return 1;
            case rhi::Multisampling::eMsaaX2: return 2;
            case rhi::Multisampling::eMsaaX4: return 4;
            case rhi::Multisampling::eMsaaX8: return 8;
            case rhi::Multisampling::eMsaaX16: return 16;
            default:;
        }

        return 1;
    }

    auto GetResourceFlags( rhi::TextureUsageFlags flags ) -> D3D12_RESOURCE_FLAGS {
        D3D12_RESOURCE_FLAGS result{ D3D12_RESOURCE_FLAG_NONE };

        if (flags & rhi::TextureUsageFlagsBits::kRenderTarget) {
            result |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }

        if (flags & rhi::TextureUsageFlagsBits::kDepthTarget) {
            result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        if (flags & rhi::TextureUsageFlagsBits::kStencilTarget) {
            result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        if (flags & rhi::TextureUsageFlagsBits::kDepthStencilTarget) {
            result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        return result;
    }
}// namespace mikoto::renderer::d3d12

#endif