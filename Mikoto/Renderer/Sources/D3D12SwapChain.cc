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

#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>

#include <Platform/Window.hh>

#include <Renderer/Core/RenderSystem.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_5.h>

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Texture.hh>
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#include <Renderer/Rhi/D3D12/D3D12SwapChain.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::platform;
    using namespace mikoto::renderer::rhi;

    SwapChain::SwapChain( platform::IWindow* window, Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory )
        : mWindow{ window }, mDxgiFactory{ mDxgiFactory } {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mWindow, "Window handle cannot be NULL." );

        mWidth = mWindow->GetWidth();
        mHeight = mWindow->GetHeight();
    }

    auto SwapChain::Present() -> void {
        MKT_BEGIN_PROFILER();
        // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present
        // 0 - The presentation occurs immediately, there is no synchronization.
        // 1 through 4 - Synchronize presentation after the nth vertical blank.

        switch ( mRefreshRate ) {
            case renderer::rhi::RefreshRate::eSync:
                ThrowIfFailed( mSwapChain->Present( 1, 0 ) );
                break;
            case renderer::rhi::RefreshRate::eUnlimited:
                ThrowIfFailed( mSwapChain->Present( 0, 0 ) );
                break;
        }
    }

    auto SwapChain::OnResize( core::u32 width, core::u32 height ) -> void {
        MKT_BEGIN_PROFILER();

        if ( width == 0 || height == 0 || ( width == mWidth && height == mHeight ) ) {
            return;
        }

        mDevice->WaitIdle();
        auto* device{ core::checked_cast<Device*>( mDevice ) };
        auto* ctx{ core::checked_cast<Context*>( RenderSystem::Get()->GetContext() ) };
        DXGI_SWAP_CHAIN_DESC1 description{};
        ThrowIfFailed( mSwapChain->GetDesc1( &description ) );
        // Release every app-owned back-buffer reference before ResizeBuffers.
        mBackBufferImages.clear();
        ThrowIfFailed( mSwapChain->ResizeBuffers( ctx->GetBackBufferCount(), width, height,
            d3d12::GetFormat( mFormat ), description.Flags ) );
        mWidth = width;
        mHeight = height;
        mViewportDescription.Width = static_cast<float>( width );
        mViewportDescription.Height = static_cast<float>( height );

        const core::usize backBufferCount{ ctx->GetBackBufferCount() };

        for ( UINT index{}; index < backBufferCount; index++ ) {
            Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetResource{};
            ThrowIfFailed( mSwapChain->GetBuffer( index, IID_PPV_ARGS( &mRenderTargetResource ) ) );

            ExternalTextureDescription externalTextureDesc{
                .mWidth = mWidth,
                .mHeight = mHeight,
                .mFormat = mFormat,
                .mTextureUsage = renderer::rhi::TextureUsageFlagsBits::RenderTarget | 
                    renderer::rhi::TextureUsageFlagsBits::CopyDest,
                .mImageResource = mRenderTargetResource
            };

            renderer::rhi::TextureHandle presentImage{ device->CreateTexture( externalTextureDesc ) };

            if ( !presentImage.IsEmpty() ) {
                presentImage->SetDebugName( string::Format( "Swapchain Img. Index {}", index ) );
                mBackBufferImages.emplace_back( presentImage );
            }
        }
    }

    auto SwapChain::SetRefreshRate( renderer::rhi::RefreshRate type ) -> void {
        MKT_BEGIN_PROFILER();
        mRefreshRate = type;
    }

    auto SwapChain::GetWidth() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mWidth;
    }

    auto SwapChain::GetHeight() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mHeight;
    }

    auto SwapChain::GetCurrentBackBufferImage() const -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return mBackBufferImages[mSwapChain->GetCurrentBackBufferIndex()];
    }

    auto SwapChain::GetFormat() const -> renderer::rhi::Format {
        MKT_BEGIN_PROFILER();
        return mFormat;
    }

    auto SwapChain::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto SwapChain::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    SwapChain::~SwapChain() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto SwapChain::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        Context* ctx{ core::checked_cast<Context*>( RenderSystem::Get()->GetContext() ) };

        // Surface dimensions
        mSurfaceSize.left = 0;
        mSurfaceSize.top = 0;
        mSurfaceSize.right = core::as<LONG>( mWidth );
        mSurfaceSize.bottom = core::as<LONG>( mHeight );

        // Viewport description
        mViewportDescription.TopLeftX = 0.0f;
        mViewportDescription.TopLeftY = 0.0f;
        mViewportDescription.Width = core::as<core::f32>( mWidth );
        mViewportDescription.Height = core::as<core::f32>( mHeight );
        mViewportDescription.MinDepth = 0.0f;
        mViewportDescription.MaxDepth = 1.0f;

        // Swapchain description
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        swapChainDesc.Width = mWidth;
        swapChainDesc.Height = mHeight;
        swapChainDesc.Format = d3d12::GetFormat( mFormat );
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc = { 1, 0 };
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = ctx->GetBackBufferCount();
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        // Window handle
        HWND win32Handle{};

        try {
            win32Handle = static_cast<HWND>( mWindow->GetSystemHandle() );
        } catch ( const std::exception& exception ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "d3d12::SwapChain::Initialize - any_cast exception: e.what(): {}", exception.what() ) );
        }

        // Command queue
        Queue* queue{ core::checked_cast<Queue*>( device->GetQueue( renderer::rhi::QueueType::ePresent ) ) };
        ID3D12CommandQueue* cmdQueue{ *queue };

        BOOL allowTearing{ FALSE };
        Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;

        if ( SUCCEEDED( mDxgiFactory.As( &factory5 ) ) ) {
            HRESULT hr{ factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allowTearing,
                sizeof( allowTearing ) ) };

            if ( FAILED( hr ) ) {
                allowTearing = FALSE;
            }
        }

        if ( allowTearing ) {
            swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1{};
        ThrowIfFailed( ctx->GetDxGIFactory4()->CreateSwapChainForHwnd(
            cmdQueue,
            win32Handle,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1 ) );

        // Disable the Alt+Enter fullscreen toggle feature.
        // Switching to fullscreen will be handled manually.
        ThrowIfFailed( ctx->GetDxGIFactory4()->MakeWindowAssociation( win32Handle, DXGI_MWA_NO_ALT_ENTER ) );
        ThrowIfFailed( swapChain1.As( &mSwapChain ) );

        const core::usize backBufferCount{ ctx->GetBackBufferCount() };

        for ( UINT index{}; index < backBufferCount; index++ ) {
            Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetResource{};
            ThrowIfFailed( mSwapChain->GetBuffer( index, IID_PPV_ARGS( &mRenderTargetResource ) ) );

            ExternalTextureDescription externalTextureDesc{
                .mWidth = mWidth,
                .mHeight = mHeight,
                .mFormat = mFormat,
                .mTextureUsage = renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::CopyDest,
                .mImageResource = mRenderTargetResource
            };

            renderer::rhi::TextureHandle presentImage{ device->CreateTexture( externalTextureDesc ) };

            if ( !presentImage.IsEmpty() ) {
                presentImage->SetDebugName( string::Format( "Swapchain Img. Index {}", index ) );
                mBackBufferImages.emplace_back( presentImage );
            }
        }

        mIsAllocated = true;
    }

    auto SwapChain::Destroy() -> void {
        MKT_BEGIN_PROFILER();

        for ( auto& item: mBackBufferImages ) {
            item.Reset();
        }

        mIsAllocated = false;
    }
}// namespace mikoto::renderer::d3d12

#endif
