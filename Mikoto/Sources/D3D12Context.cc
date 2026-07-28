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
#include <Core/Platform.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/D3D12/D3D12Context.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    auto Context::Init() -> bool {
#if !defined(NDEBUG)
        ThrowIfFailed( D3D12GetDebugInterface( IID_PPV_ARGS( &mDebug ) ) );
        ThrowIfFailed( mDebug->QueryInterface( IID_PPV_ARGS( &mDebugController ) ) );

        mDebugController->EnableDebugLayer();
        mDebugController->SetEnableGPUBasedValidation( true );
#endif

        UINT dxgiFactoryFlags{ DXGI_CREATE_FACTORY_DEBUG };
        HRESULT factoryCreateResult{ SUCCEEDED( CreateDXGIFactory2( dxgiFactoryFlags, IID_PPV_ARGS( &mDxgiFactory ) ) ) };
        MKT_ASSERT( factoryCreateResult, "Failed to create factory" );

        // Init the device when the context is ready
        mDevice = IGpuDevice::Create( { .mApi = GraphicsAPI::eD3D12 } );
        if ( !mDevice ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize D3D12 GPU Device." );
        }
        mDevice->Init();

        return true;
    }

    auto Context::Shutdown() -> void {

    }

    auto Context::SubmitFrame() -> void {

    }

    auto Context::PrepareFrame() -> void {

    }

    auto Context::Update() -> void {

    }

    auto Context::Present() -> void {

    }

    auto Context::SetPresentTarget( TextureHandle texture ) -> void {

    }

    auto Context::SetRefreshRate( RefreshRate rate ) -> void {

    }

    auto Context::GetSwapChain() const -> SwapChainHandle {
        return mSwapChain;
    }

    auto Context::GetDxiFactory() const -> IDXGIFactory4* {
        return mDxgiFactory.Get();
    }
}// namespace Mikoto

#endif
