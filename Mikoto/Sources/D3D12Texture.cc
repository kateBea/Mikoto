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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <D3D12MemAlloc.h>

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Texture.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    Sampler::Sampler( const rhi::SamplerCreateDescription& desc, DeviceResources* resources )
        : ISampler{ desc }, mResources{ resources }
    {

    }

    auto Sampler::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        return ISampler::GetNativeHandle( type );
    }

    auto Sampler::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        return ISampler::GetNativeHandle( type );
    }

    Sampler::~Sampler() {
        if (mIsAllocated) {
            Release();
        }
    }
    auto Sampler::Release() -> void {
        mIsAllocated = false;
    }

    auto Sampler::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device1* d3d12Device{ device->GetDevice() };

        D3D12_SAMPLER_DESC wrapSamplerDesc{};
        wrapSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        wrapSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        wrapSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        wrapSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        wrapSamplerDesc.MinLOD = 0;
        wrapSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        wrapSamplerDesc.MipLODBias = 0.0f;
        wrapSamplerDesc.MaxAnisotropy = 1;
        wrapSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        wrapSamplerDesc.BorderColor[0] = wrapSamplerDesc.BorderColor[1] = wrapSamplerDesc.BorderColor[2] = wrapSamplerDesc.BorderColor[3] = 0;

        d3d12Device->CreateSampler(&wrapSamplerDesc, mSamplerHandle);
        mIsAllocated = true;
    }

    Texture::Texture( const TextureCreateDescription& desc, DeviceResources& resources)
        : ITexture{ desc }, mResources{ MKT_ADDRESSOF( resources ) }, mIsExternalImage{ false }
    {

    }

    Texture::Texture( const ExternalTextureDescription& info, DeviceResources& resources )
        : ITexture{
            TextureCreateDescription{}
            .SetWidth( info.mWidth )
            .SetHeight( info.mHeight )
            .SetFormat( info.mFormat ) },
        mResources{ MKT_ADDRESSOF( resources ) },
        mIsExternalImage{ true } {

        mImageAllocation.mResource = info.mImageResource;

        mTextureUsage = info.mTextureUsage;
        mDebugName = string::Format( "External Texture" );
    }

    auto Texture::SetDebugName( const eastl::string_view name ) -> void {
        mImageAllocation.mResource->SetName( string::ToWide( name ).c_str() );
    }

    auto Texture::GetNativeHandle( ObjectType object ) -> Object {
        return ITexture::GetNativeHandle( object );
    }

    auto Texture::GetNativeHandle( ObjectType object ) const -> Object {
        return ITexture::GetNativeHandle( object );
    }

    auto Texture::CreateSRV( SIZE_T descriptor, TextureSubresourceSet subResources, Format format, TextureDimension dimension ) const -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};

        if (dimension == TextureDimension::eInvalid) {
            dimension = mDimension;
        }

        viewDesc.Format = d3d12::GetFormat(format == Format::eUnknown ? mFormat : format);
        viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        u32 planeSlice{ (viewDesc.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT) ? 1u : 0u };

        switch ( dimension ) {
            case TextureDimension::eTexture1D:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture1D.MipLevels = subResources.mNumMipLevels;
                break;
            case TextureDimension::eTexture1DArray:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                viewDesc.Texture1DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture1DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture1DArray.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture1DArray.MipLevels = subResources.mNumMipLevels;
                break;
            case TextureDimension::eTexture2D:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture2D.MipLevels = subResources.mNumMipLevels;
                viewDesc.Texture2D.PlaneSlice = planeSlice;
                break;
            case TextureDimension::eTexture2DArray:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture2DArray.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture2DArray.MipLevels = subResources.mNumMipLevels;
                viewDesc.Texture2DArray.PlaneSlice = planeSlice;
                break;
            case TextureDimension::eTextureCube:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                viewDesc.TextureCube.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.TextureCube.MipLevels = subResources.mNumMipLevels;
                break;
            case TextureDimension::eTextureCubeArray:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                viewDesc.TextureCubeArray.First2DArrayFace = subResources.mBaseArraySlice;
                viewDesc.TextureCubeArray.NumCubes = subResources.mNumArraySlices / 6;
                viewDesc.TextureCubeArray.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.TextureCubeArray.MipLevels = subResources.mNumMipLevels;
                break;
            case TextureDimension::eTexture2DMS:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                break;
            case TextureDimension::eTexture2DMSArray:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                viewDesc.Texture2DMSArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DMSArray.ArraySize = subResources.mNumArraySlices;
                break;
            case TextureDimension::eTexture3D:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture3D.MipLevels = subResources.mNumMipLevels;
                break;
            case TextureDimension::eInvalid:
            default:
                return;
        }

        d3d12Device->CreateShaderResourceView(mImageAllocation.mResource.Get(), &viewDesc, { descriptor });
    }

    auto Texture::CreateUAV( SIZE_T descriptor, TextureSubresourceSet subResources, Format format, TextureDimension dimension ) const -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};

        if (dimension == TextureDimension::eInvalid) {
            dimension = mDimension;
        }

        viewDesc.Format = d3d12::GetFormat(format == Format::eUnknown ? mFormat : format);

        switch ( dimension ) {
            case TextureDimension::eTexture1D:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture1DArray:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                viewDesc.Texture1DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture1DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture1DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2D:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2DArray:
            case TextureDimension::eTextureCube:
            case TextureDimension::eTextureCubeArray:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture2DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture3D:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.FirstWSlice = 0;
                viewDesc.Texture3D.WSize = 0; // TODO: Texture Depth
                viewDesc.Texture3D.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2DMS:
            case TextureDimension::eTexture2DMSArray: {
                MKT_CORE_LOGGER_ERROR( "Unsupported dimension for UAV" );
                return;
            }
            case TextureDimension::eInvalid:
            default:
                return;
        }

        d3d12Device->CreateUnorderedAccessView(mImageAllocation.mResource.Get(), nullptr, &viewDesc, { descriptor });
    }

    auto Texture::CreateRTV( SIZE_T descriptor, TextureSubresourceSet subResources, Format format ) const -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        D3D12_RENDER_TARGET_VIEW_DESC viewDesc{};

        viewDesc.Format = d3d12::GetFormat(format == Format::eUnknown ? mFormat : format);

        switch ( mDimension ) {
            case TextureDimension::eTexture1D:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture1DArray:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                viewDesc.Texture1DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture1DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture1DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2D:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2DArray:
            case TextureDimension::eTextureCube:
            case TextureDimension::eTextureCubeArray:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture2DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2DMS:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                break;
            case TextureDimension::eTexture2DMSArray:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                viewDesc.Texture2DMSArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DMSArray.ArraySize = subResources.mNumArraySlices;
                break;
            case TextureDimension::eTexture3D:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.FirstWSlice = subResources.mBaseArraySlice;
                viewDesc.Texture3D.WSize = subResources.mNumArraySlices;
                viewDesc.Texture3D.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eInvalid:
            default:
                return;
        }

        d3d12Device->CreateRenderTargetView(mImageAllocation.mResource.Get(), &viewDesc, { descriptor });
    }

    auto Texture::CreateDSV( SIZE_T descriptor, TextureSubresourceSet subResources, bool isReadOnly ) const -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};

        viewDesc.Format = d3d12::GetFormat(mFormat);

        if (isReadOnly) {
            viewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;

            if (viewDesc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || viewDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) {
                viewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
            }
        }

        switch ( mDimension ) {
            case TextureDimension::eTexture1D:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture1DArray:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                viewDesc.Texture1DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture1DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture1DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2D:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2DArray:
            case TextureDimension::eTextureCube:
            case TextureDimension::eTextureCubeArray:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture2DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case TextureDimension::eTexture2DMS:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
                break;
            case TextureDimension::eTexture2DMSArray:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                viewDesc.Texture2DMSArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DMSArray.ArraySize = subResources.mNumArraySlices;
                break;
            case TextureDimension::eTexture3D: {
                MKT_CORE_LOGGER_ERROR( "Unsupported dimension for DSV" );

                return;
            }
            case TextureDimension::eInvalid:
            default:
                return;
        }

        d3d12Device->CreateDepthStencilView(mImageAllocation.mResource.Get(), &viewDesc, { descriptor });
    }

    auto Texture::GetRtvDescriptorIndex() const -> DescriptorIndex {
        return mRtvDescriptorIndex;
    }

    auto Texture::GetDsvDescriptorIndex() const -> DescriptorIndex {
        return mDsvDescriptorIndex;
    }

    Texture::operator ID3D12Resource*() const {
        return mImageAllocation.mResource.Get();
    }

    Texture::~Texture() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Texture::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };

        if (!mIsExternalImage) {
            mImageAllocation.mDesc.Dimension = d3d12::GetDimension(mDimension);
            mImageAllocation.mDesc.Alignment = 0;
            mImageAllocation.mDesc.Width = mWidth;
            mImageAllocation.mDesc.Height = mHeight;
            mImageAllocation.mDesc.DepthOrArraySize = 1;
            mImageAllocation.mDesc.MipLevels = mMipCount;
            mImageAllocation.mDesc.Format = d3d12::GetFormat(mFormat);

            mImageAllocation.mDesc.SampleDesc.Quality = 0;
            mImageAllocation.mDesc.SampleDesc.Count = d3d12::GetSampleCount(mMultisampling);

            mImageAllocation.mDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            mImageAllocation.mDesc.Flags = d3d12::GetResourceFlags(mTextureUsage);

            mImageAllocation.mAllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

            auto* allocator{ device->GetAllocator() };
            ThrowIfFailed( allocator->AllocateImage( mImageAllocation ) );
        }

        // Create the descriptor when the resource already exists to not create a null view
        if (mTextureUsage & rhi::TextureUsageFlagsBits::kRenderTarget) {
            mRtvDescriptorIndex = mResources->mRenderTargetViewHeap->AllocateDescriptor();
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{ mResources->mRenderTargetViewHeap->GetCpuHandle(mRtvDescriptorIndex) };
            CreateRTV(cpuHandle.ptr, mSubResources, mFormat );
        }

        if (mTextureUsage & rhi::TextureUsageFlagsBits::kDepthTarget || mTextureUsage & rhi::TextureUsageFlagsBits::kDepthStencilTarget) {
            mDsvDescriptorIndex = mResources->mDepthStencilViewHeap->AllocateDescriptor();
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{ mResources->mDepthStencilViewHeap->GetCpuHandle(mDsvDescriptorIndex) };
            CreateDSV(cpuHandle.ptr, mSubResources );
        }

        // Fill initial contents
        if ( !mImageData.IsEmpty() ) {
            if (mDimension == TextureDimension::eTexture2D) {
                InitInitialData2D( mImageData->mBufferSpan );
            } else if (mDimension == TextureDimension::eTextureCube) {
                InitInitialDataCube( mImageData->mBufferSpan );
            }
        }

        if ( !mBufferSpan.IsEmpty() ) {
            if (mDimension == TextureDimension::eTexture2D) {
                InitInitialData2D( mBufferSpan );
            } else if (mDimension == TextureDimension::eTextureCube) {
                InitInitialDataCube( mBufferSpan );
            }
        }

        if (!mKeepInitializerResources) {
            mImageData.Reset();
            mBufferSpan.Reset();
        }

        mIsAllocated = true;
    }

    auto Texture::Release() -> void {
        mResources->mDepthStencilViewHeap->ReleaseDescriptor( mDsvDescriptorIndex );
        mResources->mRenderTargetViewHeap->ReleaseDescriptor( mRtvDescriptorIndex );

        mIsAllocated = false;
    }

    auto Texture::InitInitialData2D( BufferSpanHandle buffer ) -> void {
        u64 fenceValue{ 0 };
        FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
        CommandListHandle cmd{ mDevice->CreateCommandList( QueueType::eTransfer ) };
        cmd->Begin( { .mScopeName = string::Format( "Texture upload: {}", mDebugName ) } );

        // Data is always writen at mip zero
        cmd->Write( this, 0, buffer->GetData(), buffer->GetSize() );

        // These textures are often loaded to be read from shaders
        cmd->SetTransition( this, ResourceStates::eShaderResource );

        cmd->End();

        // Signal fenceValue on one fence on completion of these
        // commands, then we wait for that completion this blocks the caller
        // but client should ideally offload this task to worker threads
        const auto submitInfo{ SubmitInfo{}
            .AddSignal( fence, fenceValue )
            .AddCommandList( cmd ) };
        mDevice->GetQueue( QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
        ( void )fence->Wait( fenceValue, eastl::numeric_limits<u64>::max() ); // Host wait
    }

    auto Texture::InitInitialDataCube( BufferSpanHandle buffer ) -> void {

    }
}// namespace mikoto::renderer::d3d12

#endif
