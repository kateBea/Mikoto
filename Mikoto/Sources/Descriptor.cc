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

#include <Memory/BufferSpan.hh>

#include <Renderer/Rhi/Descriptor.hh>

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;
    using namespace mikoto::memory;

    auto BindingSetItem::Texture_SRV( u32 slot, ITexture *texture, Format format, TextureSubresourceSet subResources, TextureDimension dimension ) -> BindingSetItem {
        return BindingSetItem{
            .mResource = texture,
            .mSlot = slot,
            .mType = ResourceType::eTexture_SRV,
            .mFormat = format,
            .mDimension = dimension,
            .mSubResourceSet = subResources
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

    auto BindingLayoutDescription::SetShaderVisibility( ShaderFlags visibility ) -> BindingLayoutDescription & {
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
        mImageHandle = std::move( image );

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetUsage( TextureUsageFlagsBits::kCopyDst );

        return *this;
    }

    auto TextureCreateDescription::SetBufferData( BufferSpanHandle buffer ) -> TextureCreateDescription & {
        mBufferSpan = std::move( buffer );

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

    auto TextureCreateDescription::SetSubResources( const TextureSubresourceSet &subResources ) -> TextureCreateDescription & {
        mSubresourceSet = subResources;
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
        if ( mCpuAccess == CpuAccessType::eWrite && mHeapType == HeapType::eDeviceLocal ) {
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
    auto BindlessLayoutItem::Texture_SRV( u32 slot, u32 maxCapacity ) -> BindlessLayoutItem {
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

    auto BindlessLayoutDescription::SetVisibility( ShaderFlags visibility ) -> BindlessLayoutDescription & {
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