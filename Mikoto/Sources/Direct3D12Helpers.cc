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

#include <Logging/Assert.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {
    auto ThrowIfFailed( HRESULT hr ) -> void {
        if (FAILED(hr)) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 Error" ); // TODO: improved errors
        }
    }

    auto ThrowIfFailed( HRESULT hr, IGpuDevice *gpuDevice ) -> void {
        if (FAILED(hr)) {
            Device* d3d12Device{ checked_cast<Device*>( gpuDevice ) };
            d3d12Device->DumpMessages();
            MKT_THROW_RUNTIME_ERROR( "D3D12 Error" );
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