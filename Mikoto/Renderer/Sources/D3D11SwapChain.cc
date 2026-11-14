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

#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Platform/Window.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11SwapChain.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>

namespace mikoto::renderer::d3d11 {

    using namespace mikoto::core;
    using namespace mikoto::platform;

    SwapChain::SwapChain( platform::IWindow* window, Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory )
        : mWindow{ window },
          mWidth{ core::as<core::u32>( window->GetWidth() ) },
          mHeight{ core::as<core::u32>( window->GetHeight() ) },
          mDxgiFactory{ dxgiFactory } {
    }

    auto SwapChain::Present() -> void {
        MKT_BEGIN_PROFILER();
        // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present
        // 0 - The presentation occurs immediately, there is no synchronization.
        // 1 through 4 - Synchronize presentation after the nth vertical blank.

        switch ( mRefreshRate ) {

            case renderer::rhi::RefreshRate::eSync:
                mSwapChain->Present( 1, 0 );
                break;
            case renderer::rhi::RefreshRate::eUnlimited:
                mSwapChain->Present( 0, 0 );
                break;
        }
    }

    auto SwapChain::OnResize( core::u32 width, core::u32 height ) -> void {
        MKT_BEGIN_PROFILER();
        mWidth = width;
        mHeight = height;

        Device* device{ core::checked_cast<Device*>( mDevice ) };
        ID3D11DeviceContext* context{ device->GetDeviceContext() };

        context->Flush();

        // Unbind render targets first
        ID3D11RenderTargetView* nullRTV[]{ nullptr };
        context->OMSetRenderTargets( 1, nullRTV, nullptr );

        // Release all references to swapchain buffers
        mRenderTarget.Reset();
        mBackBuffer.Reset();

        if ( FAILED( mSwapChain->ResizeBuffers(
                 0,
                 mWidth,
                 mHeight,
                 d3d11::GetFormat( mFormat ),
                 MKT_D3D11_NO_FLAGS ) ) ) {
            MKT_CORE_LOGGER_CRITICAL( "D3D11SwapChain Failed to resize swapchain" );
            return;
        }

        CreateSwapchainResources();
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

    auto SwapChain::GetFormat() const -> renderer::rhi::Format {
        MKT_BEGIN_PROFILER();
        return mFormat;
    }

    auto SwapChain::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D11_Texture2D:
                return renderer::rhi::Object( mBackBuffer.Get() );

            case renderer::rhi::ObjectType::D3D11_RTV:
                return renderer::rhi::Object( mRenderTarget.Get() );

            case renderer::rhi::ObjectType::D3D11_SRV:
                return renderer::rhi::Object( nullptr );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto SwapChain::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D11_Texture2D:
                return renderer::rhi::Object( mBackBuffer.Get() );

            case renderer::rhi::ObjectType::D3D11_RTV:
                return renderer::rhi::Object( mRenderTarget.Get() );

            case renderer::rhi::ObjectType::D3D11_SRV:
                return renderer::rhi::Object( nullptr );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto SwapChain::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        DXGI_SWAP_CHAIN_DESC1 swapChainDescriptor{};
        swapChainDescriptor.Width = mWidth;
        swapChainDescriptor.Height = mHeight;
        swapChainDescriptor.Format = d3d11::GetFormat( mFormat );
        swapChainDescriptor.SampleDesc.Count = 1;
        swapChainDescriptor.SampleDesc.Quality = 0;
        swapChainDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDescriptor.BufferCount = 2;
        swapChainDescriptor.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDescriptor.Scaling = DXGI_SCALING::DXGI_SCALING_STRETCH;
        swapChainDescriptor.Flags = MKT_D3D11_NO_FLAGS;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullscreenDescriptor{};
        swapChainFullscreenDescriptor.Windowed = true;

        HWND win32Handle{};

        try {
            win32Handle = static_cast<HWND>( mWindow->GetSystemHandle() );
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::CreateSwapChain - Cast exception: e.what(): {}", exception.what() );
        }

        if ( FAILED( mDxgiFactory->CreateSwapChainForHwnd(
                 core::checked_cast<Device*>( mDevice )->GetDevice(),
                 win32Handle,
                 &swapChainDescriptor,
                 &swapChainFullscreenDescriptor,
                 nullptr,
                 &mSwapChain ) ) ) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::CreateSwapChain - Failed to create swapchain" );
        }

        CreateSwapchainResources();

        mIsAllocated = true;
    }

    auto SwapChain::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    auto SwapChain::CreateSwapchainResources() -> void {
        MKT_BEGIN_PROFILER();

        if ( FAILED( mSwapChain->GetBuffer(
                 MKT_D3D11_NO_FLAGS,
                 IID_PPV_ARGS( &mBackBuffer ) ) ) ) {
            MKT_CORE_LOGGER_ERROR( "D3D11: Failed to get Back Buffer from the SwapChain" );
        }

        if ( FAILED( core::as<Device*>( mDevice )->GetDevice()->CreateRenderTargetView(
                 mBackBuffer.Get(),
                 nullptr,
                 &mRenderTarget ) ) ) {
            MKT_CORE_LOGGER_ERROR( "D3D11: Failed to create RTV from Back Buffer" );
        }
    }

    SwapChain::~SwapChain() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }
}// namespace mikoto

#endif
