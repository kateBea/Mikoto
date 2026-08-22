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

#ifndef MIKOTO_VULKAN_SWAP_CHAIN_HH
#define MIKOTO_VULKAN_SWAP_CHAIN_HH

#include <EASTL/vector.h>
#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ResourcePool.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Swapchain.hh>

#include <Renderer/Core/RenderContext.hh>

#include <Renderer/Rhi/Vulkan/VulkanTexture.hh>
#include <Renderer/Rhi/Vulkan/VulkanInstance.hh>

namespace mikoto::renderer::vulkan {

    class BinarySemaphore;

    struct SwapChainCreateInfo {
        PhysicalDevice* mPhysicalDevice{};

        core::u32 mWidth{};
        core::u32 mHeight{};

        VkSurfaceKHR mSurface{};
        rhi::RefreshRate mRefreshRate{ rhi::RefreshRate::eUnlimited };

        rhi::Format mFormat{ rhi::Format::eBGRA8_UNORM };
    };

    class SwapChain final : public rhi::DeviceObject {
    public:
        explicit SwapChain( const SwapChainCreateInfo& createInfo );

        MKT_NODISCARD auto GetWidth() const -> core::u32;
        MKT_NODISCARD auto GetHeight() const -> core::u32;

        MKT_NODISCARD auto GetImageCount() const -> core::size_t;
        MKT_NODISCARD auto GetImage( core::size_t index ) -> rhi::TextureHandle;
        MKT_NODISCARD auto GetFormat() -> rhi::Format;

        MKT_NODISCARD auto Present( core::u32 imageIndex, const BinarySemaphore* signalSemaphore ) -> VkResult;
        MKT_NODISCARD auto GetNextImageIndex( core::u32& imageIndex, const BinarySemaphore* waitSemaphore ) -> VkResult;

        // If changed a call to OnResize is required to apply changes
        auto SetRefreshType( rhi::RefreshRate type ) -> void;

        auto OnResize( core::u32 width, core::u32 height ) -> void;

        ~SwapChain() override;

        DISABLE_COPY_AND_MOVE_FOR( SwapChain );

        using DeviceObject::Initialize;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

        auto AcquireSwapChainImages() -> void;

    private:
        core::u32 mWidth{};
        core::u32 mHeight{};

        PhysicalDevice* mPhysicalDevice{};

        VkSurfaceKHR mSurface{};
        VkColorSpaceKHR mColorSpace{ VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        VkFormat mSurfaceSupportedFormat{};

        eastl::vector<rhi::TextureHandle> mImages{};

        VkPresentModeKHR mPresentMode{};

        VkSwapchainKHR mSwapChain{ VK_NULL_HANDLE };
        VkSwapchainKHR mOldSwapChain{ VK_NULL_HANDLE };

        rhi::IQueue* mPresentQueue{};
        rhi::Format mFormat{ rhi::Format::eBGRA8_UNORM };
        rhi::RefreshRate mRefreshRate{ rhi::RefreshRate::eUnlimited };
    };

    using SwapChainHandle = core::Ref<SwapChain>;

}// namespace mikoto::renderer::vulkan

#endif//MIKOTO_VULKAN_SWAP_CHAIN_HH
