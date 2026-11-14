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
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Texture.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Libraries.hh>

namespace mikoto::renderer::d3d11 {

    auto GetBlendFactor( rhi::BlendFactor factor ) -> D3D11_BLEND {
        MKT_BEGIN_PROFILER();

        switch ( factor ) {
            case rhi::BlendFactor::eZero:
                return D3D11_BLEND_ZERO;
            case rhi::BlendFactor::eOne:
                return D3D11_BLEND_ONE;
            case rhi::BlendFactor::eSrcColor:
                return D3D11_BLEND_SRC_COLOR;
            case rhi::BlendFactor::eOneMinusSrcColor:
                return D3D11_BLEND_INV_SRC_COLOR;
            case rhi::BlendFactor::eDstColor:
                return D3D11_BLEND_DEST_COLOR;
            case rhi::BlendFactor::eOneMinusDstColor:
                return D3D11_BLEND_INV_DEST_COLOR;
            case rhi::BlendFactor::eSrcAlpha:
                return D3D11_BLEND_SRC_ALPHA;
            case rhi::BlendFactor::eOneMinusSrcAlpha:
                return D3D11_BLEND_INV_SRC_ALPHA;
            case rhi::BlendFactor::eDstAlpha:
                return D3D11_BLEND_DEST_ALPHA;
            case rhi::BlendFactor::eOneMinusDstAlpha:
                return D3D11_BLEND_INV_DEST_ALPHA;
            case rhi::BlendFactor::eSrcAlphaSaturate:
                return D3D11_BLEND_SRC_ALPHA_SAT;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid blend factor." );
        }
    }

    auto GetBlendOp( rhi::BlendOperation operation ) -> D3D11_BLEND_OP {
        MKT_BEGIN_PROFILER();

        switch ( operation ) {
            case rhi::BlendOperation::eAdd:
                return D3D11_BLEND_OP_ADD;
            case rhi::BlendOperation::eSubtract:
                return D3D11_BLEND_OP_SUBTRACT;

            // D3D11 does not support this one
            case rhi::BlendOperation::eReverseSubtract:
                return D3D11_BLEND_OP_SUBTRACT;

            case rhi::BlendOperation::eMin:
                return D3D11_BLEND_OP_MIN;
            case rhi::BlendOperation::eMax:
                return D3D11_BLEND_OP_MAX;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid blend operation." );
        }
    }

    auto GetColorWriteMask( rhi::ColorWriteMask writeMask ) -> UINT8 {
        MKT_BEGIN_PROFILER();
        const auto bits{ static_cast<core::u8>( writeMask ) };

        if ( bits > 0x0F ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid color write mask." );
        }

        UINT8 mask{};

        if ( bits & 0x01 ) {
            mask |= D3D11_COLOR_WRITE_ENABLE_RED;
        }

        if ( bits & 0x02 ) {
            mask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
        }

        if ( bits & 0x04 ) {
            mask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
        }

        if ( bits & 0x08 ) {
            mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
        }

        return mask;
    }

