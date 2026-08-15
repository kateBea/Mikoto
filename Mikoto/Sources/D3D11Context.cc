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

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Exception.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl.h>
#include <dxgidebug.h>

#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Context.hh>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>

#include <Platform/PlatformWin32.hh>

namespace mikoto::renderer::d3d11 {

#if !defined(NDEBUG)

    // https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-debug-id
    auto Context::DumpDXGIMessages() -> void {
        const auto messageCount{ mDxgiInfoQueue->GetNumStoredMessages( DXGI_DEBUG_ALL ) };

        for ( UINT64 i{}; i < messageCount; ++i ) {
            SIZE_T messageLength{};
            mDxgiInfoQueue->GetMessage( DXGI_DEBUG_ALL, i, nullptr, &messageLength );

            auto bytes{ eastl::make_unique<std::byte[]>( messageLength ) };
            auto *message{ rc_cast<DXGI_INFO_QUEUE_MESSAGE*>( bytes.get() ) };

            if ( SUCCEEDED( mDxgiInfoQueue->GetMessage( DXGI_DEBUG_ALL, i, message, &messageLength ) ) ) {
                switch (message->Severity ) {
                    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION:
                        MKT_CORE_LOGGER_CRITICAL( "[DXGI Corruption Producer {}] {}", message->pDescription, windows::GuidToString( message->Producer ) );
                        break;
                    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR:
                        MKT_CORE_LOGGER_ERROR( "[DXGI Error Producer {}] {}", message->pDescription, windows::GuidToString( message->Producer ) );
                        break;
                    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING:
                        MKT_CORE_LOGGER_WARN( "[DXGI Warning Producer {}] {}", message->pDescription, windows::GuidToString( message->Producer ) );
                        break;
                    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO:
                        MKT_CORE_LOGGER_INFO( "[DXGI Info Producer {}] {}", message->pDescription, windows::GuidToString( message->Producer ) );
                        break;
                    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE:
                        MKT_CORE_LOGGER_TRACE( "[DXGI Message Producer {}] {}", message->pDescription, windows::GuidToString( message->Producer ) );
                        break;
                }
            }
        }

        mDxgiInfoQueue->ClearStoredMessages( DXGI_DEBUG_ALL );
    }
#endif

    auto Context::Init() -> bool {
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&mDxgiFactory)))) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Unable to create DXGIFactory" );
            return false;
        }

        // Init the device when the context is ready
        mDevice = IGpuDevice::Create({ .mApi = GraphicsAPI::eD3D11 });
        if (!mDevice) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Could not initialize DIRECTX_11 GPU Device." );
        }
        mDevice->Init();

        // If no window is provided we use D3D11 headless
        if (mWindow) {
            if (!InitializeSwapchain()) {
                MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Failed to create swapchain" );
                return false;
            }
        }


#if !defined(NDEBUG)
        if ( FAILED( DXGIGetDebugInterface1( 0, IID_PPV_ARGS( &mDxgiInfoQueue ) ) ) ) {
        }
#endif

        return true;
    }

    auto Context::Shutdown() -> void {
        mPresentTarget.Reset();
    }

    auto Context::SubmitFrame() -> void {
        if (mWindow->GetWidth() != mSwapChain->GetWidth() || mWindow->GetHeight() != mSwapChain->GetHeight()) {
            mSwapChain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
        }

        // TODO: instead render a full quad sampling the final image
        // otherwise it is annoying as you ned ensure the texture you rendered to before is compatible with swap chain image to copy
        if (!mPresentTarget.IsEmpty()) {
            Device* device{ checked_cast<Device*>( mDevice.get() ) };

            ID3D11Texture2D* presentTargetRTV{ mPresentTarget->GetNativeHandle( ObjectType::D3D11_Texture2D ) };
            ID3D11Texture2D* swapChainRTV{ mSwapChain->GetNativeHandle( ObjectType::D3D11_Texture2D ) };

            device->GetDeviceContext()->CopyResource(swapChainRTV, presentTargetRTV );
        }
    }

    auto Context::PrepareFrame() -> void {
        mDevice->RunGarbageCollection();
    }

    auto Context::Update() -> void {
#if !defined(NDEBUG)
        DumpDXGIMessages();
        as<Device*>( GetGpuDevice() )->DumpErrorMessages();
#endif
    }

    auto Context::Present() -> void {
        mSwapChain->Present();
    }

    auto Context::SetPresentTarget( TextureHandle texture ) -> void {
        mPresentTarget = texture;
    }

    auto Context::SetRefreshRate( RefreshRate rate ) -> void {
        mRefreshRate = rate;
        mSwapChain->SetRefreshRate( mRefreshRate );
    }

    auto Context::BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void {

    }

    auto Context::GetSwapChain() const -> SwapChainHandle {
        return mSwapChain;
    }

    auto Context::GetDxiFactory() const -> IDXGIFactory2* {
        return mDxgiFactory.Get();
    }

    auto Context::InitializeSwapchain() -> bool {
        mSwapChain = checked_cast<Device*>( GetGpuDevice() )->CreateSwapChain( mWindow, mDxgiFactory );
        if (!mSwapChain.IsEmpty()) {
            mSwapChain->SetRefreshRate( mRefreshRate );
        }

        return !mSwapChain.IsEmpty();
    }
}

#endif
