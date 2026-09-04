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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Texture.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Libraries.hh>

namespace mikoto::renderer::d3d11 {

    MKT_NODISCARD auto GetFormat(Format format) -> DXGI_FORMAT {
        switch (format) {
            case Format::eUnknown:              return DXGI_FORMAT_UNKNOWN;

            case Format::eBGRA8_UNORM:          return DXGI_FORMAT_B8G8R8A8_UNORM;

            case Format::eR8_UNORM:             return DXGI_FORMAT_R8_UNORM;
            case Format::eRG8_UNORM:            return DXGI_FORMAT_R8G8_UNORM;
            case Format::eRGBA8_UNORM:          return DXGI_FORMAT_R8G8B8A8_UNORM;

            case Format::eR8_SNORM:             return DXGI_FORMAT_R8_SNORM;
            case Format::eRG8_SNORM:            return DXGI_FORMAT_R8G8_SNORM;
            case Format::eRGBA8_SNORM:          return DXGI_FORMAT_R8G8B8A8_SNORM;

            case Format::eR8_UINT:              return DXGI_FORMAT_R8_UINT;
            case Format::eR8_SINT:              return DXGI_FORMAT_R8_SINT;
            case Format::eRG8_UINT:             return DXGI_FORMAT_R8G8_UINT;
            case Format::eRG8_SINT:             return DXGI_FORMAT_R8G8_SINT;
            case Format::eRGBA8_UINT:           return DXGI_FORMAT_R8G8B8A8_UINT;
            case Format::eRGBA8_SINT:           return DXGI_FORMAT_R8G8B8A8_SINT;

            case Format::eR16_UNORM:            return DXGI_FORMAT_R16_UNORM;
            case Format::eRG16_UNORM:           return DXGI_FORMAT_R16G16_UNORM;
            case Format::eRGBA16_UNORM:         return DXGI_FORMAT_R16G16B16A16_UNORM;

            case Format::eR16_SNORM:            return DXGI_FORMAT_R16_SNORM;
            case Format::eRG16_SNORM:           return DXGI_FORMAT_R16G16_SNORM;
            case Format::eRGBA16_SNORM:         return DXGI_FORMAT_R16G16B16A16_SNORM;

            case Format::eR16_UINT:             return DXGI_FORMAT_R16_UINT;
            case Format::eR16_SINT:             return DXGI_FORMAT_R16_SINT;
            case Format::eRG16_UINT:            return DXGI_FORMAT_R16G16_UINT;
            case Format::eRG16_SINT:            return DXGI_FORMAT_R16G16_SINT;
            case Format::eRGBA16_UINT:          return DXGI_FORMAT_R16G16B16A16_UINT;
            case Format::eRGBA16_SINT:          return DXGI_FORMAT_R16G16B16A16_SINT;

            case Format::eR16_FLOAT:            return DXGI_FORMAT_R16_FLOAT;
            case Format::eRG16_FLOAT:           return DXGI_FORMAT_R16G16_FLOAT;
            case Format::eRGBA16_FLOAT:         return DXGI_FORMAT_R16G16B16A16_FLOAT;

            case Format::eR32_FLOAT:            return DXGI_FORMAT_R32_FLOAT;
            case Format::eRG32_FLOAT:           return DXGI_FORMAT_R32G32_FLOAT;
            case Format::eRGB32_FLOAT:          return DXGI_FORMAT_R32G32B32_FLOAT;
            case Format::eRGBA32_FLOAT:         return DXGI_FORMAT_R32G32B32A32_FLOAT;

            case Format::eR32_UINT:             return DXGI_FORMAT_R32_UINT;
            case Format::eR32_SINT:             return DXGI_FORMAT_R32_SINT;
            case Format::eRG32_UINT:            return DXGI_FORMAT_R32G32_UINT;
            case Format::eRG32_SINT:            return DXGI_FORMAT_R32G32_SINT;
            case Format::eRGB32_UINT:           return DXGI_FORMAT_R32G32B32_UINT;
            case Format::eRGB32_SINT:           return DXGI_FORMAT_R32G32B32_SINT;
            case Format::eRGBA32_UINT:          return DXGI_FORMAT_R32G32B32A32_UINT;
            case Format::eRGBA32_SINT:          return DXGI_FORMAT_R32G32B32A32_SINT;

            case Format::eR10G10B10A2_UNORM:    return DXGI_FORMAT_R10G10B10A2_UNORM;
            case Format::eR11G11B10_FLOAT:      return DXGI_FORMAT_R11G11B10_FLOAT;

            case Format::eBGRA4_UNORM:          return DXGI_FORMAT_B4G4R4A4_UNORM;
            case Format::eB5G6R5_UNORM:         return DXGI_FORMAT_B5G6R5_UNORM;
            case Format::eB5G5R5A1_UNORM:       return DXGI_FORMAT_B5G5R5A1_UNORM;

            case Format::eBGRX8_UNORM:          return DXGI_FORMAT_B8G8R8X8_UNORM;
            case Format::eSRGBA8_UNORM:         return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case Format::eSBGRA8_UNORM:         return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            case Format::eSBGRX8_UNORM:         return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

            // Depth / stencil
            case Format::eD16:                  return DXGI_FORMAT_D16_UNORM;
            case Format::eD24S8:                return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case Format::eX24G8_UINT:           return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
            case Format::eD32:                  return DXGI_FORMAT_D32_FLOAT;
            case Format::eD32S8:                return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            case Format::eX32G8_UINT:           return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

            // BC compressed
            case Format::eBC1_UNORM:            return DXGI_FORMAT_BC1_UNORM;
            case Format::eBC1_UNORM_SRGB:       return DXGI_FORMAT_BC1_UNORM_SRGB;
            case Format::eBC2_UNORM:            return DXGI_FORMAT_BC2_UNORM;
            case Format::eBC2_UNORM_SRGB:       return DXGI_FORMAT_BC2_UNORM_SRGB;
            case Format::eBC3_UNORM:            return DXGI_FORMAT_BC3_UNORM;
            case Format::eBC3_UNORM_SRGB:       return DXGI_FORMAT_BC3_UNORM_SRGB;
            case Format::eBC4_UNORM:            return DXGI_FORMAT_BC4_UNORM;
            case Format::eBC4_SNORM:            return DXGI_FORMAT_BC4_SNORM;
            case Format::eBC5_UNORM:            return DXGI_FORMAT_BC5_UNORM;
            case Format::eBC5_SNORM:            return DXGI_FORMAT_BC5_SNORM;
            case Format::eBC6H_UFLOAT:          return DXGI_FORMAT_BC6H_UF16;
            case Format::eBC6H_SFLOAT:          return DXGI_FORMAT_BC6H_SF16;
            case Format::eBC7_UNORM:            return DXGI_FORMAT_BC7_UNORM;
            case Format::eBC7_UNORM_SRGB:       return DXGI_FORMAT_BC7_UNORM_SRGB;

            case Format::eCount:
            default:
                return DXGI_FORMAT_UNKNOWN;
        }

        MKT_ASSERT(false, "Unsupported TextureFormat for D3D11");
        return DXGI_FORMAT_UNKNOWN;
    }