    MKT_NODISCARD auto GetFormat( renderer::rhi::Format format ) -> DXGI_FORMAT {
        MKT_BEGIN_PROFILER();

        switch ( format ) {
            case renderer::rhi::Format::eUnknown:
                return DXGI_FORMAT_UNKNOWN;

            case renderer::rhi::Format::eBGRA8_UNORM:
                return DXGI_FORMAT_B8G8R8A8_UNORM;

            case renderer::rhi::Format::eR8_UNORM:
                return DXGI_FORMAT_R8_UNORM;
            case renderer::rhi::Format::eRG8_UNORM:
                return DXGI_FORMAT_R8G8_UNORM;
            case renderer::rhi::Format::eRGBA8_UNORM:
                return DXGI_FORMAT_R8G8B8A8_UNORM;

            case renderer::rhi::Format::eR8_SNORM:
                return DXGI_FORMAT_R8_SNORM;
            case renderer::rhi::Format::eRG8_SNORM:
                return DXGI_FORMAT_R8G8_SNORM;
            case renderer::rhi::Format::eRGBA8_SNORM:
                return DXGI_FORMAT_R8G8B8A8_SNORM;

            case renderer::rhi::Format::eR8_UINT:
                return DXGI_FORMAT_R8_UINT;
            case renderer::rhi::Format::eR8_SINT:
                return DXGI_FORMAT_R8_SINT;
            case renderer::rhi::Format::eRG8_UINT:
                return DXGI_FORMAT_R8G8_UINT;
            case renderer::rhi::Format::eRG8_SINT:
                return DXGI_FORMAT_R8G8_SINT;
            case renderer::rhi::Format::eRGBA8_UINT:
                return DXGI_FORMAT_R8G8B8A8_UINT;
            case renderer::rhi::Format::eRGBA8_SINT:
                return DXGI_FORMAT_R8G8B8A8_SINT;

            case renderer::rhi::Format::eR16_UNORM:
                return DXGI_FORMAT_R16_UNORM;
            case renderer::rhi::Format::eRG16_UNORM:
                return DXGI_FORMAT_R16G16_UNORM;
            case renderer::rhi::Format::eRGBA16_UNORM:
                return DXGI_FORMAT_R16G16B16A16_UNORM;

            case renderer::rhi::Format::eR16_SNORM:
                return DXGI_FORMAT_R16_SNORM;
            case renderer::rhi::Format::eRG16_SNORM:
                return DXGI_FORMAT_R16G16_SNORM;
            case renderer::rhi::Format::eRGBA16_SNORM:
                return DXGI_FORMAT_R16G16B16A16_SNORM;

            case renderer::rhi::Format::eR16_UINT:
                return DXGI_FORMAT_R16_UINT;
            case renderer::rhi::Format::eR16_SINT:
                return DXGI_FORMAT_R16_SINT;
            case renderer::rhi::Format::eRG16_UINT:
                return DXGI_FORMAT_R16G16_UINT;
            case renderer::rhi::Format::eRG16_SINT:
                return DXGI_FORMAT_R16G16_SINT;
            case renderer::rhi::Format::eRGBA16_UINT:
                return DXGI_FORMAT_R16G16B16A16_UINT;
            case renderer::rhi::Format::eRGBA16_SINT:
                return DXGI_FORMAT_R16G16B16A16_SINT;

            case renderer::rhi::Format::eR16_FLOAT:
                return DXGI_FORMAT_R16_FLOAT;
            case renderer::rhi::Format::eRG16_FLOAT:
                return DXGI_FORMAT_R16G16_FLOAT;
            case renderer::rhi::Format::eRGBA16_FLOAT:
                return DXGI_FORMAT_R16G16B16A16_FLOAT;

            case renderer::rhi::Format::eR32_FLOAT:
                return DXGI_FORMAT_R32_FLOAT;
            case renderer::rhi::Format::eRG32_FLOAT:
                return DXGI_FORMAT_R32G32_FLOAT;
            case renderer::rhi::Format::eRGB32_FLOAT:
                return DXGI_FORMAT_R32G32B32_FLOAT;
            case renderer::rhi::Format::eRGBA32_FLOAT:
                return DXGI_FORMAT_R32G32B32A32_FLOAT;

            case renderer::rhi::Format::eR32_UINT:
                return DXGI_FORMAT_R32_UINT;
            case renderer::rhi::Format::eR32_SINT:
                return DXGI_FORMAT_R32_SINT;
            case renderer::rhi::Format::eRG32_UINT:
                return DXGI_FORMAT_R32G32_UINT;
            case renderer::rhi::Format::eRG32_SINT:
                return DXGI_FORMAT_R32G32_SINT;
            case renderer::rhi::Format::eRGB32_UINT:
                return DXGI_FORMAT_R32G32B32_UINT;
            case renderer::rhi::Format::eRGB32_SINT:
                return DXGI_FORMAT_R32G32B32_SINT;
            case renderer::rhi::Format::eRGBA32_UINT:
                return DXGI_FORMAT_R32G32B32A32_UINT;
            case renderer::rhi::Format::eRGBA32_SINT:
                return DXGI_FORMAT_R32G32B32A32_SINT;

            case renderer::rhi::Format::eR10G10B10A2_UNORM:
                return DXGI_FORMAT_R10G10B10A2_UNORM;
            case renderer::rhi::Format::eR11G11B10_FLOAT:
                return DXGI_FORMAT_R11G11B10_FLOAT;

            case renderer::rhi::Format::eBGRA4_UNORM:
                return DXGI_FORMAT_B4G4R4A4_UNORM;
            case renderer::rhi::Format::eB5G6R5_UNORM:
                return DXGI_FORMAT_B5G6R5_UNORM;
            case renderer::rhi::Format::eB5G5R5A1_UNORM:
                return DXGI_FORMAT_B5G5R5A1_UNORM;

            case renderer::rhi::Format::eBGRX8_UNORM:
                return DXGI_FORMAT_B8G8R8X8_UNORM;
            case renderer::rhi::Format::eSRGBA8_UNORM:
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case renderer::rhi::Format::eSBGRA8_UNORM:
                return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            case renderer::rhi::Format::eSBGRX8_UNORM:
                return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

            // Depth / stencil
            case renderer::rhi::Format::eD16:
                return DXGI_FORMAT_D16_UNORM;
            case renderer::rhi::Format::eD24S8:
                return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case renderer::rhi::Format::eX24G8_UINT:
                return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
            case renderer::rhi::Format::eD32:
                return DXGI_FORMAT_D32_FLOAT;
            case renderer::rhi::Format::eD32S8:
                return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            case renderer::rhi::Format::eX32G8_UINT:
                return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

            // BC compressed
            case renderer::rhi::Format::eBC1_UNORM:
                return DXGI_FORMAT_BC1_UNORM;
            case renderer::rhi::Format::eBC1_UNORM_SRGB:
                return DXGI_FORMAT_BC1_UNORM_SRGB;
            case renderer::rhi::Format::eBC2_UNORM:
                return DXGI_FORMAT_BC2_UNORM;
            case renderer::rhi::Format::eBC2_UNORM_SRGB:
                return DXGI_FORMAT_BC2_UNORM_SRGB;
            case renderer::rhi::Format::eBC3_UNORM:
                return DXGI_FORMAT_BC3_UNORM;
            case renderer::rhi::Format::eBC3_UNORM_SRGB:
                return DXGI_FORMAT_BC3_UNORM_SRGB;
            case renderer::rhi::Format::eBC4_UNORM:
                return DXGI_FORMAT_BC4_UNORM;
            case renderer::rhi::Format::eBC4_SNORM:
                return DXGI_FORMAT_BC4_SNORM;
            case renderer::rhi::Format::eBC5_UNORM:
                return DXGI_FORMAT_BC5_UNORM;
            case renderer::rhi::Format::eBC5_SNORM:
                return DXGI_FORMAT_BC5_SNORM;
            case renderer::rhi::Format::eBC6H_UFLOAT:
                return DXGI_FORMAT_BC6H_UF16;
            case renderer::rhi::Format::eBC6H_SFLOAT:
                return DXGI_FORMAT_BC6H_SF16;
            case renderer::rhi::Format::eBC7_UNORM:
                return DXGI_FORMAT_BC7_UNORM;
            case renderer::rhi::Format::eBC7_UNORM_SRGB:
                return DXGI_FORMAT_BC7_UNORM_SRGB;

            case renderer::rhi::Format::eCount:
            default:
                return DXGI_FORMAT_UNKNOWN;
        }

        MKT_ASSERT( false, "Unsupported TextureFormat for D3D11" );
        return DXGI_FORMAT_UNKNOWN;
    }

