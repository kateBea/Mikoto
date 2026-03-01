//    Copyright 2025 ケイト
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

#ifndef MIKOTO_VULKAN_TEXTURE2D_HH
#define MIKOTO_VULKAN_TEXTURE2D_HH

#include <filesystem>

#include <volk.h>

#include <Common/Common.hh>

#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>

#include <Material/Texture2D.hh>
#include <Material/TextureCube.hh>

#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace Mikoto {

    class VulkanSampler final : public Sampler {
    public:
        explicit VulkanSampler( const SamplerDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~VulkanSampler() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        VkSampler m_Sampler{ VK_NULL_HANDLE };
        VkSamplerCreateInfo m_CreateInfo{};
    };


    class VulkanTexture final : public Texture2D {
    public:
        explicit VulkanTexture( const TextureDescription& data );
        explicit VulkanTexture( const VkImageViewCreateInfo& viewCreateInfo, VkExtent2D extent);

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto HasExternalImage() const -> bool;

        MKT_NODISCARD auto GetCurrentLayout() const -> VkImageLayout;
        MKT_NODISCARD auto GetCreateInfo() const -> const VkImageCreateInfo&;
        MKT_NODISCARD auto GetViewCreateInfo() const -> const VkImageViewCreateInfo&;

        auto SetDebugName(std::string_view name) -> void override;

        MKT_NODISCARD auto IsSwapChainImage() const -> bool;

        auto SubmitLayoutTransition( VkImageLayout newLayout, VkCommandBuffer cmd, bool insertBarrier = true ) -> void;

        ~VulkanTexture() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto SetDebugInfo() -> void;
        auto SetupNonSwapChainImage() -> void;

    private:
        // Optional
        Size m_ExternalBufferSize{};

        bool m_IsImageExternal{ false };

        VkDeviceSize m_ImageSize{ 0 };

        ImageAllocation m_ImageAllocation{};

        VkImageView m_ImageView{ VK_NULL_HANDLE };
        VkImageViewCreateInfo m_ImageViewCreateInfo{};

        VkImageLayout m_CurrentLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
    };

    class VulkanTextureCube final : public TextureCube {
    public:
        explicit VulkanTextureCube( const TextureCubeCreateDescription& data );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto GetCurrentLayout() const -> VkImageLayout;
        MKT_NODISCARD auto GetCreateInfo() const -> const VkImageCreateInfo&;
        MKT_NODISCARD auto GetViewCreateInfo() const -> const VkImageViewCreateInfo&;

        auto SetDebugName(std::string_view name) -> void override;

        auto SubmitLayoutTransition( VkImageLayout newLayout, VkCommandBuffer cmd, bool insertBarrier = true ) -> void;

        ~VulkanTextureCube() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto LoadCubeFaces() -> void;
        auto LoadWithImageLoader() -> void;
        auto CreateImageResource() -> void;

        auto SetDebugInfo() -> void;

    private:
        VkDeviceSize m_ImageSize{ 0 };

        ImageAllocation m_ImageAllocation{};

        VkImageView m_ImageView{ VK_NULL_HANDLE };
        VkImageViewCreateInfo m_ImageViewCreateInfo{};

        VkImageLayout m_CurrentLayout{ VK_IMAGE_LAYOUT_UNDEFINED };

        std::vector<const File*> m_TextureFaces{};

        bool m_UseImageLoader{ false };
    };

    struct VulkanSwapChainCreateInfo {
        VkExtent2D Extent{};
        VkSurfaceKHR* Surface{};
        bool EnableVsync{ false };
    };

    class VulkanSwapChain final : public DeviceObject {
    public:
        explicit VulkanSwapChain( const VulkanSwapChainCreateInfo& createInfo );

        MKT_NODISCARD auto GetImplHandle() -> VkSwapchainKHR* { return std::addressof(m_Swapchain); }

        MKT_NODISCARD auto Present( UInt32 imageIndex, const VkSemaphore& renderFinished ) const -> VkResult;

        MKT_NODISCARD auto GetImageCount() const -> Size;

        MKT_NODISCARD auto GetImage( Size index ) -> TextureHandle;

        MKT_NODISCARD auto GetExtent() const -> VkExtent2D;

        MKT_NODISCARD auto IsVsyncEnabled() const -> bool;

        MKT_NODISCARD auto GetNextRenderableImageIndex( UInt32& imageIndex, VkSemaphore imageAvailable = VK_NULL_HANDLE ) const -> VkResult;

        // TODO: Destroy this swap chain and create a new one
        // you do not really want to do the swap chain of creating a new one, that is maybe in the cae you have another window
        // but if you want to recreate this swap chain because a resizing of the surface happened, then you don't need a new one
        // as in new VulkanSwapChain instance you just need a "recreate logic"
        auto OnResize(VkExtent2D newDimensions, bool vsync = false) -> void;

        ~VulkanSwapChain() override;

        DISABLE_COPY_AND_MOVE_FOR( VulkanSwapChain );

        using DeviceObject::Initialize;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

        auto CreateSwpChain() -> void;
        auto GetImages() -> void;

        MKT_NODISCARD auto ChoosePresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes ) const -> VkPresentModeKHR;
        MKT_NODISCARD auto ChooseExtent( const VkSurfaceCapabilitiesKHR& capabilities ) const -> VkExtent2D;

        MKT_NODISCARD static auto ConstructImgViewInfo( VkImage image, const VkFormat& format ) -> VkImageViewCreateInfo;
        MKT_NODISCARD static auto ChooseSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats ) -> VkSurfaceFormatKHR;

    private:
        VkExtent2D m_Extent{};
        VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };
        VkSwapchainKHR m_OldSwapchain{ VK_NULL_HANDLE };

        VkFormat m_Format{};
        VkPresentModeKHR m_PresentMode{};

        std::vector<TextureHandle> m_Images{};

        VkSurfaceKHR* m_Surface{ nullptr };

        bool m_IsVsyncEnabled{};
    };

    using SwapChainHandle = Ref<VulkanSwapChain>;
}// namespace Mikoto

#endif// MIKOTO_VULKAN_TEXTURE2D_HH
