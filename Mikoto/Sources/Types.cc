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
#include <Core/String.hh>

#include <Renderer/Rhi/Types.hh>

namespace mikoto::renderer::rhi {
    using namespace mikoto::core;

    auto BufferRange::SetByteOffset( u64 value ) -> BufferRange & {
        mByteOffset = value;
        return *this;
    }

    auto BufferRange::SetByteSize( u64 value ) -> BufferRange & {
        mByteSize = value;
        return *this;
    }

    auto BufferRange::Validate( size_t bufferByteSize ) -> BufferRange & {
        // Check specified offset is not out of bounds
        mByteOffset = eastl::min(mByteOffset, bufferByteSize);

        // If offset is 0 it means whole range
        // otherwise pick a slice
        if (mByteOffset != 0) {
            mByteSize = eastl::min(mByteSize, bufferByteSize - mByteOffset);
        } else {
            mByteSize = bufferByteSize;
        }

        return *this;
    }

    Color::operator float4() const {
        return float4{ mR, mG, mB, mA };
    }

    auto InferAPI( eastl::string_view apiName ) -> GraphicsAPI {
        if ( apiName.empty() ) {
            return GraphicsAPI::eInvalid;
        }

        eastl::string normalized{ apiName };
        eastl::transform(
                normalized.begin(),
                normalized.end(),
                normalized.begin(),
                []( char c ) {
                    return as<char>( string::ToLower( c ) );
                } );

        if ( normalized == "vulkan" || normalized == "vk" ) {
            return GraphicsAPI::eVulkan;
        }

        if ( normalized == "d3d12" || normalized == "directx12" || normalized == "direct3d12" ) {
            return GraphicsAPI::eD3D12;
        }

        if ( normalized == "d3d11" || normalized == "directx11" || normalized == "direct3d11" ) {
            return GraphicsAPI::eD3D11;
        }

        return GraphicsAPI::eInvalid;
    }

    // Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
    static constexpr FormatInfo kFormatInfo[]{
        //        format                   name             bytes blk         kind               red   green   blue  alpha  depth  stencil signed  srgb
        { Format::eUnknown, "UNKNOWN", 0, 0, FormatKind::Integer, false, false, false, false, false, false, false, false },
        { Format::eR8_UINT, "R8_UINT", 1, 1, FormatKind::Integer, true, false, false, false, false, false, false, false },
        { Format::eR8_SINT, "R8_SINT", 1, 1, FormatKind::Integer, true, false, false, false, false, false, true, false },
        { Format::eR8_UNORM, "R8_UNORM", 1, 1, FormatKind::Normalized, true, false, false, false, false, false, false, false },
        { Format::eR8_SNORM, "R8_SNORM", 1, 1, FormatKind::Normalized, true, false, false, false, false, false, true, false },
        { Format::eRG8_UINT, "RG8_UINT", 2, 1, FormatKind::Integer, true, true, false, false, false, false, false, false },
        { Format::eRG8_SINT, "RG8_SINT", 2, 1, FormatKind::Integer, true, true, false, false, false, false, true, false },
        { Format::eRG8_UNORM, "RG8_UNORM", 2, 1, FormatKind::Normalized, true, true, false, false, false, false, false, false },
        { Format::eRG8_SNORM, "RG8_SNORM", 2, 1, FormatKind::Normalized, true, true, false, false, false, false, true, false },
        { Format::eR16_UINT, "R16_UINT", 2, 1, FormatKind::Integer, true, false, false, false, false, false, false, false },
        { Format::eR16_SINT, "R16_SINT", 2, 1, FormatKind::Integer, true, false, false, false, false, false, true, false },
        { Format::eR16_UNORM, "R16_UNORM", 2, 1, FormatKind::Normalized, true, false, false, false, false, false, false, false },
        { Format::eR16_SNORM, "R16_SNORM", 2, 1, FormatKind::Normalized, true, false, false, false, false, false, true, false },
        { Format::eR16_FLOAT, "R16_FLOAT", 2, 1, FormatKind::Float, true, false, false, false, false, false, true, false },
        { Format::eBGRA4_UNORM, "BGRA4_UNORM", 2, 1, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eB5G6R5_UNORM, "B5G6R5_UNORM", 2, 1, FormatKind::Normalized, true, true, true, false, false, false, false, false },
        { Format::eB5G5R5A1_UNORM, "B5G5R5A1_UNORM", 2, 1, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eRGBA8_UINT, "RGBA8_UINT", 4, 1, FormatKind::Integer, true, true, true, true, false, false, false, false },
        { Format::eRGBA8_SINT, "RGBA8_SINT", 4, 1, FormatKind::Integer, true, true, true, true, false, false, true, false },
        { Format::eRGBA8_UNORM, "RGBA8_UNORM", 4, 1, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eRGBA8_SNORM, "RGBA8_SNORM", 4, 1, FormatKind::Normalized, true, true, true, true, false, false, true, false },
        { Format::eBGRA8_UNORM, "BGRA8_UNORM", 4, 1, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eBGRX8_UNORM, "BGRX8_UNORM", 4, 1, FormatKind::Normalized, true, true, true, false, false, false, false, false },
        { Format::eSRGBA8_UNORM, "SRGBA8_UNORM", 4, 1, FormatKind::Normalized, true, true, true, true, false, false, false, true },
        { Format::eSBGRA8_UNORM, "SBGRA8_UNORM", 4, 1, FormatKind::Normalized, true, true, true, true, false, false, false, true },
        { Format::eSBGRX8_UNORM, "SBGRX8_UNORM", 4, 1, FormatKind::Normalized, true, true, true, false, false, false, false, true },
        { Format::eR10G10B10A2_UNORM, "R10G10B10A2_UNORM", 4, 1, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eR11G11B10_FLOAT, "R11G11B10_FLOAT", 4, 1, FormatKind::Float, true, true, true, false, false, false, false, false },
        { Format::eRG16_UINT, "RG16_UINT", 4, 1, FormatKind::Integer, true, true, false, false, false, false, false, false },
        { Format::eRG16_SINT, "RG16_SINT", 4, 1, FormatKind::Integer, true, true, false, false, false, false, true, false },
        { Format::eRG16_UNORM, "RG16_UNORM", 4, 1, FormatKind::Normalized, true, true, false, false, false, false, false, false },
        { Format::eRG16_SNORM, "RG16_SNORM", 4, 1, FormatKind::Normalized, true, true, false, false, false, false, true, false },
        { Format::eRG16_FLOAT, "RG16_FLOAT", 4, 1, FormatKind::Float, true, true, false, false, false, false, true, false },
        { Format::eR32_UINT, "R32_UINT", 4, 1, FormatKind::Integer, true, false, false, false, false, false, false, false },
        { Format::eR32_SINT, "R32_SINT", 4, 1, FormatKind::Integer, true, false, false, false, false, false, true, false },
        { Format::eR32_FLOAT, "R32_FLOAT", 4, 1, FormatKind::Float, true, false, false, false, false, false, true, false },
        { Format::eRGBA16_UINT, "RGBA16_UINT", 8, 1, FormatKind::Integer, true, true, true, true, false, false, false, false },
        { Format::eRGBA16_SINT, "RGBA16_SINT", 8, 1, FormatKind::Integer, true, true, true, true, false, false, true, false },
        { Format::eRGBA16_FLOAT, "RGBA16_FLOAT", 8, 1, FormatKind::Float, true, true, true, true, false, false, true, false },
        { Format::eRGBA16_UNORM, "RGBA16_UNORM", 8, 1, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eRGBA16_SNORM, "RGBA16_SNORM", 8, 1, FormatKind::Normalized, true, true, true, true, false, false, true, false },
        { Format::eRG32_UINT, "RG32_UINT", 8, 1, FormatKind::Integer, true, true, false, false, false, false, false, false },
        { Format::eRG32_SINT, "RG32_SINT", 8, 1, FormatKind::Integer, true, true, false, false, false, false, true, false },
        { Format::eRG32_FLOAT, "RG32_FLOAT", 8, 1, FormatKind::Float, true, true, false, false, false, false, true, false },
        { Format::eRGB32_UINT, "RGB32_UINT", 12, 1, FormatKind::Integer, true, true, true, false, false, false, false, false },
        { Format::eRGB32_SINT, "RGB32_SINT", 12, 1, FormatKind::Integer, true, true, true, false, false, false, true, false },
        { Format::eRGB32_FLOAT, "RGB32_FLOAT", 12, 1, FormatKind::Float, true, true, true, false, false, false, true, false },
        { Format::eRGBA32_UINT, "RGBA32_UINT", 16, 1, FormatKind::Integer, true, true, true, true, false, false, false, false },
        { Format::eRGBA32_SINT, "RGBA32_SINT", 16, 1, FormatKind::Integer, true, true, true, true, false, false, true, false },
        { Format::eRGBA32_FLOAT, "RGBA32_FLOAT", 16, 1, FormatKind::Float, true, true, true, true, false, false, true, false },
        { Format::eD16, "D16", 2, 1, FormatKind::DepthStencil, false, false, false, false, true, false, false, false },
        { Format::eD24S8, "D24S8", 4, 1, FormatKind::DepthStencil, false, false, false, false, true, true, false, false },
        { Format::eX24G8_UINT, "X24G8_UINT", 4, 1, FormatKind::Integer, false, false, false, false, false, true, false, false },
        { Format::eD32, "D32", 4, 1, FormatKind::DepthStencil, false, false, false, false, true, false, false, false },
        { Format::eD32S8, "D32S8", 8, 1, FormatKind::DepthStencil, false, false, false, false, true, true, false, false },
        { Format::eX32G8_UINT, "X32G8_UINT", 8, 1, FormatKind::Integer, false, false, false, false, false, true, false, false },
        { Format::eBC1_UNORM, "BC1_UNORM", 8, 4, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eBC1_UNORM_SRGB, "BC1_UNORM_SRGB", 8, 4, FormatKind::Normalized, true, true, true, true, false, false, false, true },
        { Format::eBC2_UNORM, "BC2_UNORM", 16, 4, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eBC2_UNORM_SRGB, "BC2_UNORM_SRGB", 16, 4, FormatKind::Normalized, true, true, true, true, false, false, false, true },
        { Format::eBC3_UNORM, "BC3_UNORM", 16, 4, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eBC3_UNORM_SRGB, "BC3_UNORM_SRGB", 16, 4, FormatKind::Normalized, true, true, true, true, false, false, false, true },
        { Format::eBC4_UNORM, "BC4_UNORM", 8, 4, FormatKind::Normalized, true, false, false, false, false, false, false, false },
        { Format::eBC4_SNORM, "BC4_SNORM", 8, 4, FormatKind::Normalized, true, false, false, false, false, false, true, false },
        { Format::eBC5_UNORM, "BC5_UNORM", 16, 4, FormatKind::Normalized, true, true, false, false, false, false, false, false },
        { Format::eBC5_SNORM, "BC5_SNORM", 16, 4, FormatKind::Normalized, true, true, false, false, false, false, true, false },
        { Format::eBC6H_UFLOAT, "BC6H_UFLOAT", 16, 4, FormatKind::Float, true, true, true, false, false, false, false, false },
        { Format::eBC6H_SFLOAT, "BC6H_SFLOAT", 16, 4, FormatKind::Float, true, true, true, false, false, false, true, false },
        { Format::eBC7_UNORM, "BC7_UNORM", 16, 4, FormatKind::Normalized, true, true, true, true, false, false, false, false },
        { Format::eBC7_UNORM_SRGB, "BC7_UNORM_SRGB", 16, 4, FormatKind::Normalized, true, true, true, true, false, false, false, true },
    };

