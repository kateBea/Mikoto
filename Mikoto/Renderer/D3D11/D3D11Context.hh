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

#ifndef MIKOTO_D3D11CONTEXT_HH
#define MIKOTO_D3D11CONTEXT_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Logging/Assert.hh>

#include <Renderer/Core/RenderContext.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/D3D11/D3D11SwapChain.hh>
#include <Renderer/D3D11/Direct3D11Libraries.hh>

#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl.h>
#include <dxgidebug.h>

// https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics

namespace mikoto::renderer::d3d11 {

    class Context final : public RenderContext {
    public:
        explicit Context(const RenderContextCreateInfo& createInfo)
           :  RenderContext{ createInfo }
        { }

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto SubmitFrame() -> void override;
        auto PrepareFrame() -> void override;

        auto Update() -> void override;

        auto Present() -> void override;
        auto SetPresentTarget( TextureHandle texture ) -> void override;
        auto SetRefreshRate( RefreshRate rate ) -> void override;

        // D3D11 Specifics
        MKT_NODISCARD auto GetSwapChain() const -> SwapChainHandle;
        MKT_NODISCARD auto GetDxiFactory() const -> IDXGIFactory2*;

        auto DumpDXGIMessages() -> void;

        ~Context() override = default;

    private:
        MKT_NODISCARD auto CreateSwapChain() -> bool;

    private:
        TextureHandle mPresentTarget{};

        SwapChainHandle mSwapChain{};
        Microsoft::WRL::ComPtr<IDXGIFactory2> mDxgiFactory{};

#if !defined(NDEBUG)
        Microsoft::WRL::ComPtr<IDXGIInfoQueue> mDxgiInfoQueue{};
#endif
    };

#define MKT_D3D11_CHECK( expr, message )                            \
    do {                                                            \
        HRESULT hr__{ ( expr ) };                                   \
        checked_cast<Context*>( RenderSystem::Get()->GetContext() ) \
                ->DumpDXGIMessages();                               \
                                                                    \
        if ( FAILED( hr__ ) ) {                                     \
            MKT_ASSERT( false, message );                           \
        }                                                           \
    } while ( 0 )
}// namespace mikoto::renderer::d3d11

#endif


#endif//MIKOTO_D3D11CONTEXT_HH