    auto GetUsageFromHeapType( HeapType type ) -> D3D11_USAGE {
        switch (type ) {
            case HeapType::eDeviceLocal: return D3D11_USAGE_DEFAULT;
            case HeapType::eUpload: return D3D11_USAGE_DYNAMIC;
            case HeapType::eReadback: return D3D11_USAGE_STAGING;
        }

        return D3D11_USAGE_IMMUTABLE;
    }

    auto GetBindFlags( BufferUsageFlags flags ) -> UINT {
        UINT result{};

        if (flags.Has( BufferUsageFlagsBits::kVertex )) {
            result |= D3D11_BIND_VERTEX_BUFFER;
        }

        if (flags.Has( BufferUsageFlagsBits::kIndex )) {
            result |= D3D11_BIND_INDEX_BUFFER;
        }

        if (flags.Has( BufferUsageFlagsBits::kConstant )) {
            result |= D3D11_BIND_CONSTANT_BUFFER;
        }

        if (flags.Has( BufferUsageFlagsBits::kStorage )) {
            result |= D3D11_BIND_UNORDERED_ACCESS;
        }

        return result;
    }

    auto GetBindFlags( TextureUsageFlags flags ) -> D3D11_BIND_FLAG {
        UINT result{};

        // Either render target or depth
        if (flags.Has( TextureUsageFlagsBits::kRenderTarget )) {
            result = result | D3D11_BIND_RENDER_TARGET;
        }

        if (flags.Has( TextureUsageFlagsBits::kDepthTarget |
            TextureUsageFlagsBits::kDepthStencilTarget |
            TextureUsageFlagsBits::kStencilTarget )) {
            result = D3D11_BIND_DEPTH_STENCIL ;
        }

        if (flags.Has( TextureUsageFlagsBits::kShaderResource )) {
            result = result | D3D11_BIND_SHADER_RESOURCE;
        }

        return as<D3D11_BIND_FLAG>(result);
    }

