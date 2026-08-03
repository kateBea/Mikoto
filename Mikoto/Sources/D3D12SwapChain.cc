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

#include <Renderer/D3D12/D3D12SwapChain.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/D3D12/D3D12Context.hh>
#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>

namespace mikoto::renderer::d3d12 {

    SwapChain::SwapChain( Window *window, Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory )
        : mWindow{ window }, mDxgiFactory{ mDxgiFactory } {
        MKT_ASSERT( mWindow, "Window handle cannot be NULL." );

        mWidth = mWindow->GetWidth();
        mHeight = mWindow->GetHeight();
    }

    auto SwapChain::Present() -> void {
        // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present
        // 0 - The presentation occurs immediately, there is no synchronization.
        // 1 through 4 - Synchronize presentation after the nth vertical blank.

        switch (mRefreshRate) {
            case RefreshRate::eSync:
                ThrowIfFailed( mSwapChain->Present(1, 0) );
                break;
            case RefreshRate::eUnlimited:
                ThrowIfFailed( mSwapChain->Present(0, 0) );
                break;
        }
    }

    auto SwapChain::OnResize( u32 width, u32 height ) -> void {
        mWidth = width;
        mHeight = height;

        Context* ctx{ checked_cast<Context*>( RenderSystem::Get()->GetContext() ) };
        mSwapChain->ResizeBuffers(ctx->GetBackBufferCount(), mWidth, mHeight,
                                 DXGI_FORMAT_R8G8B8A8_UNORM, 0);
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

    auto SwapChain::GetCurrentBackBufferImage() const -> TextureHandle {
        return mBackBufferImages[mSwapChain->GetCurrentBackBufferIndex()];
    }

    auto SwapChain::GetFormat() const -> Format {
        return mFormat;
    }

    auto SwapChain::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            default:;
        }

        return Object(nullptr);
    }

    auto SwapChain::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            default:;
        }

        return Object(nullptr);
    }

    SwapChain::~SwapChain() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto SwapChain::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        Context* ctx{ checked_cast<Context*>( RenderSystem::Get()->GetContext() ) };

        mRenderTargetsViews.resize( (size_t)ctx->GetBackBufferCount() );

        // Surface dimensions
        mSurfaceSize.left = 0;
        mSurfaceSize.top = 0;
        mSurfaceSize.right = as<LONG>(mWidth);
        mSurfaceSize.bottom = as<LONG>(mHeight);

        // Viewport description
        mViewportDescription.TopLeftX = 0.0f;
        mViewportDescription.TopLeftY = 0.0f;
        mViewportDescription.Width = as<f32>(mWidth);
        mViewportDescription.Height = as<f32>(mHeight);
        mViewportDescription.MinDepth = .1f;
        mViewportDescription.MaxDepth = 1000.f;

        // Swapchain description
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        swapChainDesc.Width = mWidth;
        swapChainDesc.Height = mHeight;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
            auto window{ eastl::any_cast<GLFWwindow*>( mWindow->GetNativeWindow() ) };
            win32Handle = glfwGetWin32Window(window);
        } catch ( const std::exception& exception ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "d3d12::SwapChain::Initialize - any_cast exception: e.what(): {}", exception.what() ) );
        }

        // Command queue
        Queue* queue{ device->GetQueue( QueueType::ePresent ) };
        ID3D12CommandQueue* cmdQueue{ *queue };

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1{};
        ThrowIfFailed(ctx->GetDxGIFactory()->CreateSwapChainForHwnd(
            cmdQueue,
            win32Handle,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1));

        // Disable the Alt+Enter fullscreen toggle feature.
        // Switching to fullscreen will be handled manually.
        ThrowIfFailed(ctx->GetDxGIFactory()->MakeWindowAssociation(win32Handle, DXGI_MWA_NO_ALT_ENTER));
        ThrowIfFailed(swapChain1.As(&mSwapChain));

        // Describe and create a render target view (RTV) descriptor heap.
        // It allocates memory on the GPU to hold a list of descriptors (views).
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.NumDescriptors = ctx->GetBackBufferCount();
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->GetDevice()->CreateDescriptorHeap(
            &rtvHeapDesc, IID_PPV_ARGS(&mRenderTargetViewHeap)));

        // Here I ask my specific GPU hardware: "How many bytes wide is one single RTV slot?"
        mRtvDescriptorSize = device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Frame resources
        // Handle to beginning of render target descriptors
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{ mRenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart() };

        // TODO: Move these ID3D12Resources to the Texture class
        // Create an RTV for each frame.
        // Pull the raw texture resource handles out of the DXGI swapchain and save them
        // into mRenderTargetsViews array, create the actual view, link the raw texture
        // (mRenderTargetsViews[index]) to the current slot pointer (rtvHandle). nullptr is used to let
        // the method infer the format automatically from the texture description.
        // Finally advance the pointer forward by the byte size of exactly one descriptor slot.
        for (UINT index{}; index < ctx->GetBackBufferCount(); index++) {
            ThrowIfFailed(mSwapChain->GetBuffer(index, IID_PPV_ARGS(&mRenderTargetsViews[index])));
            device->GetDevice()->CreateRenderTargetView(mRenderTargetsViews[index], nullptr, rtvHandle);
            rtvHandle.ptr += 1 * mRtvDescriptorSize;
        }

        mIsAllocated = true;
    }

    auto SwapChain::Release() -> void {
        mIsAllocated = false;
    }
}// namespace mikoto::renderer::d3d12

#endif