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

#ifndef MIKOTO_D3D12CONTEXT_HH
#define MIKOTO_D3D12CONTEXT_HH

#include <Core/Platform.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/RenderContext.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>

#include <dxgi1_4.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <Renderer/D3D12/D3D12SwapChain.hh>

namespace mikoto::renderer::d3d12 {

    // Refs: https://alain.xyz/blog/raw-directx12
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

        // D3D12 Specifics
        MKT_NODISCARD auto GetSwapChain() const -> SwapChainHandle;
        MKT_NODISCARD auto GetDxiFactory() const -> IDXGIFactory4*;

        ~Context() override = default;

    private:
        SwapChainHandle mSwapChain{};
        Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory{};

#if !defined(NDEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> mDebug{};
        Microsoft::WRL::ComPtr<ID3D12Debug1> mDebugController{};
#endif
    };
}

#endif


#endif//MIKOTO_D3D12CONTEXT_HH
