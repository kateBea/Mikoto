/**
 * VulkanSwapChain.hh
 * Created by kate on 12/7/23.
 * */

#ifndef MIKOTO_VULKAN_SWAP_CHAIN_HH
#define MIKOTO_VULKAN_SWAP_CHAIN_HH

// C++ Standard Library
#include <memory>
#include <string>
#include <vector>

// Third-Party Libraries
#include <volk.h>
#include <GLFW/glfw3.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Vulkan/VulkanObject.hh>

namespace Mikoto {
    struct VulkanSwapChainCreateInfo {
        VkExtent2D Extent{};
        VkSurfaceKHR* Surface{};
        bool EnableVsync{ false };
        VkSwapchainKHR OldSwapChain{ VK_NULL_HANDLE };
    };

    class VulkanSwapChain final : VulkanObject {
    public:

        explicit VulkanSwapChain(const VulkanSwapChainCreateInfo& createInfo);

        auto Init() -> void;

        MKT_NODISCARD auto Present( UInt32_T imageIndex, const VkSemaphore &renderFinished ) -> VkResult;

        MKT_NODISCARD auto GetImageCount() const -> Size_T { return m_Images.size(); }
        MKT_NODISCARD auto GetImage( const Size_T index ) const -> VkImage { return m_Images[index]->Get(); }
        MKT_NODISCARD auto GetSwapChainKHR() const -> VkSwapchainKHR { return m_Swapchain; }
        MKT_NODISCARD auto GetExtent() const -> VkExtent2D { return m_Extent; }
        MKT_NODISCARD auto IsVsyncEnabled() const -> bool { return m_IsVsyncEnabled; }
        MKT_NODISCARD auto GetNextRenderableImage(UInt32_T &imageIndex, VkFence fence = VK_NULL_HANDLE, VkSemaphore imageAvailable = VK_NULL_HANDLE ) const -> VkResult;

        auto Release() -> void override;

        ~VulkanSwapChain() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanSwapChain);

    private:
        struct SwapchainInfo {
            VkFormat Format{};
            VkPresentModeKHR PresentMode{};
        };

    private:
        auto CreateSwapChain() -> void;
        auto AcquireSwapchainImages() -> void;
        MKT_NODISCARD auto ChoosePresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const -> VkPresentModeKHR;
        MKT_NODISCARD auto ChooseExtent(const VkSurfaceCapabilitiesKHR &capabilities ) const -> VkExtent2D;

        MKT_NODISCARD static auto CreateSwapchainImageViewCreateInfo(const VkImage& image, const VkFormat& format ) -> VkImageViewCreateInfo;
        MKT_NODISCARD static auto ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) -> VkSurfaceFormatKHR;

    private:
        /**
         * Maximum number of frames that can be processed concurrently.
         * */
        static constexpr Int32_T MAX_FRAMES_IN_FLIGHT{ 2 };

    private:

        VkExtent2D  m_Extent{};
        VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };
        VkSwapchainKHR m_OldSwapChain{ VK_NULL_HANDLE };
        SwapchainInfo m_SwapchainInfo{};
        std::vector<Scope_T<VulkanImage>> m_Images{};

        VkSurfaceKHR* m_Surface{ nullptr };

        bool m_IsVsyncEnabled{};

        Size_T m_CurrentFrame{};
    };
}

#endif // MIKOTO_VULKAN_SWAP_CHAIN_HH