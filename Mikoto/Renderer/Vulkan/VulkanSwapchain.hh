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

#include <Renderer/Core/Rhi.hh>

#include <Renderer/Core/RenderContext.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>
#include <Renderer/Vulkan/VulkanInstance.hh>

namespace mikoto::renderer::vulkan {

    class BinarySemaphore;

    using namespace mikoto::core;

    struct SwapChainCreateInfo {
        PhysicalDevice* mPhysicalDevice{};

        u32 mWidth{};
        u32 mHeight{};

        VkSurfaceKHR mSurface{};
        RefreshRate mRefreshRate{ RefreshRate::eUnlimited };

        Format mFormat{ Format::eBGRA8_UNORM };
    };

    class SwapChain final : public DeviceObject {
    public:
        explicit SwapChain( const SwapChainCreateInfo& createInfo );

        MKT_NODISCARD auto GetWidth() const -> u32;
        MKT_NODISCARD auto GetHeight() const -> u32;

        MKT_NODISCARD auto GetImageCount() const -> size_t;
        MKT_NODISCARD auto GetImage( size_t index ) -> TextureHandle;
        MKT_NODISCARD auto GetFormat() -> Format;

        MKT_NODISCARD auto Present( u32 imageIndex, const BinarySemaphore& signalSemaphore ) const -> VkResult;
        MKT_NODISCARD auto GetNextImage( u32& imageIndex, const BinarySemaphore& waitSemaphore ) const -> VkResult;

        // If changed a call to OnResize is required to apply changes
        auto SetRefreshType( RefreshRate type ) -> void;

        auto OnResize(u32 width, u32 height) -> void;

        ~SwapChain() override;

        DISABLE_COPY_AND_MOVE_FOR( SwapChain );

        using DeviceObject::Initialize;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

        auto AcquireSwapChainImages() -> void;

    private:
        u32 mWidth{};
        u32 mHeight{};

        PhysicalDevice* mPhysicalDevice{};

        VkSurfaceKHR mSurface{};
        VkColorSpaceKHR mColorSpace{ VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        VkFormat mSurfaceSupportedFormat{};

        eastl::vector<TextureHandle> mImages{};

        VkPresentModeKHR mPresentMode{};

        VkSwapchainKHR mSwapChain{ VK_NULL_HANDLE };
        VkSwapchainKHR mOldSwapChain{ VK_NULL_HANDLE };

        Format mFormat{ Format::eBGRA8_UNORM };
        RefreshRate mRefreshRate{ RefreshRate::eUnlimited };
    };

    using SwapChainHandle = Ref<SwapChain>;

}// namespace mikoto

#endif//MIKOTO_VULKAN_SWAP_CHAIN_HH
