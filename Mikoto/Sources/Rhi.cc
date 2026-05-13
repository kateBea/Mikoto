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

#include <EASTL/algorithm.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>

#include <Core/String.hh>
#include <Renderer/Core/Rhi.hh>
#include <utility>

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;

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

    auto BindingSetItem::Texture_SRV( u32 slot, ITexture *texture, Format format, TextureSubresourceSet subResources, TextureDimension dimension ) -> BindingSetItem {
        return BindingSetItem{
            .mResource = texture,
            .mSlot = slot,
            .mType = ResourceType::eTexture_SRV,
            .mFormat = format,
            .mDimension =  dimension,
            .mSubResourceSet = subResources,
        };
    }

    auto BindingSetItem::Sampler( u32 slot, ISampler *sampler ) -> BindingSetItem {
        return BindingSetItem{
            .mResource = sampler,
            .mSlot = slot,
            .mType = ResourceType::eSampler,
        };
    }

    auto BindingSetItem::StructuredBuffer_SRV( u32 slot, IBuffer *buffer, BufferRange range ) -> BindingSetItem {
        return BindingSetItem{
            .mResource = buffer,
            .mSlot = slot,
            .mRange = range,
            .mType = ResourceType::eStructuredBuffer_SRV,
        };
    }

    auto BindingSetItem::StructuredBuffer_UAV( u32 slot, IBuffer *buffer, BufferRange range ) -> BindingSetItem {
        return BindingSetItem{
            .mResource = buffer,
            .mSlot = slot,
            .mRange = range,
            .mType = ResourceType::eStructuredBuffer_UAV,
        };
    }

    auto BindingLayoutDescription::SetRegisterSpace( u32 group ) -> BindingLayoutDescription & {
        mRegisterSpace = group;
        return *this;
    }

    auto BindingLayoutDescription::AddItem( const BindingLayoutItem &item ) -> BindingLayoutDescription & {
        mBindings.emplace_back( item );
        return *this;
    }

    auto BindingLayoutDescription::SetShaderVisibility( ShaderStage visibility ) -> BindingLayoutDescription & {
        mStageVisibility = visibility;
        return *this;
    }

    auto BindingLayoutItem::Texture_SRV( u32 slot ) -> BindingLayoutItem {
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eTexture_SRV,
        };

        return result;
    }

    auto BindingLayoutItem::Sampler( u32 slot ) -> BindingLayoutItem {
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eSampler,
        };

        return result;
    }

    auto BindingLayoutItem::ConstantBuffer( u32 slot ) -> BindingLayoutItem {
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eConstantBuffer,
        };

        return result;
    }
    auto BindingLayoutItem::StructuredBuffer_SRV( u32 slot ) -> BindingLayoutItem {
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eStructuredBuffer_SRV,
        };

        return result;
    }

    auto BindingLayoutItem::StructuredBuffer_UAV( u32 slot ) -> BindingLayoutItem {
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eStructuredBuffer_UAV,
        };

        return result;
    }

    auto BindingSetItem::ConstantBuffer( u32 slot, IBuffer *buffer, BufferRange range ) -> BindingSetItem {
        BindingSetItem result{
            .mResource = buffer,
            .mSlot = slot,
            .mType = ResourceType::eConstantBuffer,
        };

        return result;
    }

    auto TextureCreateDescription::SetWidth( u32 width ) -> TextureCreateDescription & {
        mWidth = width;
        return *this;
    }

    auto TextureCreateDescription::SetName( eastl::string_view name ) -> TextureCreateDescription & {
        mName = name;
        return *this;
    }

    auto TextureCreateDescription::SetHeight( u32 height ) -> TextureCreateDescription & {
        mHeight = height;
        return *this;
    }

    auto TextureCreateDescription::SetMipCount( u32 count ) -> TextureCreateDescription & {
        mMipCount = count;
        return *this;
    }

    auto TextureCreateDescription::SetKeepInitializerResources( bool value ) -> TextureCreateDescription & {
        mKeepInitializerResources = value;
        return *this;
    }

    auto TextureCreateDescription::SetMultisampling( Multisampling sampleCount ) -> TextureCreateDescription & {
        mMSAA = sampleCount;
        return *this;
    }

    auto TextureCreateDescription::SetImageData( asset::ImageHandle image ) -> TextureCreateDescription & {
        mImageHandle = std::move(image);

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetUsage( TextureUsageFlagsBits::kCopyDst );

        return *this;
    }

    auto TextureCreateDescription::SetBufferData( BufferSpanHandle buffer ) -> TextureCreateDescription & {
        mBufferSpan = std::move(buffer);

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetUsage( TextureUsageFlagsBits::kCopyDst );

        return *this;
    }

    auto TextureCreateDescription::SetHeapType( HeapType heapType ) -> TextureCreateDescription & {
        mHeapType = heapType;
        return *this;
    }

    auto TextureCreateDescription::SetFormat( Format format ) -> TextureCreateDescription & {
        mFormat = format;
        return *this;
    }

    auto TextureCreateDescription::SetDimensions( TextureDimension dimensions ) -> TextureCreateDescription & {
        mDimension = dimensions;
        return *this;
    }

    auto TextureCreateDescription::SetUsage( TextureUsageFlags usage ) -> TextureCreateDescription & {
        mUsage |= usage;
        return *this;
    }

    auto TextureCreateDescription::SetResourceType( ResourceType usage ) -> TextureCreateDescription & {
        mResourceType = usage;
        return *this;
    }

    auto BufferCreateDescription::SetName( eastl::string_view name ) -> BufferCreateDescription & {
        mName = name;
        return *this;
    }

    auto BufferCreateDescription::ForElement( size_t byteSize, size_t count ) -> BufferCreateDescription & {
        if ( byteSize != 0 && count != 0 ) {
            mElementCount = count;
            mElementSize = byteSize;
        }
        return *this;
    }

    auto BufferCreateDescription::SetByteSize( size_t byteSize ) -> BufferCreateDescription & {
        mElementSize = byteSize;
        return *this;
    }

    auto BufferCreateDescription::SetFormat( Format format ) -> BufferCreateDescription & {
        mFormat = format;
        return *this;
    }

    auto BufferCreateDescription::SetInitialData( BufferSpanHandle data ) -> BufferCreateDescription & {
        mSpanHandle = data;
        mElementSize = data->GetSize();

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetBufferUsage( BufferUsageFlagsBits::kCopyDst );

        return *this;
    }

    auto BufferCreateDescription::SetBufferUsage( BufferUsageFlags usage ) -> BufferCreateDescription & {
        mUsageFlags |= usage;
        return *this;
    }

    auto BufferCreateDescription::SetBufferDataType( BufferDataType type ) -> BufferCreateDescription & {
        mDataType = type;
        return *this;
    }

    auto BufferCreateDescription::SetHeapType( HeapType type ) -> BufferCreateDescription & {
        mHeapType = type;
        return *this;
    }

    auto BufferCreateDescription::SetCpuAccessType( CpuAccessType type ) -> BufferCreateDescription & {
        mCpuAccess = type;

        // Cannot have a CPU writeable memory that is in VRAM
        if (mCpuAccess == CpuAccessType::eWrite && mHeapType == HeapType::eDeviceLocal) {
            mHeapType = HeapType::eUpload;
        }

        return *this;
    }

    auto BufferCreateDescription::SetResourceType( ResourceType type ) -> BufferCreateDescription & {
        mResourceType = type;
        return *this;
    }

    auto BufferCreateDescription::SetKeepInitializerResources( bool value ) -> BufferCreateDescription & {
        mKeepInitializerResources = value;
        return *this;
    }

    auto BufferCreateDescription::SetIsVolatile( bool value ) -> BufferCreateDescription & {
        mIsVolatile = value;
        return *this;
    }

    auto BufferCreateDescription::SetMaxVersions( u32 count ) -> BufferCreateDescription & {
        mMaxVersions = count;
        return *this;
    }

    auto ComputePipelineDescription::SetUseReflection( bool value ) -> ComputePipelineDescription & {
        mUseReflection = value;
        return *this;
    }

    auto ComputePipelineDescription::SetComputeStage( ShaderModuleHandle handle ) -> ComputePipelineDescription & {
        mStage = eastl::move( handle );
        return *this;
    }

    auto ComputePipelineDescription::SetPipelineLayout( PipelineLayoutHandle handle ) -> ComputePipelineDescription & {
        mPipelineLayout = eastl::move( handle );
        return *this;
    }

    auto GraphicsPipelineDescription::SetPipelineLayout( PipelineLayoutHandle handle ) -> GraphicsPipelineDescription & {
        mPipelineLayout = eastl::move( handle );
        return *this;
    }

    auto GraphicsPipelineDescription::SetInputLayout( InputLayoutHandle handle ) -> GraphicsPipelineDescription & {
        mInputLayout = eastl::move( handle );
        return *this;
    }

    auto GraphicsPipelineDescription::SetPolygonMode( PolygonMode mode ) -> GraphicsPipelineDescription & {
        mPolygonMode = mode;
        return *this;
    }

    auto GraphicsPipelineDescription::SetTopology( PrimitiveTopology topology ) -> GraphicsPipelineDescription & {
        mPrimitiveTopology = topology;
        return *this;
    }

    auto GraphicsPipelineDescription::SetUseReflection( bool value ) -> GraphicsPipelineDescription & {
        mUseReflection = value;
        return *this;
    }

    auto GraphicsPipelineDescription::AddShader( ShaderModuleHandle handle ) -> GraphicsPipelineDescription & {
        mShaders[handle->GetType()] = handle;
        return *this;
    }

    auto GraphicsPipelineDescription::SetCullMode( CullMode mode ) -> GraphicsPipelineDescription & {
        mCullMode = mode;
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthTest( bool value ) -> GraphicsPipelineDescription & {
        mEnableDepthTest = value;
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthWrite( bool value ) -> GraphicsPipelineDescription & {
        mEnableDepthWrite = value;
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthFormat( Format format ) -> GraphicsPipelineDescription & {
        mDepthFormat = format;
        return *this;
    }

    auto GraphicsPipelineDescription::AddColorFormat( Format format ) -> GraphicsPipelineDescription & {
        mColorFormats.push_back( format );
        return *this;
    }

    auto GraphicsPipelineDescription::SetWindingOrder( WindingOrder order ) -> GraphicsPipelineDescription & {
        mWindingOrder = order;
        return *this;
    }

    IGraphicsPipeline::IGraphicsPipeline( const GraphicsPipelineDescription &desc )
        : IPipeline{ PipelineType::eGraphics }, mDesc{ desc } {}

    auto IGraphicsPipeline::GetDescription() const noexcept -> const GraphicsPipelineDescription & {
        return mDesc;
    }

    auto IGraphicsPipeline::GetPipelineLayout() const -> PipelineLayoutHandle {
        return mDesc.mPipelineLayout;
    }

    auto IComputePipeline::GetDescription() const noexcept -> const ComputePipelineDescription & {
        return mDesc;
    }

    auto IComputePipeline::GetPipelineLayout() const -> PipelineLayoutHandle {
        return mDesc.mPipelineLayout;
    }

    auto GraphicsState::SetScopeName( eastl::string_view name ) -> GraphicsState & {
        mName = name;
        return *this;
    }

    auto GraphicsState::SetRenderArea( const Rect &rec ) -> GraphicsState & {
        mRenderArea = rec;
        return *this;
    }

    auto GraphicsState::AddDepthTarget( TextureHandle target, LoadOp op ) -> GraphicsState & {
        mDepthTarget = RenderTargetState{
            .mClearColor = kColorWhite,
            .mLoadOp = op,
            .mRenderTarget = target,
        };

        return *this;
    }

    auto GraphicsState::AddRenderTarget( TextureHandle target, const Color &c, LoadOp op, TextureSubresourceSet set ) -> GraphicsState & {
        mCurrentRenderTargets.emplace_back( RenderTargetState{
            .mClearColor = c,
            .mLoadOp = op,
            .mRenderTarget = std::move(target),
            .mSubresourceSet = set,
        });

        return *this;
    }

    auto BindResourcesDescription::SetPipelineLayout( IPipelineLayout *layout ) -> BindResourcesDescription & {
        mPipelineLayout = layout;
        return *this;
    }

    auto BindResourcesDescription::SetBindPoint(PipelineType bindPoint) -> BindResourcesDescription& {
        mBindPoint = bindPoint;
        return *this;
    }

    auto BindResourcesDescription::SetPushConstants( const void *ptr, size_t sizeBytes, ShaderStage stage ) -> BindResourcesDescription & {
        MKT_ASSERT( sizeBytes <= sizeof( mPushConstants ), "Exceeded push constants size" );
        mPushConstantVisibility = stage;
        eastl::copy_n( as<byte_t*>( ptr ), sizeBytes, mPushConstants.data() );
        return *this;
    }

    auto BindResourcesDescription::AddResourceSet( u32 bindingIndex, IBindingSet* set ) -> BindResourcesDescription& {
        MKT_ASSERT(set, "ResourceSet is null");
        mResourceSets.insert_or_assign(bindingIndex, ResourceSet{
            .mResourceSet = set
        });

        return *this;
    }

    auto ShaderModuleCreateDescription::SetFile( filesystem::FileHandle file ) -> ShaderModuleCreateDescription & {
        mFile = std::move( file );
        return *this;
    }

    auto ShaderModuleCreateDescription::SetStage( ShaderType stage ) -> ShaderModuleCreateDescription & {
        mType = stage;
        return *this;
    }

    auto ShaderModuleCreateDescription::SetEntryPoint( eastl::string_view name ) -> ShaderModuleCreateDescription & {
        mEntryPoint = name;
        return *this;
    }

    auto ShaderModuleCreateDescription::SetIsSlang( bool value ) -> ShaderModuleCreateDescription & {
        mIsSlangShader = value;
        return *this;
    }

    auto PipelineLayoutCreateDescription::SetPushConstantsVisibility( ShaderStage stage ) -> PipelineLayoutCreateDescription & {
        mPushConstantsVisibility = stage;
        return *this;
    }

    auto PipelineLayoutCreateDescription::AddBindingLayout( BindingLayoutHandle layout ) -> PipelineLayoutCreateDescription & {
        mBindingLayouts.emplace_back( layout );
        return *this;
    }

    auto SamplerCreateDescription::SetMipLevels( float mipLevels ) -> SamplerCreateDescription & {
        mMipLevels = mipLevels;
        return *this;
    }

    auto SamplerCreateDescription::SetBorderColor( const Color &color ) -> SamplerCreateDescription & {
        mBorderColor = color;
        return *this;
    }

    auto SamplerCreateDescription::SetFilter( SamplerFilter filter ) -> SamplerCreateDescription & {
        mMinFilter = filter;
        mMagFilter = filter;
        return *this;
    }

    auto SamplerCreateDescription::SetMinFilter( SamplerFilter filter ) -> SamplerCreateDescription & {
        mMinFilter = filter;
        return *this;
    }

    auto SamplerCreateDescription::SetMagFilter( SamplerFilter filter ) -> SamplerCreateDescription & {
        mMagFilter = filter;
        return *this;
    }

    auto SamplerCreateDescription::SetWrap( SamplerWrapMode wrap ) -> SamplerCreateDescription & {
        mWrapU = wrap;
        mWrapV = wrap;
        mWrapW = wrap;
        return *this;
    }

    auto SamplerCreateDescription::SetWrapU( SamplerWrapMode wrap ) -> SamplerCreateDescription & {
        mWrapU = wrap;
        return *this;
    }

    auto SamplerCreateDescription::SetWrapV( SamplerWrapMode wrap ) -> SamplerCreateDescription & {
        mWrapV = wrap;
        return *this;
    }

    auto SamplerCreateDescription::SetWrapW( SamplerWrapMode wrap ) -> SamplerCreateDescription & {
        mWrapW = wrap;
        return *this;
    }

    auto VertexBindingDescription::SetBinding( u32 binding ) -> VertexBindingDescription & {
        mBinding = binding;
        return *this;
    }

    auto VertexBindingDescription::SetStride( u32 stride ) -> VertexBindingDescription & {
        mStride = stride;
        return *this;
    }

    auto VertexBindingDescription::SetInputRate( InputRate rate ) -> VertexBindingDescription & {
        mRate = rate;
        return *this;
    }

    auto VertexAttributeDescription::SetName( eastl::string name ) -> VertexAttributeDescription & {
        mName = name;
        return *this;
    }

    auto VertexAttributeDescription::SetLocation( uint32_t loc ) -> VertexAttributeDescription & {
        mLocation = loc;
        return *this;
    }

    auto VertexAttributeDescription::SetBinding( uint32_t binding ) -> VertexAttributeDescription & {
        mBinding = binding;
        return *this;
    }

    auto VertexAttributeDescription::SetFormat( Format value ) -> VertexAttributeDescription & {
        mFormat = value;
        return *this;
    }

    auto VertexAttributeDescription::SetOffset( u32 value ) -> VertexAttributeDescription & {
        mOffset = value;
        return *this;
    }

    auto InputLayoutCreateDescription::SetShader( ShaderModuleHandle shader ) -> InputLayoutCreateDescription & {
        mShaderModule = shader;
        return *this;
    }

    auto InputLayoutCreateDescription::SetBindings( eastl::span<const VertexBindingDescription> items ) -> InputLayoutCreateDescription & {
        mVertexBindingDescriptions.insert( mVertexBindingDescriptions.end(), items.begin(), items.end() );
        return *this;
    }

    auto InputLayoutCreateDescription::SetAttributes( eastl::span<const VertexAttributeDescription> items ) -> InputLayoutCreateDescription & {
        mVertexAttributeDescriptions.insert( mVertexAttributeDescriptions.end(), items.begin(), items.end() );
        return *this;
    }

    auto InputLayoutCreateDescription::PushBinding( const VertexBindingDescription &desc ) -> InputLayoutCreateDescription & {
        mVertexBindingDescriptions.emplace_back( desc );
        return *this;
    }

    auto InputLayoutCreateDescription::PushAttribute( const VertexAttributeDescription &desc ) -> InputLayoutCreateDescription& {
        mVertexAttributeDescriptions.emplace_back( desc );
        return *this;
    }

    auto BindingSetDescription::AddItem( const BindingSetItem &value ) -> BindingSetDescription & {
        mBindings.emplace_back( value );
        return *this;
    }

    auto BindingSetDescription::AddShader( ShaderModuleHandle shader ) -> BindingSetDescription & {
        mShaders.emplace_back( shader );
        return *this;
    }

    auto BindlessLayoutItem::Samplers( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eSampler
        };
    }

    // --- Textures ---
    auto BindlessLayoutItem::Texture_SRV( u32 slot, u32 maxCapacity) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eTexture_SRV,
        };
    }

    auto BindlessLayoutItem::Texture_UAV( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eTexture_UAV,
        };
    }

    // --- Constant Buffers ---
    auto BindlessLayoutItem::ConstantBuffer( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eConstantBuffer
        };
    }

    auto BindlessLayoutItem::ConstantBuffer_UAV( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eStructuredBuffer_UAV
        };
    }

    // --- Raw Buffers ---
    auto BindlessLayoutItem::Buffer_SRV( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eRawBuffer_SRV
        };
    }

    auto BindlessLayoutItem::Buffer_UAV( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eRawBuffer_UAV
        };
    }

    // --- Structured Buffers ---
    auto BindlessLayoutItem::StructuredBuffer_SRV( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eStructuredBuffer_SRV
        };
    }

    auto BindlessLayoutItem::StructuredBuffer_UAV( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eStructuredBuffer_UAV
        };
    }

    // --- Acceleration Structures ---
    auto BindlessLayoutItem::AccelerationStructures( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mType = ResourceType::eRayTracingAccelStruct
        };
    }

    auto BindlessLayoutDescription::SetDebugName( eastl::string_view name ) -> BindlessLayoutDescription & {
        mName = name;
        return *this;
    }

    auto BindlessLayoutDescription::SetRegisterSpace( u32 registerSpace ) -> BindlessLayoutDescription & {
        mRegisterSpace = registerSpace;
        return *this;
    }

    auto BindlessLayoutDescription::SetVisibility( ShaderStage visibility ) -> BindlessLayoutDescription & {
        mStageVisibility = visibility;
        return *this;
    }

    auto BindlessLayoutDescription::AddBindlessItem( const BindlessLayoutItem &item ) -> BindlessLayoutDescription & {
        mSlots.emplace_back( item );
        return *this;
    }

    auto BindlessLayoutDescription::AddShader( ShaderModuleHandle shader ) -> BindlessLayoutDescription & {
        mShaders.emplace_back( shader );
        return *this;
    }
}// namespace mikoto::renderer::rhi