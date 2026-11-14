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
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Texture.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

namespace mikoto::renderer::d3d11 {

    using namespace mikoto::core;

    Sampler::Sampler( const rhi::SamplerCreateDescription& desc )
        : mMipLevels{ desc.mMipLevels },
          mMinFilter{ desc.mMinFilter },
          mMagFilter{ desc.mMagFilter },
          mWrapU{ desc.mWrapU },
          mWrapV{ desc.mWrapV },
          mWrapW{ desc.mWrapW },
          mBorderColor{ desc.mBorderColor } {
    }

    auto Sampler::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type == rhi::ObjectType::D3D11_Sampler ) {
            return rhi::Object( mSampler.Get() );
        }

        return rhi::Object( nullptr );
    }

    auto Sampler::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type == rhi::ObjectType::D3D11_Sampler ) {
            return rhi::Object( mSampler.Get() );
        }

        return rhi::Object( nullptr );
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
        D3D11_SAMPLER_DESC samplerDesc{};

        samplerDesc.Filter = GetFilter( mMinFilter, mMagFilter );

        samplerDesc.AddressU = GetAddressMode( mWrapU );
        samplerDesc.AddressV = GetAddressMode( mWrapV );
        samplerDesc.AddressW = GetAddressMode( mWrapW );

        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;

        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = mMipLevels;

        samplerDesc.BorderColor[0] = mBorderColor.mR;
        samplerDesc.BorderColor[1] = mBorderColor.mG;
        samplerDesc.BorderColor[2] = mBorderColor.mB;
        samplerDesc.BorderColor[3] = mBorderColor.mA;

        if ( FAILED(
                 core::checked_cast<Device*>( mDevice )->GetDevice()->CreateSamplerState(
                     &samplerDesc,
                     &mSampler ) ) ) {
            MKT_ASSERT( false, "Failed to create sampler state" );
        }

        mIsAllocated = true;
    }

    auto Sampler::SetDebugName( const eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        mSampler->SetPrivateData( WKPDID_D3DDebugObjectName, core::as<UINT>( name.size() ), name.data() );
    }

    Texture::Texture( const renderer::rhi::TextureCreateDescription& data )
        : mWidth{ data.mWidth }, mHeight{ data.mHeight }, mMipCount{ data.mMipCount }, mImageData{ data.mImageHandle }, mBufferSpan{ data.mBufferSpan }, mFormat{ data.mFormat }, mDimension{ data.mDimension }, mTextureUsage{ data.mUsage }, mMultisampling{ data.mMultisampling }, mInitialState{ data.mInitialState }, mSubResources{ data.mSubresourceSet }, mKeepInitializerResources{ data.mKeepInitializerResources } {
        mHeapType = data.mHeapType;
        mResourceType = data.mResourceType;
    }

    auto Texture::SetDebugName( const eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        mTexture->SetPrivateData( WKPDID_D3DDebugObjectName, core::as<UINT>( name.size() ), name.data() );
    }

    auto Texture::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D11_Texture2D:
                return renderer::rhi::Object( mTexture.Get() );

            case renderer::rhi::ObjectType::D3D11_RTV:
                return renderer::rhi::Object( mRenderTargetView.Get() );

            case renderer::rhi::ObjectType::D3D11_SRV:
                return renderer::rhi::Object( mShaderResourceView.Get() );

            case renderer::rhi::ObjectType::D3D11_DSV:
                return renderer::rhi::Object( mDepthStencilTargetView.Get() );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Texture::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D11_Texture2D:
                return renderer::rhi::Object( mTexture.Get() );

            case renderer::rhi::ObjectType::D3D11_RTV:
                return renderer::rhi::Object( mRenderTargetView.Get() );

            case renderer::rhi::ObjectType::D3D11_SRV:
                return renderer::rhi::Object( mShaderResourceView.Get() );

            case renderer::rhi::ObjectType::D3D11_DSV:
                return renderer::rhi::Object( mDepthStencilTargetView.Get() );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Texture::EnableUsage( renderer::rhi::ResourceStates state ) -> void {
        MKT_BEGIN_PROFILER();
        // When we create the texture we only create the types of views
        // according to specified usage, when we transition usage we may create
        // more type of views if not present, say a texture only supported RTV,
        // but now we need it to support shader read
        switch ( state ) {
            case renderer::rhi::ResourceStates::eShaderResource:
                if ( mShaderResourceView == nullptr ) {
                    // Create the shader target view.
                    HRESULT result{ core::as<Device*>( mDevice )->GetDevice()->CreateShaderResourceView( mTexture.Get(),
                        nullptr, mShaderResourceView.GetAddressOf() ) };

                    if ( FAILED( result ) ) {
                        MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateShaderResourceView failed" );
                    }
                }

                break;
            default:
                break;
        }
    }

    Texture::operator ID3D11Resource*() const {
        MKT_BEGIN_PROFILER();
        return mTexture.Get();
    }

    Texture::operator ID3D11RenderTargetView*() const {
        MKT_BEGIN_PROFILER();
        return mRenderTargetView.Get();
    }

    Texture::~Texture() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Texture::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        // Assumes everything went right
        // unless otherwise specified, some checks are done on the fly
        mIsAllocated = true;

        D3D11_TEXTURE2D_DESC textureDesc{};

        // Setup the render target texture description.
        textureDesc.Width = mWidth;
        textureDesc.Height = mHeight;
        textureDesc.MipLevels = mMipCount;
        textureDesc.ArraySize = 1;
        textureDesc.Format = d3d11::GetFormat( mFormat );
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = GetUsageFromHeapType( mHeapType );

        textureDesc.BindFlags = GetBindFlags( mTextureUsage );

        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;

        if ( textureDesc.Usage == D3D11_USAGE_DYNAMIC || textureDesc.Usage == D3D11_USAGE_STAGING ) {
            textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        }

        // Populate initial data. Of the sources needs to be empty
        MKT_ASSERT( mImageData.IsEmpty() || mBufferSpan.IsEmpty(), "Cannot provide buffer data and image at the same time" );

        D3D11_SUBRESOURCE_DATA initialData{};

        if ( !mImageData.IsEmpty() ) {
            const UINT bytesPerPixel{ GetBytesPerPixel( mFormat ) };

            initialData.pSysMem = mImageData->mBufferSpan->GetData();
            initialData.SysMemPitch = bytesPerPixel * mWidth;
        }

        if ( !mBufferSpan.IsEmpty() ) {
            const UINT bytesPerPixel{ GetBytesPerPixel( mFormat ) };

            initialData.pSysMem = mBufferSpan->GetData();
            initialData.SysMemPitch = bytesPerPixel * mWidth;
        }

        // Create the render target texture.
        HRESULT result{ core::checked_cast<Device*>( mDevice )->GetDevice()->CreateTexture2D( &textureDesc,
            mImageData.IsEmpty() ? nullptr : MKT_ADDRESSOF( initialData ), mTexture.GetAddressOf() ) };

        if ( FAILED( result ) ) {
            MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateTexture2D failed" );
            mIsAllocated = false;
        }

        if ( mTextureUsage & renderer::rhi::TextureUsageFlagsBits::RenderTarget ) {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format = d3d11::GetFormat( mFormat );
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;

            result = core::checked_cast<Device*>( mDevice )->GetDevice()->CreateRenderTargetView(
                mTexture.Get(), MKT_ADDRESSOF( rtvDesc ), mRenderTargetView.GetAddressOf() );

            if ( FAILED( result ) ) {
                MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateRenderTargetView failed" );
                mIsAllocated = false;
                ;
            }
        }

        if ( mTextureUsage & renderer::rhi::TextureUsageFlagsBits::DepthTarget ||
             mTextureUsage & renderer::rhi::TextureUsageFlagsBits::DepthStencilTarget ||
             mTextureUsage & renderer::rhi::TextureUsageFlagsBits::StencilTarget ) {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
            dsvDesc.Format = d3d11::GetFormat( mFormat );
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;

            result = core::checked_cast<Device*>( mDevice )->GetDevice()->CreateDepthStencilView( mTexture.Get(),
                MKT_ADDRESSOF( dsvDesc ), mDepthStencilTargetView.GetAddressOf() );

            if ( FAILED( result ) ) {
                MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateRenderTargetView failed" );
                mIsAllocated = false;
                ;
            }
        }

        if ( mTextureUsage.Has( renderer::rhi::TextureUsageFlagsBits::ShaderResource ) ) {
            // Create the shader target view.
            result = core::checked_cast<Device*>( mDevice )->GetDevice()->CreateShaderResourceView( mTexture.Get(),
                nullptr, mShaderResourceView.GetAddressOf() );

            if ( FAILED( result ) ) {
                MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateShaderResourceView failed" );
                mIsAllocated = false;
            }
        }

        if ( !mKeepInitializerResources ) {
            mImageData.Reset();
            mBufferSpan.Reset();
        }
    }

    auto Texture::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
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
        const auto& format{ rhi::GetFormatInfo( mFormat ) };
        const core::usize block{ eastl::max<core::usize>( 1, format.mBlockSize ) };
        core::usize width{ mWidth };
        core::usize height{ mHeight };
        core::usize result{};

        for ( core::u32 mip{}; mip < mMipCount; ++mip ) {
            result += ( ( width + block - 1 ) / block ) * ( ( height + block - 1 ) / block ) * format.mBytesPerBlock;
            width = eastl::max<core::usize>( 1, width / 2 );
            height = eastl::max<core::usize>( 1, height / 2 );
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
}
#endif
