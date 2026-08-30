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

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Texture.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

namespace mikoto::renderer::d3d11 {

    using namespace mikoto::core;

    Sampler::Sampler( const rhi::SamplerCreateDescription& desc )
        : ISampler{ desc }
    {}

    auto Sampler::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        if ( type == rhi::ObjectType::D3D11_Sampler ) {
            return rhi::Object( mSampler.Get() );
        }
        return rhi::Object( nullptr );
    }

    auto Sampler::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        if ( type == rhi::ObjectType::D3D11_Sampler ) {
            return rhi::Object( mSampler.Get() );
        }
        return rhi::Object( nullptr );
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
        D3D11_SAMPLER_DESC samplerDesc{};

        samplerDesc.Filter = GetFilter(mMinFilter, mMagFilter);

        samplerDesc.AddressU = GetAddressMode(mWrapU);
        samplerDesc.AddressV = GetAddressMode(mWrapV);
        samplerDesc.AddressW = GetAddressMode(mWrapW);

        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;

        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = mMipLevels;

        samplerDesc.BorderColor[0] = mBorderColor.mR;
        samplerDesc.BorderColor[1] = mBorderColor.mG;
        samplerDesc.BorderColor[2] = mBorderColor.mB;
        samplerDesc.BorderColor[3] = mBorderColor.mA;

        if (FAILED(
                checked_cast<Device*>(mDevice)->GetDevice()->CreateSamplerState(
                    &samplerDesc,
                    &mSampler
                ))) {
            MKT_ASSERT( false, "Failed to create sampler state" );
        }

        mIsAllocated = true;
    }

    auto Sampler::SetDebugName( const eastl::string_view name ) -> void {
        mSampler->SetPrivateData(WKPDID_D3DDebugObjectName,as<UINT>(name.size()), name.data() );
    }

    Texture::Texture( const TextureCreateDescription &data )
        : ITexture{ data } , mKeepInitializerResources{ data.mKeepInitializerResources } {}

    auto Texture::SetDebugName( const eastl::string_view name ) -> void {
        mTexture->SetPrivateData(WKPDID_D3DDebugObjectName,as<UINT>(name.size()), name.data() );
    }

    auto Texture::GetNativeHandle( ObjectType type ) -> Object {
        switch ( type ) {
            case ObjectType::D3D11_Texture2D:
                return Object( mTexture.Get() );

            case ObjectType::D3D11_RTV:
                return Object( mRenderTargetView.Get() );

            case ObjectType::D3D11_SRV:
                return Object( mShaderResourceView.Get() );

            case ObjectType::D3D11_DSV:
                return Object( mDepthStencilTargetView.Get() );

            default:;
        }

        return Object( nullptr );
    }

    auto Texture::GetNativeHandle( ObjectType type ) const -> Object {
        switch ( type ) {
            case ObjectType::D3D11_Texture2D:
                return Object( mTexture.Get() );

            case ObjectType::D3D11_RTV:
                return Object( mRenderTargetView.Get() );

            case ObjectType::D3D11_SRV:
                return Object( mShaderResourceView.Get() );

            case ObjectType::D3D11_DSV:
                return Object( mDepthStencilTargetView.Get() );

            default:;
        }

        return Object( nullptr );
    }

    auto Texture::EnableUsage( ResourceStates state ) -> void {
        // When we create the texture we only create the types of views
        // according to specified usage, when we transition usage we may create
        // more type of views if not present, say a texture only supported RTV,
        // but now we need it to support shader read
        switch (state) {
            case ResourceStates::eShaderResource:
                if (mShaderResourceView == nullptr) {
                    // Create the shader target view.
                    HRESULT result{ as<Device*>(mDevice)->GetDevice()->CreateShaderResourceView(mTexture.Get(),
                        nullptr, mShaderResourceView.GetAddressOf() ) };
                    if(FAILED(result)) {
                        MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateShaderResourceView failed" );
                    }
                }
                break;
            default:
                break;
        }
    }

    Texture::~Texture() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Texture::Initialize() -> void {
        // Assumes everything went right
        // unless otherwise specified
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

        if (textureDesc.Usage == D3D11_USAGE_DYNAMIC || textureDesc.Usage == D3D11_USAGE_STAGING) {
            textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        }

        // Populate initial data. Of the sources needs to be empty
        MKT_ASSERT( mImageData.IsEmpty() || mBufferSpan.IsEmpty(), "Cannot provide buffer data and image at the same time" );

        D3D11_SUBRESOURCE_DATA initialData{};

        if ( !mImageData.IsEmpty() ) {
            const UINT bytesPerPixel{ GetBytesPerPixel(mFormat) };

            initialData.pSysMem = mImageData->mBufferSpan->GetData();
            initialData.SysMemPitch = bytesPerPixel * mWidth;
        }

        if ( !mBufferSpan.IsEmpty() ) {
            const UINT bytesPerPixel{ GetBytesPerPixel(mFormat) };

            initialData.pSysMem = mBufferSpan->GetData();
            initialData.SysMemPitch = bytesPerPixel * mWidth;
        }

        // Create the render target texture.
        HRESULT result{ checked_cast<Device*>(mDevice)->GetDevice()->CreateTexture2D(&textureDesc,
            mImageData.IsEmpty() ? nullptr : MKT_ADDRESSOF( initialData ), mTexture.GetAddressOf() ) };
        if(FAILED(result)) {
            MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateTexture2D failed" );
            mIsAllocated = false;
        }

        if ( mTextureUsage & TextureUsageFlagsBits::kRenderTarget ) {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format = d3d11::GetFormat( mFormat );
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;

            result = checked_cast<Device*>(mDevice)->GetDevice()->CreateRenderTargetView(
                mTexture.Get(), MKT_ADDRESSOF( rtvDesc ), mRenderTargetView.GetAddressOf() );
            if(FAILED(result)) {
                MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateRenderTargetView failed" );
                mIsAllocated = false;;
            }
        }

        if ( mTextureUsage & TextureUsageFlagsBits::kDepthTarget ||
            mTextureUsage & TextureUsageFlagsBits::kDepthStencilTarget ||
            mTextureUsage & TextureUsageFlagsBits::kStencilTarget ) {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
            dsvDesc.Format = d3d11::GetFormat( mFormat );
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;

            result = checked_cast<Device*>(mDevice)->GetDevice()->CreateDepthStencilView(mTexture.Get(),
                MKT_ADDRESSOF( dsvDesc ), mDepthStencilTargetView.GetAddressOf() );
            if(FAILED(result)) {
                MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateRenderTargetView failed" );
                mIsAllocated = false;;
            }
        }

        if (mTextureUsage.Has(TextureUsageFlagsBits::kShaderResource )) {
            // Create the shader target view.
            result = checked_cast<Device*>(mDevice)->GetDevice()->CreateShaderResourceView(mTexture.Get(),
                nullptr, mShaderResourceView.GetAddressOf() );
            if(FAILED(result)) {
                MKT_CORE_LOGGER_ERROR( "D3D11Texture::Initialize CreateShaderResourceView failed" );
                mIsAllocated = false;
            }
        }

        if (!mKeepInitializerResources) {
            mImageData.Release();
            mBufferSpan.Release();
        }
    }

    auto Texture::Release() -> void {
        mIsAllocated = false;
    }
}// namespace mikoto::renderer::d3d11

#endif