    auto GetFormatInfo( Format format ) -> const FormatInfo & {
        static_assert( sizeof( kFormatInfo ) / sizeof( FormatInfo ) == size_t( Format::eCount ),
                       "The format info table doesn't have the right number of elements" );

        if ( uint32_t( format ) >= uint32_t( Format::eCount ) )
            return kFormatInfo[0];// UNKNOWN

        const FormatInfo &info = kFormatInfo[uint32_t( format )];
        assert( info.mFormat == format );
        return info;
    }

    auto InferDimensions( RenderResolution resolution ) -> eastl::pair<float, float> {
        switch ( resolution ) {
            case RenderResolution::e720P:
                return eastl::make_pair( 1280.0f, 720.0f );
            case RenderResolution::e1080P:
                return eastl::make_pair( 1920.0f, 1080.0f );
            case RenderResolution::e1440P:
                return eastl::make_pair( 2560.0f, 1440.0f );
            case RenderResolution::e3120P:
                return eastl::make_pair( 3840.0f, 2160.0f );
        }

        return eastl::make_pair( 1920.0f, 1080.0f );
    }

    auto InferDimensions( RenderResolution resolution, u32 mipLevel ) -> eastl::pair<u32, u32> {
        auto result{ InferDimensions( resolution ) };

        const u32 baseWidth{ as<u32>( result.first ) };
        const u32 baseHeight{ as<u32>( result.second ) };

        u32 width{ eastl::max( 1u, baseWidth >> mipLevel ) };
        u32 height{ eastl::max( 1u, baseHeight >> mipLevel ) };

        return eastl::make_pair( width, height );
    }

    auto InferDimensions( float width, float height, u32 mipLevel ) -> eastl::pair<u32, u32> {
        u32 baseWidth{ as<u32>( width ) };
        u32 baseHeight{ as<u32>( height ) };

        u32 mipWidth{ eastl::max( 1u, baseWidth >> mipLevel ) };
        u32 mipHeight{ eastl::max( 1u, baseHeight >> mipLevel ) };

        return { mipWidth, mipHeight };
    }

    auto InferElementCount( Format dataType, size_t sizeBytes ) -> size_t {
        const FormatInfo &info{ GetFormatInfo( dataType ) };
        return sizeBytes / info.mBytesPerBlock;
    }

    auto IsBuffer( ResourceType type ) noexcept -> bool {
        return type >= ResourceType::eTypedBuffer_SRV &&
                   type <= ResourceType::eConstantBuffer;
    }

    auto IsTexture( ResourceType type ) noexcept -> bool {
        return (type >= ResourceType::eTexture_SRV && type <= ResourceType::eTexture_UAV) ||
                   type == ResourceType::eSamplerFeedbackTexture_UAV;
    }

    auto IsSampler( ResourceType type ) noexcept -> bool {
        return type == ResourceType::eSampler ||
                   type == ResourceType::eSamplerFeedbackTexture_UAV;
    }


    auto IsMultiple( core::usize value, core::usize compare ) -> bool {
        return value % compare == 0;
    }

    auto NextMultiple( core::usize value, core::usize multiple ) -> core::usize {
        if (value % multiple == 0) {
            return value;
        }

        return value - (value % multiple) + multiple;
    }
    
}