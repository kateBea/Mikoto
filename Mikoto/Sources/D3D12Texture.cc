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

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <D3D12MemAlloc.h>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/D3D12Texture.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    Sampler::Sampler( const rhi::SamplerCreateDescription& desc )
        : ISampler{ desc }
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
        // TODO: revise Microsoft DirectX 12 examples
        Device* device{ checked_cast<Device*>( mDevice ) };

        // TODO: get descriptor handle

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

        device->GetDevice()->CreateSampler(&wrapSamplerDesc, mSamplerHandle);
        mIsAllocated = true;
    }

    Texture::Texture( const TextureCreateDescription& desc)
        : ITexture{ desc }
    {

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

    Texture::~Texture() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Texture::Initialize() -> void {
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

        auto* allocator{ checked_cast<Device*>( mDevice )->GetAllocator() };
        ThrowIfFailed( allocator->AllocateImage( mImageAllocation ) );

        // Fill initial contents
        if ( !mImageData.IsEmpty() ) {
            if (mDimension == TextureDimension::eTexture2D) {
                InitInitialData2D();
            } else if (mDimension == TextureDimension::eTextureCube) {
                InitInitialDataCube();
            }

            if (!mKeepInitializerResources) {
                mImageData.Reset();
            }
        }

        mIsAllocated = true;
    }

    auto Texture::Release() -> void {
        mIsAllocated = false;
    }

    auto Texture::InitInitialData2D() -> void {
        CommandListHandle cmd{ mDevice->CreateCommandList( QueueType::eTransfer ) };
        cmd->Begin( {} );

        // Data is always writen at mip zero
        cmd->Write( this, 0, mImageData->mBufferSpan->GetData(), mImageData->mBufferSpan->GetSize() );

        // These textures are often loaded to be read from shaders
        cmd->SetResourceState( this, ResourceStates::eShaderResource );

        cmd->End();
        mDevice->ExecuteCommands( cmd );
    }

    auto Texture::InitInitialDataCube() -> void {

    }
}// namespace mikoto::renderer::d3d12

#endif
