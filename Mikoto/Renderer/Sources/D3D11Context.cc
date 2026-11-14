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
#include <Core/CVar.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Platform/Utility.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <wrl.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Context.hh>

namespace mikoto::renderer::d3d11 {

    using namespace mikoto::core;
    using namespace mikoto::platform;

#if !defined( NDEBUG )

    // https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-debug-id
    auto Context::DumpDXGIMessages() -> void {
        MKT_BEGIN_PROFILER();
        const auto messageCount{ mDxgiInfoQueue->GetNumStoredMessages( DXGI_DEBUG_ALL ) };

        for ( UINT64 i{}; i < messageCount; ++i ) {
            core::usize messageLength{};
            mDxgiInfoQueue->GetMessage( DXGI_DEBUG_ALL, i, nullptr, &messageLength );

            auto bytes{ eastl::make_unique<std::byte[]>( messageLength ) };
            auto* message{ rc_cast<DXGI_INFO_QUEUE_MESSAGE*>( bytes.get() ) };

            if ( SUCCEEDED( mDxgiInfoQueue->GetMessage( DXGI_DEBUG_ALL, i, message, &messageLength ) ) ) {
                switch ( message->Severity ) {
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
        MKT_BEGIN_PROFILER();

        if ( FAILED( CreateDXGIFactory1( IID_PPV_ARGS( &mDxgiFactory ) ) ) ) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Unable to create DXGIFactory" );
            return false;
        }

        const eastl::string deviceTypeString{ core::CVarRegistry::Get()->GetValue<eastl::string>( "device_type" ) };
        const renderer::rhi::GpuDeviceType deviceType{ rhi::InferGpuDeviceType( deviceTypeString ) };

        MKT_ASSERT( deviceType != renderer::rhi::GpuDeviceType::eInvalid, "Invalid device type." );

        // Init the device when the context is ready
        mDevice = renderer::rhi::IGpuDevice::Create( { .mApi = renderer::rhi::GraphicsAPI::eD3D11,
            .mDeviceType = deviceType,
            .mFeatureSupportFlags = GetRequestedDeviceFeatures() } );

        if ( !mDevice ) {
            MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Could not initialize DIRECTX_11 GPU Device." );
            return false;
        }

        mDevice->Initialize();

        // If no window is provided we use D3D11 headless
        if ( mWindow ) {
            if ( !InitializeSwapchain() ) {
                MKT_CORE_LOGGER_ERROR( "D3D11Context::Init - Failed to create swapchain" );
                return false;
            }
        }


#if !defined( NDEBUG )
        if ( FAILED( DXGIGetDebugInterface1( 0, IID_PPV_ARGS( &mDxgiInfoQueue ) ) ) ) {
        }
#endif

        return true;
    }

    auto Context::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        mPresentTarget.Reset();
    }

    auto Context::SubmitFrame() -> void {
        MKT_BEGIN_PROFILER();
        // Submit batched commands
        decltype( mBatchedSubmissions ) submissions{};
        {
            std::lock_guard lock{ mBatchedSubmissionMutex };
            submissions.swap( mBatchedSubmissions );
        }

        for ( const auto& [queue, submitInfo]: submissions ) {
            queue->ExecuteCommandLists( submitInfo );
        }

        if ( mWindow->GetWidth() != mSwapChain->GetWidth() || mWindow->GetHeight() != mSwapChain->GetHeight() ) {
            mSwapChain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
        }

        // TODO: instead render a full quad sampling the final image
        // otherwise it is annoying as you ned ensure the texture you rendered to before is compatible with swap chain image to copy
        if ( !mPresentTarget.IsEmpty() ) {
            Device* device{ core::checked_cast<Device*>( mDevice.get() ) };

            ID3D11Texture2D* presentTargetRTV{ mPresentTarget->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Texture2D ) };
            ID3D11Texture2D* swapChainRTV{ mSwapChain->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Texture2D ) };

            device->GetDeviceContext()->CopyResource( swapChainRTV, presentTargetRTV );
        }
    }

    auto Context::PrepareFrame() -> void {
        MKT_BEGIN_PROFILER();
        mDevice->RunGarbageCollection();
    }

    auto Context::Update() -> void {
        MKT_BEGIN_PROFILER();
#if !defined( NDEBUG )
        DumpDXGIMessages();
        core::checked_cast<Device*>( GetGpuDevice() )->DumpErrorMessages();
#endif
    }

    auto Context::Present() -> void {
        MKT_BEGIN_PROFILER();
        mSwapChain->Present();
    }

    auto Context::SetPresentTarget( renderer::rhi::TextureHandle texture ) -> void {
        MKT_BEGIN_PROFILER();
        mPresentTarget = texture;
    }

    auto Context::SetRefreshRate( renderer::rhi::RefreshRate rate ) -> void {
        MKT_BEGIN_PROFILER();
        mRefreshRate = rate;
        mSwapChain->SetRefreshRate( mRefreshRate );
    }

    auto Context::BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mBatchedSubmissionMutex };
        auto* target{ mDevice->GetQueue( queue ) };

        if ( !target ) {
            MKT_THROW_INVALID_ARGUMENT( "Unsupported D3D11 submission queue." );
        }

        mBatchedSubmissions.emplace_back( target, eastl::move( submitInfo ) );
    }

    auto Context::GetSwapChain() const -> D3D11SwapChainHandle {
        MKT_BEGIN_PROFILER();
        return mSwapChain;
    }

    auto Context::GetDxiFactory() const -> IDXGIFactory2* {
        MKT_BEGIN_PROFILER();
        return mDxgiFactory.Get();
    }

    auto Context::InitializeSwapchain() -> bool {
        MKT_BEGIN_PROFILER();
        mSwapChain = core::checked_cast<Device*>( GetGpuDevice() )->CreateSwapChain( mWindow, mDxgiFactory );

        if ( !mSwapChain.IsEmpty() ) {
            mSwapChain->SetRefreshRate( mRefreshRate );
        }

        return !mSwapChain.IsEmpty();
    }

    Context::Context( const RenderContextCreateInfo& createInfo )
        : RenderContext{ createInfo } {
    }
}
#endif
