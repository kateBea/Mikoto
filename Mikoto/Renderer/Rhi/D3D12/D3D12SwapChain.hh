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

#ifndef MIKOTOROOT_D3D12_SWAPCHAIN_HH
#define MIKOTOROOT_D3D12_SWAPCHAIN_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/ReferenceCounted.hh>

#include <Platform/Window.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Swapchain.hh>

#include <Renderer/Core/RenderContext.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D12/Direct3D12Libraries.hh>

namespace mikoto::renderer::d3d12 {

    class SwapChain final : public rhi::DeviceObject {
    public:
        explicit SwapChain(platform::Window* window, Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory);

        auto Present() -> void;

        auto OnResize( core::u32 width, core::u32 height ) -> void;
        auto SetRefreshRate( rhi::RefreshRate type ) -> void;

        MKT_NODISCARD auto GetWidth() const -> core::u32;
        MKT_NODISCARD auto GetHeight() const -> core::u32;

        MKT_NODISCARD auto GetCurrentBackBufferImage() const -> rhi::TextureHandle;

        MKT_NODISCARD auto GetFormat() const -> rhi::Format;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~SwapChain() override;

        using DeviceObject::Initialize;

        DISABLE_COPY_AND_MOVE_FOR( SwapChain );

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        platform::Window* mWindow{ nullptr };

        core::u32 mWidth{ 0u };
        core::u32 mHeight{ 0u };

        rhi::Format mFormat{ rhi::Format::eBGRA8_UNORM };
        rhi::RefreshRate mRefreshRate{ rhi::RefreshRate::eUnlimited };

        // TODO: remove these, descriptor heaps are managed by the device
        // the backbuffer resources will be part of the Texture class
        UINT mRtvDescriptorSize{};
        eastl::vector<ID3D12Resource*> mRenderTargetsViews{};
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRenderTargetViewHeap{};

        eastl::vector<rhi::TextureHandle> mBackBufferImages{};

        D3D12_RECT mSurfaceSize{};
        D3D12_VIEWPORT mViewportDescription{};

        Microsoft::WRL::ComPtr<IDXGISwapChain4> mSwapChain{};
        Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory{};
    };

    using SwapChainHandle = core::Ref<SwapChain>;

}// namespace mikoto

#endif

#endif//MIKOTOROOT_D3D12_SWAPCHAIN_HH
