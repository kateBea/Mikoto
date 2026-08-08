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

#ifndef MIKOTO_D3D11_SWAPCHAIN_HH
#define MIKOTO_D3D11_SWAPCHAIN_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/ReferenceCounted.hh>

#include <Platform/Window.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Swapchain.hh>

#include <Renderer/Core/RenderContext.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl.h>

namespace mikoto::renderer::d3d11 {

    class SwapChain final : public rhi::DeviceObject {
    public:
        explicit SwapChain(platform::Window* window, Microsoft::WRL::ComPtr<IDXGIFactory2> mDxgiFactory);

        auto Present() -> void;

        auto OnResize( core::u32 width, core::u32 height ) -> void;
        auto SetRefreshRate( RefreshRate type ) -> void;

        MKT_NODISCARD auto GetWidth() const -> core::u32;
        MKT_NODISCARD auto GetHeight() const -> core::u32;

        MKT_NODISCARD auto GetFormat() const -> rhi::Format;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~SwapChain() override;

        using DeviceObject::Initialize;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto CreateSwapchainResources() -> void;

    private:
        platform::Window* mWindow{ nullptr };

        core::u32 mWidth{ 0u };
        core::u32 mHeight{ 0u };

        rhi::Format mFormat{ Format::eBGRA8_UNORM };
        RefreshRate mRefreshRate{ RefreshRate::eUnlimited };

        Microsoft::WRL::ComPtr<IDXGISwapChain1> mSwapChain{};
        Microsoft::WRL::ComPtr<IDXGIFactory2> mDxgiFactory{};
        Microsoft::WRL::ComPtr<ID3D11Texture2D> mBackBuffer{};
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mRenderTarget{};
    };

    using SwapChainHandle = Ref<SwapChain>;

}// namespace mikoto

#endif//MIKOTO_D3D11_SWAPCHAIN_HH

#endif
