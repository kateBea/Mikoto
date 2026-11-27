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
#include <Core/Exception.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Types.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <D3D12MemAlloc.h>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <Platform/Windows/Windows.hh>

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Texture.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    Sampler::Sampler( const rhi::SamplerCreateDescription &desc, DeviceResources &resources )
        : mMipLevels{ desc.mMipLevels },
          mMinFilter{ desc.mMinFilter },
          mMagFilter{ desc.mMagFilter },
          mWrapU{ desc.mWrapU },
          mWrapV{ desc.mWrapV },
          mWrapW{ desc.mWrapW },
          mBorderColor{ desc.mBorderColor } {
        mSamplerDescription.Filter = d3d12::GetFilter( desc.mMinFilter, desc.mMagFilter, desc.mMipMapMode, desc.mCompareOp );
        mSamplerDescription.AddressU = d3d12::GetAddressMode( desc.mWrapU );
        mSamplerDescription.AddressV = d3d12::GetAddressMode( desc.mWrapV );
        mSamplerDescription.AddressW = d3d12::GetAddressMode( desc.mWrapW );
        mSamplerDescription.MinLOD = 0;
        mSamplerDescription.MaxLOD = D3D12_FLOAT32_MAX;
        mSamplerDescription.MipLODBias = 0.0f;
        mSamplerDescription.MaxAnisotropy = static_cast<UINT>( glm::clamp( desc.mMaxAnisotropy, 1.0f, 16.0f ) );
        if ( desc.mMaxAnisotropy > 1 && mMinFilter == rhi::SamplerFilter::eLinear && mMagFilter == rhi::SamplerFilter::eLinear ) {
            mSamplerDescription.Filter = desc.mCompareOp == rhi::CompareOp::eNever ? D3D12_FILTER_ANISOTROPIC : D3D12_FILTER_COMPARISON_ANISOTROPIC;
        }
        mSamplerDescription.ComparisonFunc = d3d12::GetCompareOp( desc.mCompareOp );
        mSamplerDescription.BorderColor[0] = mSamplerDescription.BorderColor[1] = mSamplerDescription.BorderColor[2] = mSamplerDescription.BorderColor[3] = 0;
    }

    auto Sampler::GetSamplerIndex() const -> DescriptorIndex {
        MKT_BEGIN_PROFILER();
        return mSamplerResourceIndex;
    }

    auto Sampler::AllocateSampler( D3D12_CPU_DESCRIPTOR_HANDLE handle ) -> void {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };
        ID3D12Device1 *d3d12Device{ device->GetDevice() };
        d3d12Device->CreateSampler( &mSamplerDescription, handle );
    }

    auto Sampler::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::ISampler::GetNativeHandle( type );
    }

    auto Sampler::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::ISampler::GetNativeHandle( type );
    }

    Sampler::~Sampler() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Sampler::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    auto Sampler::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        mIsAllocated = true;
    }

    auto Sampler::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        mDebugName = name;
    }

    Texture::Texture( const renderer::rhi::TextureCreateDescription &desc, DeviceResources &resources )
        : mWidth{ desc.mWidth },
          mHeight{ desc.mHeight },
          mDepth{ desc.mDimension == rhi::TextureDimension::eTexture3D ? desc.mDepth : 1u },
          mMipCount{ desc.mMipCount },
          mImageData{ desc.mImageHandle },
          mBufferSpan{ desc.mBufferSpan },
          mFormat{ desc.mFormat },
          mDimension{ desc.mDimension },
          mInitialState{ desc.mInitialState },
          mTextureUsage{ desc.mUsage },
          mMultisampling{ desc.mMultisampling },
          mSubResources{ desc.mSubresourceSet },
          mResources{ MKT_ADDRESSOF( resources ) },
          mIsExternalImage{ false } {
        mHeapType = desc.mHeapType;
        mResourceType = desc.mResourceType;
    }

    Texture::Texture( const ExternalTextureDescription &info, DeviceResources &resources )
        : Texture{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( info.mWidth )
            .SetHeight( info.mHeight )
            .SetFormat( info.mFormat ),
              resources } {
        mIsExternalImage = true;

        mImageAllocation.mResource = info.mImageResource;
        mExternalStorageOwner = info.mStorageOwner;
        mPlaneSlice = info.mPlaneSlice;

        mTextureUsage = info.mTextureUsage;
        mDebugName = string::Format( "External Texture" );
    }

    auto Texture::SetDebugName( const eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() ) {
            return;
        }

        mDebugName = name;
        mImageAllocation.mResource->SetName( string::ToWide( name ).c_str() );
    }

    auto Texture::GetNativeHandle( renderer::rhi::ObjectType object ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::ITexture::GetNativeHandle( object );
    }

    auto Texture::GetNativeHandle( renderer::rhi::ObjectType object ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::ITexture::GetNativeHandle( object );
    }

    auto Texture::NormalizeSubResources( renderer::rhi::TextureSubresourceSet range ) const -> renderer::rhi::TextureSubresourceSet {
        const auto desc{ mImageAllocation.mResource->GetDesc() };
        const core::u32 layers{ desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : desc.DepthOrArraySize };

        if ( range.mBaseMipLevel >= desc.MipLevels || range.mBaseArraySlice >= layers ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 texture view starts outside its resource." );
        }

        range.mNumMipLevels = glm::min( range.mNumMipLevels, core::u32( desc.MipLevels ) - range.mBaseMipLevel );
        range.mNumArraySlices = glm::min( range.mNumArraySlices, layers - range.mBaseArraySlice );

        if ( !range.mNumMipLevels || !range.mNumArraySlices ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 texture views must contain at least one subresource." );
        }

        return range;
    }

    auto Texture::CreateSrv( core::usize descriptor, renderer::rhi::TextureSubresourceSet subResources, renderer::rhi::Format format, renderer::rhi::TextureDimension dimension ) const -> void {
        MKT_BEGIN_PROFILER();
        subResources = NormalizeSubResources( subResources );
        Device *device{ core::checked_cast<Device *>( mDevice ) };
        ID3D12Device2 *d3d12Device{ device->GetDevice() };

        D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};

        if ( dimension == renderer::rhi::TextureDimension::eInvalid ) {
            dimension = mDimension;
        }

        if ( mMultisampling != renderer::rhi::Multisampling::eMsaaX1 ) {
            dimension = dimension == renderer::rhi::TextureDimension::eTexture2DArray || dimension == renderer::rhi::TextureDimension::eTexture2DMSArray ? renderer::rhi::TextureDimension::eTexture2DMSArray : renderer::rhi::TextureDimension::eTexture2DMS;
        }

        viewDesc.Format = d3d12::GetFormat( format == renderer::rhi::Format::eUnknown ? mFormat : format );

        switch ( viewDesc.Format ) {
            case DXGI_FORMAT_D32_FLOAT:
                viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
                break;
            case DXGI_FORMAT_D16_UNORM:
                viewDesc.Format = DXGI_FORMAT_R16_UNORM;
                break;
            case DXGI_FORMAT_D24_UNORM_S8_UINT:
                viewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                break;
            case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
                viewDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                break;
            default:
                break;
        }

        viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        core::u32 planeSlice{ mPlaneSlice + ( viewDesc.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT ? 1u : 0u ) };

        switch ( dimension ) {
            case renderer::rhi::TextureDimension::eTexture1D:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture1D.MipLevels = subResources.mNumMipLevels;
                break;
            case renderer::rhi::TextureDimension::eTexture1DArray:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                viewDesc.Texture1DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture1DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture1DArray.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture1DArray.MipLevels = subResources.mNumMipLevels;
                break;
            case renderer::rhi::TextureDimension::eTexture2D:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture2D.MipLevels = subResources.mNumMipLevels;
                viewDesc.Texture2D.PlaneSlice = planeSlice;
                break;
            case renderer::rhi::TextureDimension::eTexture2DArray:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture2DArray.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture2DArray.MipLevels = subResources.mNumMipLevels;
                viewDesc.Texture2DArray.PlaneSlice = planeSlice;
                break;
            case renderer::rhi::TextureDimension::eTextureCube:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                viewDesc.TextureCube.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.TextureCube.MipLevels = subResources.mNumMipLevels;
                break;
            case renderer::rhi::TextureDimension::eTextureCubeArray:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                viewDesc.TextureCubeArray.First2DArrayFace = subResources.mBaseArraySlice;
                viewDesc.TextureCubeArray.NumCubes = subResources.mNumArraySlices / 6;
                viewDesc.TextureCubeArray.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.TextureCubeArray.MipLevels = subResources.mNumMipLevels;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMS:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMSArray:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                viewDesc.Texture2DMSArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DMSArray.ArraySize = subResources.mNumArraySlices;
                break;
            case renderer::rhi::TextureDimension::eTexture3D:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MostDetailedMip = subResources.mBaseMipLevel;
                viewDesc.Texture3D.MipLevels = subResources.mNumMipLevels;
                break;
            case renderer::rhi::TextureDimension::eInvalid:
            default:
                return;
        }

        d3d12Device->CreateShaderResourceView( mImageAllocation.mResource.Get(), &viewDesc, { descriptor } );
    }

    auto Texture::CreateUav( core::usize descriptor, renderer::rhi::TextureSubresourceSet subResources, renderer::rhi::Format format, renderer::rhi::TextureDimension dimension ) const -> void {
        MKT_BEGIN_PROFILER();
        subResources = NormalizeSubResources( subResources );
        Device *device{ core::checked_cast<Device *>( mDevice ) };
        ID3D12Device2 *d3d12Device{ device->GetDevice() };

        D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};

        if ( dimension == renderer::rhi::TextureDimension::eInvalid ) {
            dimension = mDimension;
        }

        viewDesc.Format = d3d12::GetFormat( format == renderer::rhi::Format::eUnknown ? mFormat : format );

        switch ( dimension ) {
            case renderer::rhi::TextureDimension::eTexture1D:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture1DArray:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                viewDesc.Texture1DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture1DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture1DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2D:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2DArray:
            case renderer::rhi::TextureDimension::eTextureCube:
            case renderer::rhi::TextureDimension::eTextureCubeArray:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture2DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture3D:
                viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.FirstWSlice = 0;
                viewDesc.Texture3D.WSize = UINT( -1 );
                viewDesc.Texture3D.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMS:
            case renderer::rhi::TextureDimension::eTexture2DMSArray: {
                MKT_CORE_LOGGER_ERROR( "Unsupported dimension for UAV" );
                return;
            }

            case renderer::rhi::TextureDimension::eInvalid:
            default:
                return;
        }

        d3d12Device->CreateUnorderedAccessView( mImageAllocation.mResource.Get(), nullptr, &viewDesc, { descriptor } );
    }

    auto Texture::CreateRtv( core::usize descriptor, renderer::rhi::TextureSubresourceSet subResources, renderer::rhi::Format format ) const -> void {
        MKT_BEGIN_PROFILER();
        subResources = NormalizeSubResources( subResources );
        Device *device{ core::checked_cast<Device *>( mDevice ) };
        ID3D12Device2 *d3d12Device{ device->GetDevice() };

        D3D12_RENDER_TARGET_VIEW_DESC viewDesc{};

        viewDesc.Format = d3d12::GetFormat( format == renderer::rhi::Format::eUnknown ? mFormat : format );

        auto dimension{ mDimension };

        if ( mMultisampling != renderer::rhi::Multisampling::eMsaaX1 ) {
            dimension = dimension == renderer::rhi::TextureDimension::eTexture2DArray || dimension == renderer::rhi::TextureDimension::eTexture2DMSArray ? renderer::rhi::TextureDimension::eTexture2DMSArray : renderer::rhi::TextureDimension::eTexture2DMS;
        }

        switch ( dimension ) {
            case renderer::rhi::TextureDimension::eTexture1D:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture1DArray:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                viewDesc.Texture1DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture1DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture1DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2D:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2DArray:
            case renderer::rhi::TextureDimension::eTextureCube:
            case renderer::rhi::TextureDimension::eTextureCubeArray:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture2DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMS:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMSArray:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                viewDesc.Texture2DMSArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DMSArray.ArraySize = subResources.mNumArraySlices;
                break;
            case renderer::rhi::TextureDimension::eTexture3D:
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.FirstWSlice = subResources.mBaseArraySlice;
                viewDesc.Texture3D.WSize = subResources.mNumArraySlices;
                viewDesc.Texture3D.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eInvalid:
            default:
                return;
        }

        d3d12Device->CreateRenderTargetView( mImageAllocation.mResource.Get(), &viewDesc, { descriptor } );
    }

    auto Texture::CreateDsv( core::usize descriptor, renderer::rhi::TextureSubresourceSet subResources, bool isReadOnly ) const -> void {
        MKT_BEGIN_PROFILER();
        subResources = NormalizeSubResources( subResources );
        Device *device{ core::checked_cast<Device *>( mDevice ) };
        ID3D12Device2 *d3d12Device{ device->GetDevice() };

        D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};

        viewDesc.Format = d3d12::GetFormat( mFormat );

        if ( isReadOnly ) {
            viewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;

            if ( viewDesc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || viewDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ) {
                viewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
            }
        }

        auto dimension{ mDimension };

        if ( mMultisampling != renderer::rhi::Multisampling::eMsaaX1 ) {
            dimension = dimension == renderer::rhi::TextureDimension::eTexture2DArray || dimension == renderer::rhi::TextureDimension::eTexture2DMSArray ? renderer::rhi::TextureDimension::eTexture2DMSArray : renderer::rhi::TextureDimension::eTexture2DMS;
        }

        switch ( dimension ) {
            case renderer::rhi::TextureDimension::eTexture1D:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture1DArray:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                viewDesc.Texture1DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture1DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture1DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2D:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2DArray:
            case renderer::rhi::TextureDimension::eTextureCube:
            case renderer::rhi::TextureDimension::eTextureCubeArray:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.ArraySize = subResources.mNumArraySlices;
                viewDesc.Texture2DArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DArray.MipSlice = subResources.mBaseMipLevel;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMS:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMSArray:
                viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                viewDesc.Texture2DMSArray.FirstArraySlice = subResources.mBaseArraySlice;
                viewDesc.Texture2DMSArray.ArraySize = subResources.mNumArraySlices;
                break;
            case renderer::rhi::TextureDimension::eTexture3D: {
                MKT_CORE_LOGGER_ERROR( "Unsupported dimension for DSV" );

                return;
            }

            case renderer::rhi::TextureDimension::eInvalid:
            default:
                return;
        }

        d3d12Device->CreateDepthStencilView( mImageAllocation.mResource.Get(), &viewDesc, { descriptor } );
    }

    auto Texture::GetRtvDescriptorIndex() const -> DescriptorIndex {
        MKT_BEGIN_PROFILER();
        return mRtvDescriptorIndex;
    }

    auto Texture::GetDsvDescriptorIndex() const -> DescriptorIndex {
        MKT_BEGIN_PROFILER();
        return mDsvDescriptorIndex;
    }

    auto Texture::GetRtvDescriptorIndex( renderer::rhi::TextureSubresourceSet subresources ) -> DescriptorIndex {
        subresources = NormalizeSubResources( subresources );
        subresources.mNumMipLevels = 1;
        std::scoped_lock lock{ mViewMutex };

        for ( const auto &view: mRenderTargetViews ) {
            if ( view.mSubResources.mBaseMipLevel == subresources.mBaseMipLevel &&
                 view.mSubResources.mBaseArraySlice == subresources.mBaseArraySlice &&
                 view.mSubResources.mNumArraySlices == subresources.mNumArraySlices && view.mReadOnly == false ) {
                return view.mIndex;
            }
        }

        const auto index{ mResources->mRenderTargetViewHeap->AllocateDescriptor() };
        const auto cpu{ mResources->mRenderTargetViewHeap->GetCpuHandle( index ) };
        CreateRtv( cpu.ptr, subresources );
        mRenderTargetViews.push_back( { subresources, index, false } );
        return index;
    }

    auto Texture::GetDsvDescriptorIndex( renderer::rhi::TextureSubresourceSet subresources, bool readOnly ) -> DescriptorIndex {
        subresources = NormalizeSubResources( subresources );
        subresources.mNumMipLevels = 1;
        std::scoped_lock lock{ mViewMutex };

        for ( const auto &view: mDepthStencilViews ) {
            if ( view.mSubResources.mBaseMipLevel == subresources.mBaseMipLevel &&
                 view.mSubResources.mBaseArraySlice == subresources.mBaseArraySlice &&
                 view.mSubResources.mNumArraySlices == subresources.mNumArraySlices && view.mReadOnly == readOnly ) {
                return view.mIndex;
            }
        }

        const auto index{ mResources->mDepthStencilViewHeap->AllocateDescriptor() };
        const auto cpu{ mResources->mDepthStencilViewHeap->GetCpuHandle( index ) };
        CreateDsv( cpu.ptr, subresources, readOnly );
        mDepthStencilViews.push_back( { subresources, index, readOnly } );
        return index;
    }

    Texture::operator ID3D12Resource *() const {
        MKT_BEGIN_PROFILER();
        return mImageAllocation.mResource.Get();
    }

    Texture::~Texture() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Texture::GetResourceIndex() const -> DescriptorIndex {
        MKT_BEGIN_PROFILER();
        return mShaderResourceIndex;
    }

    auto Texture::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };

        // https://asawicki.info/news_1726_secrets_of_direct3d_12_resource_alignment
        // https://logins.github.io/graphics/2020/07/31/DX12ResourceHandling.html
        // We can also use D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT constant
        // instead of specifying buffer and base textures alignment size for the
        // functions that require it and D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
        // for MSAA textures, but we can also set the field to 0, and that will
        // be automatically resolved by the SDK depending on the resource.
        // Still those values represent the minimum size for a resource heap memory allocation.


        // Placed/nonzeroed RT and DS subresources need a full clear, copy or discard
        // before partial operations. The optimized clear value does not initialize them.

        if ( !mIsExternalImage ) {
            mImageAllocation.mDesc.Dimension = d3d12::GetDimension( mDimension );
            mImageAllocation.mDesc.Alignment = 0;
            mImageAllocation.mDesc.Width = mWidth;
            mImageAllocation.mDesc.Height = mHeight;
            mImageAllocation.mDesc.DepthOrArraySize = static_cast<UINT16>(
                mDimension == rhi::TextureDimension::eTexture3D ? mDepth : mDimension == rhi::TextureDimension::eTextureCube ? 6u
                                                                                                                             : glm::max( 1u, mSubResources.mNumArraySlices ) );
            mImageAllocation.mDesc.MipLevels = mMipCount;
            mImageAllocation.mDesc.Format = d3d12::GetFormat( mFormat );
            // Depth attachments are also sampled by SSAO, shadows and post effects.
            switch ( mImageAllocation.mDesc.Format ) {
                case DXGI_FORMAT_D32_FLOAT:
                    mImageAllocation.mDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                    break;
                case DXGI_FORMAT_D16_UNORM:
                    mImageAllocation.mDesc.Format = DXGI_FORMAT_R16_TYPELESS;
                    break;
                case DXGI_FORMAT_D24_UNORM_S8_UINT:
                    mImageAllocation.mDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
                    break;
                case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
                    mImageAllocation.mDesc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
                    break;
                default:
                    break;
            }

            mImageAllocation.mDesc.SampleDesc.Quality = 0;
            mImageAllocation.mDesc.SampleDesc.Count = d3d12::GetSampleCount( mMultisampling );

            mImageAllocation.mDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            mImageAllocation.mDesc.Flags = d3d12::GetResourceFlags( mTextureUsage );

            mImageAllocation.mAllocDesc.HeapType = d3d12::GetHeapType( mHeapType );
            mImageAllocation.mAllocDesc.ExtraHeapFlags |= D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;

            // Optimized clear value. This warning appears on debug builds, but it has been disabled. See infoQueue setup
            // ID3D12Device::CreatePlacedResource: pOptimizedClearValue must be NULL when D3D12_RESOURCE_DESC::Dimension is
            // not D3D12_RESOURCE_DIMENSION_BUFFER and neither D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET nor
            // D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL are set in D3D12_RESOURCE_DESC::Flags.
            // https://wiki.libsdl.org/SDL3/SDL_BeginGPURenderPass
            // https://github.com/libsdl-org/SDL/issues/10758
            if ( ( mTextureUsage & rhi::TextureUsageFlagsBits::RenderTarget ) ||
                 ( mTextureUsage & rhi::TextureUsageFlagsBits::DepthTarget || mTextureUsage & rhi::TextureUsageFlagsBits::DepthStencilTarget ) ) {
                renderer::rhi::Color clearColor{ rhi::kColorMagenta };
                mOptimizedClearValue.Format = d3d12::GetFormat( mFormat );
                mOptimizedClearValue.Color[0] = clearColor.mR;
                mOptimizedClearValue.Color[1] = clearColor.mG;
                mOptimizedClearValue.Color[2] = clearColor.mB;
                mOptimizedClearValue.Color[3] = clearColor.mA;

                // Color and DepthStencil share a union in D3D12_CLEAR_VALUE.
                if ( mImageAllocation.mDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL ) {
                    mOptimizedClearValue.DepthStencil.Depth = 1.0f;
                    mOptimizedClearValue.DepthStencil.Stencil = 0;
                }

                mImageAllocation.mOptimizedClearValue = MKT_ADDRESSOF( mOptimizedClearValue );
            }

            // Allocator creates them in common state
            mResourceState = renderer::rhi::ResourceStates::eCommon;

            if ( mInitialState != renderer::rhi::ResourceStates::eCommon ) {
                mImageAllocation.mInitialState = d3d12::GetResourceState( mInitialState );
                mResourceState = mInitialState;
            }

            auto *allocator{ device->GetAllocator() };
            ThrowIfFailed( allocator->AllocateImage( mImageAllocation ) );
        }

        if ( mTextureUsage & renderer::rhi::TextureUsageFlagsBits::RenderTarget ) {
            mRtvDescriptorIndex = GetRtvDescriptorIndex( {} );
        }

        if ( mTextureUsage & renderer::rhi::TextureUsageFlagsBits::DepthTarget || mTextureUsage & renderer::rhi::TextureUsageFlagsBits::DepthStencilTarget ) {
            mDsvDescriptorIndex = GetDsvDescriptorIndex( {} );
        }

        // Fill initial contents
        if ( !mImageData.IsEmpty() ) {
            if ( mDimension == renderer::rhi::TextureDimension::eTexture2D ) {
                InitInitialData2D( mImageData->mBufferSpan );
            }
            else if ( mDimension == renderer::rhi::TextureDimension::eTextureCube ) {
                InitInitialDataCube( mImageData->mBufferSpan );
            }
        }

        if ( !mBufferSpan.IsEmpty() ) {
            if ( mDimension == renderer::rhi::TextureDimension::eTexture2D ) {
                InitInitialData2D( mBufferSpan );
            }
            else if ( mDimension == renderer::rhi::TextureDimension::eTextureCube ) {
                InitInitialDataCube( mBufferSpan );
            }
        }

        if ( !mKeepInitializerResources ) {
            mImageData.Reset();
            mBufferSpan.Reset();
        }

        mIsAllocated = true;
    }

    auto Texture::Destroy() -> void {
        MKT_BEGIN_PROFILER();

        for ( const auto &view: mDepthStencilViews ) {
            mResources->mDepthStencilViewHeap->ReleaseDescriptor( view.mIndex );
        }

        for ( const auto &view: mRenderTargetViews ) {
            mResources->mRenderTargetViewHeap->ReleaseDescriptor( view.mIndex );
        }

        mDepthStencilViews.clear();
        mRenderTargetViews.clear();

        mIsAllocated = false;
    }

    auto Texture::InitInitialData2D( memory::BufferSpanHandle buffer ) -> void {
        MKT_BEGIN_PROFILER();
        core::u64 fenceValue{ 0 };
        renderer::rhi::FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
        renderer::rhi::CommandListHandle cmd{ mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics ) };
        cmd->Open( { .mScopeName = string::Format( "Texture upload: {}", mDebugName ) } );

        // Upload the supplied base level or complete mip chain.
        cmd->Write( this, buffer->GetData(), buffer->GetSize() );

        // These textures are often loaded to be read from shaders
        cmd->SetTransition( renderer::rhi::TransitionDescription{}
                .AddTexture( this, renderer::rhi::ResourceStates::eShaderResource ) );

        cmd->Close();

        // Signal fenceValue on one fence on completion of these
        // commands, then we wait for that completion this blocks the caller
        // but client should ideally offload this task to worker threads
        const auto submitInfo{ renderer::rhi::SubmitInfo{}
                .AddSignal( fence, fenceValue )
                .AddCommandList( cmd ) };
        mDevice->GetQueue( renderer::rhi::QueueType::eGraphics )->ExecuteCommandLists( submitInfo );
        ( void )fence->Wait( fenceValue, eastl::numeric_limits<core::u64>::max() );// Host wait
    }

    auto Texture::InitInitialDataCube( memory::BufferSpanHandle buffer ) -> void {
        InitInitialData2D( buffer );
    }

    auto Sampler::GetMinFilter() const -> rhi::SamplerFilter {
        return mMinFilter;
    }

    auto Sampler::GetMagFilter() const -> rhi::SamplerFilter {
        return mMagFilter;
    }

    auto Sampler::GetWrapU() const -> rhi::SamplerWrapMode {
        return mWrapU;
    }

    auto Sampler::GetWrapV() const -> rhi::SamplerWrapMode {
        return mWrapV;
    }

    auto Sampler::GetWrapW() const -> rhi::SamplerWrapMode {
        return mWrapW;
    }

    auto Sampler::GetMipLevels() const -> float {
        return mMipLevels;
    }

    auto Texture::GetStorageSizeBytes() const -> core::usize {
        const auto &format{ rhi::GetFormatInfo( mFormat ) };
        const core::usize block{ eastl::max<core::usize>( 1, format.mBlockSize ) };
        core::usize width{ mWidth };
        core::usize height{ mHeight };
        core::usize depth{ mDepth };
        core::usize result{};

        for ( core::u32 mip{}; mip < mMipCount; ++mip ) {
            result += ( ( width + block - 1 ) / block ) * ( ( height + block - 1 ) / block ) * depth * format.mBytesPerBlock;
            width = eastl::max<core::usize>( 1, width / 2 );
            height = eastl::max<core::usize>( 1, height / 2 );
            depth = eastl::max<core::usize>( 1, depth / 2 );
        }

        return result * ( mDimension == rhi::TextureDimension::eTextureCube ? 6 : 1 );
    }

    auto Texture::GetFormat() const -> rhi::Format {
        return mFormat;
    }

    auto Texture::GetWidth() const -> core::u32 {
        return mWidth;
    }

    auto Texture::GetHeight() const -> core::u32 {
        return mHeight;
    }

    auto Texture::GetDepth() const -> core::u32 {
        return mDepth;
    }

    auto Texture::GetTextureUsage() const -> rhi::TextureUsageFlags {
        return mTextureUsage;
    }

    auto Texture::GetImage() const -> asset::ImageHandle {
        return mImageData;
    }

    auto Texture::GetSizeBytes() const -> core::usize {
        if ( mImageData && mImageData->mBufferSpan ) {
            return mImageData->mBufferSpan->GetSize();
        }

        return mBufferSpan ? mBufferSpan->GetSize() : 0;
    }

    auto Texture::GetDimension() const -> rhi::TextureDimension {
        return mDimension;
    }

    auto Texture::GetSampleCount() const -> rhi::Multisampling {
        return mMultisampling;
    }

    auto Texture::GetMipLevelCount() const -> core::u32 {
        return mMipCount;
    }
}// namespace mikoto::renderer::d3d12
#endif
