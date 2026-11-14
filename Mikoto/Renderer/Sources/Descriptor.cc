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

#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Memory/BufferSpan.hh>

#include <Renderer/Rhi/Descriptor.hh>
#include <Renderer/Rhi/AccelerationStructure.hh>

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;
    using namespace mikoto::memory;

    auto BindingTableItem::AccelerationStructure( core::u32 slot, IAccelerationStructure *structure ) -> BindingTableItem {
        return BindingTableItem{ .mBindingIndex = slot, .mResource = structure, .mResourceType = ResourceType::eRayTracingAccelStruct };
    }

    auto BindingTableItem::TextureSRV( core::u32 slot, ITexture *texture, Format format, TextureSubresourceSet subResources, TextureDimension dimension ) -> BindingTableItem {
        MKT_BEGIN_PROFILER();
        return BindingTableItem{
            .mBindingIndex = slot,
            .mResource = texture,
            .mResourceType = ResourceType::eTexture_SRV,
            .mFormat = format,
            .mDimension = dimension,
            .mSubResourceSet = subResources
        };
    }

    auto BindingTableItem::TextureUAV( core::u32 slot, ITexture *texture, Format format, TextureSubresourceSet subResources, TextureDimension dimension ) -> BindingTableItem {
        MKT_BEGIN_PROFILER();
        return BindingTableItem{
            .mBindingIndex = slot,
            .mResource = texture,
            .mResourceType = ResourceType::eTexture_UAV,
            .mFormat = format,
            .mDimension = dimension,
            .mSubResourceSet = subResources
        };
    }

    auto BindingTableItem::Sampler( core::u32 slot, ISampler *sampler ) -> BindingTableItem {
        MKT_BEGIN_PROFILER();
        return BindingTableItem{
            .mBindingIndex = slot,
            .mResource = sampler,
            .mResourceType = ResourceType::eSampler,
        };
    }

    auto BindingTableItem::StructuredSRV( core::u32 slot, IBuffer *buffer, BufferRange range ) -> BindingTableItem {
        MKT_BEGIN_PROFILER();
        return BindingTableItem{
            .mRange = range,
            .mBindingIndex = slot,
            .mResource = buffer,
            .mResourceType = ResourceType::eStructuredBuffer_SRV,
        };
    }

    auto BindingTableItem::StructuredUAV( core::u32 slot, IBuffer *buffer, BufferRange range ) -> BindingTableItem {
        MKT_BEGIN_PROFILER();
        return BindingTableItem{
            .mRange = range,
            .mBindingIndex = slot,
            .mResource = buffer,
            .mResourceType = ResourceType::eStructuredBuffer_UAV,
        };
    }

    auto BindingTableItem::RawBufferSRV( core::u32 slot, IBuffer *buffer, BufferRange range ) -> BindingTableItem {
        MKT_BEGIN_PROFILER();
        return BindingTableItem{
            .mRange = range,
            .mBindingIndex = slot,
            .mResource = buffer,
            .mResourceType = ResourceType::eRawBuffer_SRV,
        };
    }

    auto BindingTableItem::RawBufferUAV( core::u32 slot, IBuffer *buffer, BufferRange range ) -> BindingTableItem {
        MKT_BEGIN_PROFILER();
        return BindingTableItem{
            .mRange = range,
            .mBindingIndex = slot,
            .mResource = buffer,
            .mResourceType = ResourceType::eRawBuffer_UAV,
        };
    }

    auto BindingLayoutDescription::SetRegisterSpace( core::u32 group ) -> BindingLayoutDescription & {
        MKT_BEGIN_PROFILER();
        mRegisterSpace = group;
        return *this;
    }

    auto BindingLayoutDescription::AddItem( const BindingLayoutItem &item ) -> BindingLayoutDescription & {
        MKT_BEGIN_PROFILER();
        mBindings.emplace_back( item );
        return *this;
    }

    auto BindingLayoutDescription::SetShaderVisibility( ShaderFlags visibility ) -> BindingLayoutDescription & {
        MKT_BEGIN_PROFILER();
        mStageVisibility = visibility;
        return *this;
    }

    auto BindingLayoutItem::TextureSRV( core::u32 slot ) -> BindingLayoutItem {
        MKT_BEGIN_PROFILER();
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eTexture_SRV,
        };

        return result;
    }

    auto BindingLayoutItem::Sampler( core::u32 slot ) -> BindingLayoutItem {
        MKT_BEGIN_PROFILER();
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eSampler,
        };

        return result;
    }

    auto BindingLayoutItem::ConstantBuffer( core::u32 slot ) -> BindingLayoutItem {
        MKT_BEGIN_PROFILER();
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eConstantBuffer,
        };

        return result;
    }

    auto BindingLayoutItem::StructuredSRV( core::u32 slot ) -> BindingLayoutItem {
        MKT_BEGIN_PROFILER();
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eStructuredBuffer_SRV,
        };

        return result;
    }

    auto BindingLayoutItem::StructuredUAV( core::u32 slot ) -> BindingLayoutItem {
        MKT_BEGIN_PROFILER();
        BindingLayoutItem result{
            .mSlot = slot,
            .mType = ResourceType::eStructuredBuffer_UAV,
        };

        return result;
    }

    auto BindingTableItem::ConstantBuffer( core::u32 slot, IBuffer *buffer, BufferRange range ) -> BindingTableItem {
        MKT_BEGIN_PROFILER();
        BindingTableItem result{
            .mBindingIndex = slot,
            .mResource = buffer,
            .mResourceType = ResourceType::eConstantBuffer,
        };

        return result;
    }

    auto TextureCreateDescription::SetWidth( core::u32 width ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mWidth = width;
        return *this;
    }

    auto TextureCreateDescription::SetName( eastl::string_view name ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto TextureCreateDescription::SetHeight( core::u32 height ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mHeight = height;
        return *this;
    }

    auto TextureCreateDescription::SetMipCount( core::u32 count ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mMipCount = count;
        return *this;
    }

    auto TextureCreateDescription::SetMultisampling( Multisampling sampleCount ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mMultisampling = sampleCount;
        return *this;
    }

    auto TextureCreateDescription::SetImageData( asset::ImageHandle image ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mImageHandle = eastl::move( image );

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetUsage( TextureUsageFlagsBits::CopyDest );

        return *this;
    }

    auto TextureCreateDescription::SetBufferData( memory::BufferSpanHandle buffer ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mBufferSpan = eastl::move( buffer );

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetUsage( TextureUsageFlagsBits::CopyDest );

        return *this;
    }

    auto TextureCreateDescription::SetHeapType( HeapType heapType ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mHeapType = heapType;
        return *this;
    }

    auto TextureCreateDescription::SetFormat( Format format ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mFormat = format;
        return *this;
    }

    auto TextureCreateDescription::SetDimensions( TextureDimension dimensions ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mDimension = dimensions;
        return *this;
    }

    auto TextureCreateDescription::SetUsage( TextureUsageFlags usage ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mUsage |= usage;
        return *this;
    }

    auto TextureCreateDescription::SetResourceType( ResourceType usage ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mResourceType = usage;
        return *this;
    }

    auto TextureCreateDescription::SetSubResources( const TextureSubresourceSet &subResources ) -> TextureCreateDescription & {
        MKT_BEGIN_PROFILER();
        mSubresourceSet = subResources;
        return *this;
    }

    auto BufferCreateDescription::SetName( eastl::string_view name ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto BufferCreateDescription::ForElement( core::usize byteSize, core::usize count ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();

        if ( byteSize != 0 && count != 0 ) {
            mElementCount = count;
            mElementSize = byteSize;
        }

        return *this;
    }

    auto BufferCreateDescription::SetByteSize( core::usize byteSize ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mElementSize = byteSize;
        return *this;
    }

    auto BufferCreateDescription::SetFormat( Format format ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mFormat = format;
        return *this;
    }

    auto BufferCreateDescription::SetInitialData( memory::BufferSpanHandle data ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mSpanHandle = data;
        mElementSize = data->GetSize();

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetBufferUsage( BufferUsageFlagsBits::CopyDest );

        return *this;
    }

    auto BufferCreateDescription::SetBufferUsage( BufferUsageFlags usage ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mUsageFlags |= usage;
        return *this;
    }

    auto BufferCreateDescription::SetBufferDataType( BufferDataType type ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mDataType = type;
        return *this;
    }

    auto BufferCreateDescription::SetHeapType( HeapType type ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mHeapType = type;
        return *this;
    }

    auto BufferCreateDescription::SetCpuAccessType( AccessType type ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mCpuAccess = type;

        // Cannot have a CPU writeable memory that is in VRAM
        if ( mCpuAccess == AccessType::eWrite && mHeapType == HeapType::eDeviceLocal ) {
            mHeapType = HeapType::eUpload;
        }

        return *this;
    }

    auto BufferCreateDescription::SetResourceType( ResourceType type ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mResourceType = type;
        return *this;
    }

    auto BufferCreateDescription::SetKeepInitializerResources( bool value ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mKeepInitializerResources = value;
        return *this;
    }

    auto BufferCreateDescription::SetIsVolatile( bool value ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mIsVolatile = value;
        return *this;
    }

    auto BufferCreateDescription::SetMaxVersions( core::u32 count ) -> BufferCreateDescription & {
        MKT_BEGIN_PROFILER();
        mMaxVersions = count;
        return *this;
    }

    auto SamplerCreateDescription::SetMipLevels( float mipLevels ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mMipLevels = mipLevels;
        return *this;
    }

    auto SamplerCreateDescription::SetBorderColor( const Color &color ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mBorderColor = color;
        return *this;
    }

    auto SamplerCreateDescription::SetMipmapMode( SamplerMipmapMode mode ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mMipMapMode = mode;
        return *this;
    }

    auto SamplerCreateDescription::SetFilter( SamplerFilter filter ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mMinFilter = filter;
        mMagFilter = filter;
        return *this;
    }

    auto SamplerCreateDescription::SetMinFilter( SamplerFilter filter ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mMinFilter = filter;
        return *this;
    }

    auto SamplerCreateDescription::SetMagFilter( SamplerFilter filter ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mMagFilter = filter;
        return *this;
    }

    auto SamplerCreateDescription::SetWrap( SamplerWrapMode wrap ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mWrapU = wrap;
        mWrapV = wrap;
        mWrapW = wrap;
        return *this;
    }

    auto SamplerCreateDescription::SetWrapU( SamplerWrapMode wrap ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mWrapU = wrap;
        return *this;
    }

    auto SamplerCreateDescription::SetWrapV( SamplerWrapMode wrap ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mWrapV = wrap;
        return *this;
    }

    auto SamplerCreateDescription::SetWrapW( SamplerWrapMode wrap ) -> SamplerCreateDescription & {
        MKT_BEGIN_PROFILER();
        mWrapW = wrap;
        return *this;
    }


    auto VertexBindingDescription::SetBinding( core::u32 binding ) -> VertexBindingDescription & {
        MKT_BEGIN_PROFILER();
        mBinding = binding;
        return *this;
    }

    auto VertexBindingDescription::SetStride( core::u32 stride ) -> VertexBindingDescription & {
        MKT_BEGIN_PROFILER();
        mStride = stride;
        return *this;
    }

    auto VertexBindingDescription::SetInputRate( InputRate rate ) -> VertexBindingDescription & {
        MKT_BEGIN_PROFILER();
        mRate = rate;
        return *this;
    }

    auto VertexAttributeDescription::SetName( eastl::string name ) -> VertexAttributeDescription & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto VertexAttributeDescription::SetLocation( core::u32 loc ) -> VertexAttributeDescription & {
        MKT_BEGIN_PROFILER();
        mLocation = loc;
        return *this;
    }

    auto VertexAttributeDescription::SetBinding( core::u32 binding ) -> VertexAttributeDescription & {
        MKT_BEGIN_PROFILER();
        mBinding = binding;
        return *this;
    }

    auto VertexAttributeDescription::SetFormat( Format value ) -> VertexAttributeDescription & {
        MKT_BEGIN_PROFILER();
        mFormat = value;
        return *this;
    }

    auto VertexAttributeDescription::SetOffset( core::u32 value ) -> VertexAttributeDescription & {
        MKT_BEGIN_PROFILER();
        mOffset = value;
        return *this;
    }

    auto BindingTableDescription::AddItem( const BindingTableItem &value ) -> BindingTableDescription & {
        MKT_BEGIN_PROFILER();
        mBindings.emplace_back( value );
        return *this;
    }

    auto BindingTableDescription::AddShader( ShaderModuleHandle shader ) -> BindingTableDescription & {
        MKT_BEGIN_PROFILER();
        mShaders.emplace_back( shader );
        return *this;
    }

    auto BindlessLayoutItem::Samplers( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eSampler
        };
    }

    auto BindlessLayoutItem::TextureSRV( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eTexture_SRV,
        };
    }

    auto BindlessLayoutItem::TextureUAV( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eTexture_UAV,
        };
    }

    auto BindlessLayoutItem::ConstantBuffer( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eConstantBuffer
        };
    }

    auto BindlessLayoutItem::RawBufferSRV( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eRawBuffer_SRV
        };
    }

    auto BindlessLayoutItem::RawBufferUAV( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eRawBuffer_UAV
        };
    }

    auto BindlessLayoutItem::StructuredSRV( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eStructuredBuffer_SRV
        };
    }

    auto BindlessLayoutItem::StructuredUAV( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eStructuredBuffer_UAV
        };
    }

    auto BindlessLayoutItem::AccelerationStructure( core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem {
        MKT_BEGIN_PROFILER();
        return {
            .mSlot = slot,
            .mMaxCapacity = maxCapacity,
            .mResourceType = ResourceType::eRayTracingAccelStruct
        };
    }

    auto DescriptorTableLayoutDescription::SetDebugName( eastl::string_view name ) -> DescriptorTableLayoutDescription & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto DescriptorTableLayoutDescription::SetRegisterSpace( core::u32 registerSpace ) -> DescriptorTableLayoutDescription & {
        MKT_BEGIN_PROFILER();
        mRegisterSpace = registerSpace;
        return *this;
    }

    auto DescriptorTableLayoutDescription::SetVisibility( ShaderFlags visibility ) -> DescriptorTableLayoutDescription & {
        MKT_BEGIN_PROFILER();
        mStageVisibility = visibility;
        return *this;
    }

    auto DescriptorTableLayoutDescription::AddBindlessItem( const BindlessLayoutItem &item ) -> DescriptorTableLayoutDescription & {
        MKT_BEGIN_PROFILER();
        mSlots.emplace_back( item );
        return *this;
    }

    auto DescriptorTableLayoutDescription::AddShader( ShaderModuleHandle shader ) -> DescriptorTableLayoutDescription & {
        MKT_BEGIN_PROFILER();
        mShaders.emplace_back( shader );
        return *this;
    }
}// namespace mikoto::renderer::rhi