    auto GetPrimitiveTopology( PrimitiveTopology topology) -> D3D11_PRIMITIVE_TOPOLOGY {
        switch (topology) {
            case PrimitiveTopology::eTriangleList:
                return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

            case PrimitiveTopology::eTriangleStrip:
                return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

            case PrimitiveTopology::eLineList:
                return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

            default:
                return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        }

        return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    auto GetCullMode( CullMode mode ) -> D3D11_CULL_MODE {
        switch (mode) {
            case CullMode::eCullFront: return D3D11_CULL_FRONT;
            case CullMode::eCullBack: return D3D11_CULL_BACK;
            default:;
        }

        return D3D11_CULL_NONE;
    }

    auto GetComparisonFunc(CompareOp op) -> D3D11_COMPARISON_FUNC {
        switch (op) {
            case CompareOp::eNever:
                return D3D11_COMPARISON_NEVER;

            case CompareOp::eLess:
                return D3D11_COMPARISON_LESS;

            case CompareOp::eEqual:
                return D3D11_COMPARISON_EQUAL;

            case CompareOp::eLessOrEqual:
                return D3D11_COMPARISON_LESS_EQUAL;

            case CompareOp::eGreater:
                return D3D11_COMPARISON_GREATER;

            case CompareOp::eNotEqual:
                return D3D11_COMPARISON_NOT_EQUAL;

            case CompareOp::eGreaterOrEqual:
                return D3D11_COMPARISON_GREATER_EQUAL;

            case CompareOp::eAlways:
                return D3D11_COMPARISON_ALWAYS;

            default:
                return D3D11_COMPARISON_ALWAYS;
        }
    }

    auto GetFilter( SamplerFilter min, SamplerFilter mag ) -> D3D11_FILTER {
        if (min == SamplerFilter::eNearest && mag == SamplerFilter::eNearest)
            return D3D11_FILTER_MIN_MAG_MIP_POINT;

        if (min == SamplerFilter::eLinear && mag == SamplerFilter::eLinear)
            return D3D11_FILTER_MIN_MAG_MIP_LINEAR;

        if (min == SamplerFilter::eNearest && mag == SamplerFilter::eLinear)
            return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;

        if (min == SamplerFilter::eLinear && mag == SamplerFilter::eNearest)
            return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;

        return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    }

    auto GetAddressMode( SamplerWrapMode mode ) -> D3D11_TEXTURE_ADDRESS_MODE {
        switch (mode) {
            case SamplerWrapMode::eRepeat:       return D3D11_TEXTURE_ADDRESS_WRAP;
            case SamplerWrapMode::eClampToEdge:   return D3D11_TEXTURE_ADDRESS_CLAMP;
            case SamplerWrapMode::eMirroredRepeat: return D3D11_TEXTURE_ADDRESS_MIRROR;
            case SamplerWrapMode::eClampToBorder: return D3D11_TEXTURE_ADDRESS_BORDER;
            default:                            return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }

    auto GetBytesPerPixel( rhi::Format type ) -> UINT {
        switch (type) {
            case Format::eRGBA8_SNORM:
            case Format::eRGBA8_UNORM:
                return 4;
            case Format::eRGBA32_FLOAT:
            case Format::eRGBA32_SINT:
            case Format::eRGBA32_UINT:
                return 16;
            default:;
        }

        MKT_ASSERT( false, "Unsupported format for bytes per pixel" );
        return 0;
    }

    auto GetInputElementDescription( const VertexAttributeDescription &desc, InputRate rate ) -> D3D11_INPUT_ELEMENT_DESC {
        D3D11_INPUT_ELEMENT_DESC result{};

        const auto& formatInfo{ rhi::GetFormatInfo( desc.mFormat ) };

        result.SemanticName         = desc.mName.data();
        result.SemanticIndex        = desc.mLocation;

        result.Format               = GetFormat(desc.mFormat);

        result.InputSlot            = desc.mBinding;
        result.AlignedByteOffset    = desc.mOffset;

        if (rate == InputRate::ePerVertex) {
            result.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
            result.InstanceDataStepRate = 0;
        } else {
            result.InputSlotClass       = D3D11_INPUT_PER_INSTANCE_DATA;
            result.InstanceDataStepRate = 1;
        }

        return result;
    }

    auto ParseSemantic( eastl::string_view value ) -> Semantic {
        Semantic result{};

        if (value.empty())
            return result;

        size_t splitPos = value.size();

        // Walk backwards to find where digits start
        while (splitPos > 0 && string::IsNumber(value[splitPos - 1])) {
            --splitPos;
        }

        // Name part
        result.mName = value.substr(0, splitPos);

        // Index part (if any)
        if (splitPos < value.size()) {
            uint32_t index{0};

            for (size_t i = splitPos; i < value.size(); ++i) {
                index = index * 10 + (value[i] - '0');
            }

            result.mIndex = index;
        } else {
            result.mIndex = 0;
        }

        return result;
    }
}// namespace mikoto::renderer::d3d11

#endif