    auto GetUsageFromHeapType( renderer::rhi::HeapType type ) -> D3D11_USAGE {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::HeapType::eDeviceLocal:
                return D3D11_USAGE_DEFAULT;
            case renderer::rhi::HeapType::eUpload:
                return D3D11_USAGE_DYNAMIC;
            case renderer::rhi::HeapType::eReadback:
                return D3D11_USAGE_STAGING;
        }

        return D3D11_USAGE_IMMUTABLE;
    }

    auto GetBindFlags( renderer::rhi::BufferUsageFlags flags ) -> UINT {
        MKT_BEGIN_PROFILER();
        UINT result{};

        if ( flags.Has( renderer::rhi::BufferUsageFlagsBits::Vertex ) ) {
            result |= D3D11_BIND_VERTEX_BUFFER;
        }

        if ( flags.Has( renderer::rhi::BufferUsageFlagsBits::Index ) ) {
            result |= D3D11_BIND_INDEX_BUFFER;
        }

        if ( flags.Has( renderer::rhi::BufferUsageFlagsBits::Constant ) ) {
            result |= D3D11_BIND_CONSTANT_BUFFER;
        }

        if ( flags.Has( renderer::rhi::BufferUsageFlagsBits::Structured ) ) {
            result |= D3D11_BIND_UNORDERED_ACCESS;
        }

        return result;
    }

    auto GetBindFlags( renderer::rhi::TextureUsageFlags flags ) -> D3D11_BIND_FLAG {
        MKT_BEGIN_PROFILER();
        UINT result{};

        // Either render target or depth
        if ( flags.Has( renderer::rhi::TextureUsageFlagsBits::RenderTarget ) ) {
            result = result | D3D11_BIND_RENDER_TARGET;
        }

        if ( flags.Has( renderer::rhi::TextureUsageFlagsBits::DepthTarget |
                        renderer::rhi::TextureUsageFlagsBits::DepthStencilTarget |
                        renderer::rhi::TextureUsageFlagsBits::StencilTarget ) ) {
            result = D3D11_BIND_DEPTH_STENCIL;
        }

        if ( flags.Has( renderer::rhi::TextureUsageFlagsBits::ShaderResource ) ) {
            result = result | D3D11_BIND_SHADER_RESOURCE;
        }

        return core::as<D3D11_BIND_FLAG>( result );
    }

    auto GetPrimitiveTopology( renderer::rhi::PrimitiveTopology topology ) -> D3D11_PRIMITIVE_TOPOLOGY {
        MKT_BEGIN_PROFILER();

        switch ( topology ) {
            case renderer::rhi::PrimitiveTopology::eTriangleList:
                return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

            case renderer::rhi::PrimitiveTopology::eTriangleStrip:
                return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

            case renderer::rhi::PrimitiveTopology::eLineList:
                return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

            default:
                return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        }

        return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    auto GetCullMode( renderer::rhi::CullMode mode ) -> D3D11_CULL_MODE {
        MKT_BEGIN_PROFILER();

        switch ( mode ) {
            case renderer::rhi::CullMode::eCullFront:
                return D3D11_CULL_FRONT;
            case renderer::rhi::CullMode::eCullBack:
                return D3D11_CULL_BACK;
            default:;
        }

        return D3D11_CULL_NONE;
    }

    auto GetComparisonFunc( renderer::rhi::CompareOp op ) -> D3D11_COMPARISON_FUNC {
        MKT_BEGIN_PROFILER();

        switch ( op ) {
            case renderer::rhi::CompareOp::eNever:
                return D3D11_COMPARISON_NEVER;

            case renderer::rhi::CompareOp::eLess:
                return D3D11_COMPARISON_LESS;

            case renderer::rhi::CompareOp::eEqual:
                return D3D11_COMPARISON_EQUAL;

            case renderer::rhi::CompareOp::eLessOrEqual:
                return D3D11_COMPARISON_LESS_EQUAL;

            case renderer::rhi::CompareOp::eGreater:
                return D3D11_COMPARISON_GREATER;

            case renderer::rhi::CompareOp::eNotEqual:
                return D3D11_COMPARISON_NOT_EQUAL;

            case renderer::rhi::CompareOp::eGreaterOrEqual:
                return D3D11_COMPARISON_GREATER_EQUAL;

            case renderer::rhi::CompareOp::eAlways:
                return D3D11_COMPARISON_ALWAYS;

            default:
                return D3D11_COMPARISON_ALWAYS;
        }
    }

    auto GetFilter( renderer::rhi::SamplerFilter min, renderer::rhi::SamplerFilter mag ) -> D3D11_FILTER {
        MKT_BEGIN_PROFILER();

        if ( min == renderer::rhi::SamplerFilter::eNearest && mag == renderer::rhi::SamplerFilter::eNearest ) {
            return D3D11_FILTER_MIN_MAG_MIP_POINT;
        }

        if ( min == renderer::rhi::SamplerFilter::eLinear && mag == renderer::rhi::SamplerFilter::eLinear ) {
            return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        }

        if ( min == renderer::rhi::SamplerFilter::eNearest && mag == renderer::rhi::SamplerFilter::eLinear ) {
            return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        }

        if ( min == renderer::rhi::SamplerFilter::eLinear && mag == renderer::rhi::SamplerFilter::eNearest ) {
            return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        }

        return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    }

    auto GetAddressMode( renderer::rhi::SamplerWrapMode mode ) -> D3D11_TEXTURE_ADDRESS_MODE {
        MKT_BEGIN_PROFILER();

        switch ( mode ) {
            case renderer::rhi::SamplerWrapMode::eRepeat:
                return D3D11_TEXTURE_ADDRESS_WRAP;
            case renderer::rhi::SamplerWrapMode::eClampToEdge:
                return D3D11_TEXTURE_ADDRESS_CLAMP;
            case renderer::rhi::SamplerWrapMode::eMirroredRepeat:
                return D3D11_TEXTURE_ADDRESS_MIRROR;
            case renderer::rhi::SamplerWrapMode::eClampToBorder:
                return D3D11_TEXTURE_ADDRESS_BORDER;
            default:
                return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }

    auto GetBytesPerPixel( rhi::Format type ) -> UINT {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::Format::eRGBA8_SNORM:
            case renderer::rhi::Format::eRGBA8_UNORM:
                return 4;
            case renderer::rhi::Format::eRGBA32_FLOAT:
            case renderer::rhi::Format::eRGBA32_SINT:
            case renderer::rhi::Format::eRGBA32_UINT:
                return 16;
            default:;
        }

        MKT_ASSERT( false, "Unsupported format for bytes per pixel" );
        return 0;
    }

    auto GetInputElementDescription( const renderer::rhi::VertexAttributeDescription &desc, renderer::rhi::InputRate rate ) -> D3D11_INPUT_ELEMENT_DESC {
        MKT_BEGIN_PROFILER();
        D3D11_INPUT_ELEMENT_DESC result{};

        const auto &formatInfo{ rhi::GetFormatInfo( desc.mFormat ) };

        result.SemanticName = desc.mName.data();
        result.SemanticIndex = desc.mLocation;

        result.Format = GetFormat( desc.mFormat );

        result.InputSlot = desc.mBinding;
        result.AlignedByteOffset = desc.mOffset;

        if ( rate == renderer::rhi::InputRate::ePerVertex ) {
            result.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            result.InstanceDataStepRate = 0;
        }
        else {
            result.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
            result.InstanceDataStepRate = 1;
        }

        return result;
    }

    auto ParseSemantic( eastl::string_view value ) -> Semantic {
        MKT_BEGIN_PROFILER();
        Semantic result{};

        if ( value.empty() ) {
            return result;
        }

        core::usize splitPos = value.size();

        // Walk backwards to find where digits start
        while ( splitPos > 0 && string::IsNumber( value[splitPos - 1] ) ) {
            --splitPos;
        }

        // Name part
        result.mName = value.substr( 0, splitPos );

        // Index part (if any)
        if ( splitPos < value.size() ) {
            uint32_t index{ 0 };

            for ( core::usize i = splitPos; i < value.size(); ++i ) {
                index = index * 10 + ( value[i] - '0' );
            }

            result.mIndex = index;
        }
        else {
            result.mIndex = 0;
        }

        return result;
    }
}// namespace mikoto::renderer::d3d11

#endif
