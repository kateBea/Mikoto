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
#include <Utility/Common.hh>

namespace Mikoto {
    /**
     * This structure holds the data required to create a
     * a VulkanSwapChain object
     * */
    struct VulkanSwapChainCreateInfo {
        VkSwapchainKHR OldSwapChain{}; // For use in future
        VkExtent2D Extent{};
        bool VSyncEnable{};
    };

    class VulkanSwapChain {
    public:
        explicit VulkanSwapChain() = default;

        auto OnCreate(VulkanSwapChainCreateInfo&& createInfo) -> void;

        MKT_NODISCARD auto GetFrameBuffer(Size_T index) -> VkFramebuffer { return m_SwapChainFrameBuffers[index]; }
        MKT_NODISCARD auto GetRenderPass() const -> VkRenderPass { return m_RenderPass; }
        MKT_NODISCARD auto GetImageView(Size_T index) -> VkImageView { return m_SwapChainImageViews[index]; }
        MKT_NODISCARD auto GetImageCount() -> Size_T { return m_SwapChainImages.size(); }
        MKT_NODISCARD auto GetSwapChainImageFormat() -> VkFormat { return m_SwapChainImageFormat; }
        MKT_NODISCARD auto GetSwapChainExtent() -> VkExtent2D { return m_SwapChainExtent; }
        MKT_NODISCARD auto GetSwapChainKHR() const -> VkSwapchainKHR { return m_SwapChain; }
        MKT_NODISCARD auto IsVsyncEnabled() const -> bool { return m_SwapChainDetails.VSyncEnable; }

        MKT_NODISCARD auto GetWidth() const -> UInt32_T { return m_SwapChainExtent.width; }
        MKT_NODISCARD auto GetHeight() const -> UInt32_T { return m_SwapChainExtent.height; }
        MKT_NODISCARD auto GetExtentAspectRatio() const -> float { return static_cast<float>(m_SwapChainExtent.width) / static_cast<float>(m_SwapChainExtent.height); }
        MKT_NODISCARD auto GetSwapChainCreateInfo() const -> const VulkanSwapChainCreateInfo& { return m_SwapChainDetails; }
        MKT_NODISCARD auto GetNextImage(UInt32_T *imageIndex) -> VkResult;
        MKT_NODISCARD auto SubmitCommandBuffers(const VkCommandBuffer* buffers, UInt32_T imageIndex) -> VkResult;
        MKT_NODISCARD auto GetCurrentFrame() const -> UInt32_T { return m_CurrentFrame; }
        MKT_NODISCARD auto GetFrameBufferAtIndex(Size_T index) -> VkFramebuffer { return m_SwapChainFrameBuffers[index]; }

        MKT_NODISCARD static auto FindDepthFormat() -> VkFormat;
        MKT_NODISCARD static auto GetDefaultCreateInfo() -> VulkanSwapChainCreateInfo;

        auto OnRelease() -> void;

        ~VulkanSwapChain() = default;
    public:
        // Forbidden operations
        VulkanSwapChain(const VulkanSwapChain &) = delete;
        auto operator=(const VulkanSwapChain &) -> VulkanSwapChain& = delete;

    private:
        auto CreateSwapChain() -> void;
        auto CreateImageViews() -> void;
        auto CreateDepthResources() -> void;
        auto CreateRenderPass() -> void;
        auto CreateFrameBuffers() -> void;
        auto CreateSyncObjects() -> void;

        MKT_NODISCARD static auto ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) -> VkSurfaceFormatKHR;
        MKT_NODISCARD auto ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const -> VkPresentModeKHR;
        MKT_NODISCARD auto ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) -> VkExtent2D;

        auto AcquireSwapchainImages(UInt32_T& imageCount) -> void;

        static auto CreateImageWithInfo(const VkImageCreateInfo &imageInfo, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) -> void;
    private:
        /**
         * Maximum number of frames that can be processed concurrently
         * */
        static constexpr Int32_T MAX_FRAMES_IN_FLIGHT{ 2 };

        /**
         * We may sometimes have to wait on the driver to complete internal operations
         * before we can acquire another image to render to. Therefore, it is recommended
         * to request at least one more image
         * */
        const UInt32_T EXTRA_IMAGE_REQUEST{ 1 };

        VulkanSwapChainCreateInfo       m_SwapChainDetails{};

        VkFormat                        m_SwapChainImageFormat{};
        VkExtent2D                      m_SwapChainExtent{};

        std::vector<VkFramebuffer>      m_SwapChainFrameBuffers{};
        VkRenderPass                    m_RenderPass{};

        std::vector<VkImage>            m_DepthImages{};
        std::vector<VkDeviceMemory>     m_DepthImageMemories{};
        std::vector<VkImageView>        m_DepthImageViews{};

        std::vector<VkImage>            m_SwapChainImages{};
        std::vector<VkImageView>        m_SwapChainImageViews{};

        VkExtent2D                      m_WindowExtent{};
        VkSwapchainKHR                  m_SwapChain{};

        std::vector<VkSemaphore>        m_ImageAvailableSemaphores{};
        std::vector<VkSemaphore>        m_RenderFinishedSemaphores{};
        std::vector<VkFence>            m_InFlightFences{};
        std::vector<VkFence>            m_ImagesInFlight{};
        Size_T                     m_CurrentFrame{};
    };
}

#endif // MIKOTO_VULKAN_SWAP_CHAIN_HH