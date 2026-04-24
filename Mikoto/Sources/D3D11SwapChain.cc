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

#include <Renderer/D3D11/D3D11Device.hh>
#include <Renderer/D3D11/D3D11SwapChain.hh>
#include <Renderer/D3D11/Direct3D11Helpers.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>

namespace mikoto::renderer::d3d11 {

    SwapChain::SwapChain( Window* window, Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory )
        : mWindow{ window },
          mWidth{ as<u32>( window->GetWidth() ) },
          mHeight{ as<u32>( window->GetHeight() ) },
          mDxgiFactory{ dxgiFactory } {}

    auto SwapChain::Present() -> void {
        // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present
        // 0 - The presentation occurs immediately, there is no synchronization.
        // 1 through 4 - Synchronize presentation after the nth vertical blank.

        switch (mRefreshRate) {

            case RefreshRate::eSync:
                mSwapChain->Present(1, 0);
                break;
            case RefreshRate::eUnlimited:
                mSwapChain->Present(0, 0);
                break;
        }
    }

    auto SwapChain::OnResize( u32 width, u32 height ) -> void {
        mWidth = width;
        mHeight = height;

        Device* device{ as<Device*>( mDevice ) };
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

    auto SwapChain::SetRefreshRate( RefreshRate type ) -> void {
        mRefreshRate = type;
    }

    auto SwapChain::GetWidth() const -> u32 {
        return mWidth;
    }

    auto SwapChain::GetHeight() const -> u32 {
        return mHeight;
    }

    auto SwapChain::GetFormat() const -> Format {
        return mFormat;
    }

    auto SwapChain::GetNativeHandle( ObjectType type ) -> Object {
        switch ( type ) {
            case ObjectType::D3D11_Texture2D:
                return Object( mBackBuffer.Get() );

            case ObjectType::D3D11_RTV:
                return Object( mRenderTarget.Get() );

            case ObjectType::D3D11_SRV:
                return Object( nullptr );

            default:;
        }

        return Object( nullptr );
    }

    auto SwapChain::GetNativeHandle( ObjectType type ) const -> Object {
        switch ( type ) {
            case ObjectType::D3D11_Texture2D:
                return Object( mBackBuffer.Get() );

            case ObjectType::D3D11_RTV:
                return Object( mRenderTarget.Get() );

            case ObjectType::D3D11_SRV:
                return Object( nullptr );

            default:;
        }

        return Object( nullptr );
    }

    auto SwapChain::Initialize() -> void {
        DXGI_SWAP_CHAIN_DESC1 swapChainDescriptor{};
        swapChainDescriptor.Width = mWidth;
        swapChainDescriptor.Height = mHeight;
        swapChainDescriptor.Format = d3d11::GetFormat(mFormat);
        swapChainDescriptor.SampleDesc.Count = 1;
        swapChainDescriptor.SampleDesc.Quality = 0;
        swapChainDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDescriptor.BufferCount = 2;
        swapChainDescriptor.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDescriptor.Scaling = DXGI_SCALING::DXGI_SCALING_STRETCH;
        swapChainDescriptor.Flags = {};

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullscreenDescriptor{};
        swapChainFullscreenDescriptor.Windowed = true;

        HWND win32Handle{};

        try {
            auto window{ eastl::any_cast<GLFWwindow*>( mWindow->GetNativeWindow() ) };
            win32Handle = glfwGetWin32Window(window);
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::CreateSwapChain - Cast exception: e.what(): {}", exception.what() );
        }

        if (FAILED(mDxgiFactory->CreateSwapChainForHwnd(
            checked_cast<Device*>(mDevice)->GetDevice(),
            win32Handle,
            &swapChainDescriptor,
            &swapChainFullscreenDescriptor,
            nullptr,
            &mSwapChain)))
        {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::CreateSwapChain - Failed to create swapchain" );
        }

        CreateSwapchainResources();

        mIsAllocated = true;
    }

    auto SwapChain::Release() -> void {
        mIsAllocated = false;
    }

    auto SwapChain::CreateSwapchainResources() -> void {
        if (FAILED(mSwapChain->GetBuffer(
            MKT_D3D11_NO_FLAGS,
            IID_PPV_ARGS(&mBackBuffer))))
        {
            MKT_CORE_LOGGER_ERROR( "D3D11: Failed to get Back Buffer from the SwapChain" );
        }

        if (FAILED(as<Device*>(mDevice)->GetDevice()->CreateRenderTargetView(
            mBackBuffer.Get(),
            nullptr,
            &mRenderTarget)))
        {
            MKT_CORE_LOGGER_ERROR( "D3D11: Failed to create RTV from Back Buffer" );
        }
    }

    SwapChain::~SwapChain() {
        if (mIsAllocated) {
            Release();
        }
    }
}// namespace mikoto

#endif