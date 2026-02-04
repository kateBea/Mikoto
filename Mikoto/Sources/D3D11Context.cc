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


#include <Core/Exception.hh>

#include <Renderer/D3D11/D3D11Context.hh>


#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl.h>

#include <Renderer/D3D11/D3D11Device.hh>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>

namespace Mikoto {

    auto D3D11Context::Init() -> bool {
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&m_DxgiFactory)))) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Unable to create DXGIFactory" );
            return false;
        }

        // Init the device when the context is ready
        m_Device = GpuDevice::Create({ .Api = GraphicsAPI::DIRECTX_11 });
        if (!m_Device) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Could not initialize DIRECTX_11 GPU Device." );
        }
        m_Device->Init();

        if (!CreateSwapChain()) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Failed to create swapchain" );
            return false;
        }

        return true;
    }

    auto D3D11Context::Shutdown() -> void {

    }

    auto D3D11Context::SubmitFrame() -> void {

    }

    auto D3D11Context::PrepareFrame() -> void {

    }

    auto D3D11Context::Update() -> void {

    }

    auto D3D11Context::Present() -> void {

    }

    auto D3D11Context::SetPresentTarget( TextureHandle texture ) -> void {

    }

    auto D3D11Context::EnableVSync() -> void {

    }

    auto D3D11Context::DisableVSync() -> void {

    }

    auto D3D11Context::IsVsyncEnabled() const -> bool {
        return false;
    }

    auto D3D11Context::CreateSwapChain() -> bool {
        DXGI_SWAP_CHAIN_DESC1 swapChainDescriptor = {};
        swapChainDescriptor.Width = m_TargetWindow->GetWidth();
        swapChainDescriptor.Height = m_TargetWindow->GetHeight();
        swapChainDescriptor.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
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
            const auto window{ std::any_cast<GLFWwindow*>( m_TargetWindow->GetNativeWindow() ) };
            win32Handle = glfwGetWin32Window(window);
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::CreateSwapChain - Cast exception: e.what(): {}", exception.what() );
            return false;
        }

        if (FAILED(m_DxgiFactory->CreateSwapChainForHwnd(
            TO_D3D11_DEVICE(m_Device.get()),
            win32Handle,
            &swapChainDescriptor,
            &swapChainFullscreenDescriptor,
            nullptr,
            &m_SwapChain)))
        {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::CreateSwapChain - Failed to create swapchain" );
            return false;
        }

        return true;
    }
}

#endif
