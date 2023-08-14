#ifndef KATE_ENGINE_VULKAN_SWAP_CHAIN_HH
#define KATE_ENGINE_VULKAN_SWAP_CHAIN_HH

#include <memory>
#include <string>
#include <vector>

#include <volk.h>
#include <GLFW/glfw3.h>

#include <Utility/Common.hh>

namespace Mikoto {
    struct VulkanSwapChainCreateInfo {
        VkExtent2D Extent{};
        bool VSyncEnable{};
    };

    class VulkanSwapChain {
    public:
        // Max number of frames that can be processed concurrently
        static constexpr Int32_T MAX_FRAMES_IN_FLIGHT{ 2 };

        explicit VulkanSwapChain() = default;
        explicit VulkanSwapChain(const VulkanSwapChainCreateInfo& createInfo);

        auto OnCreate(const VulkanSwapChainCreateInfo& createInfo) -> void;

        KT_NODISCARD auto GetFrameBuffer(Size_T index) -> VkFramebuffer { return m_SwapChainFrameBuffers[index]; }
        KT_NODISCARD auto GetRenderPass() const -> VkRenderPass { return m_RenderPass; }
        KT_NODISCARD auto GetImageView(Size_T index) -> VkImageView { return m_SwapChainImageViews[index]; }
        KT_NODISCARD auto GetImageCount() -> Size_T { return m_SwapChainImages.size(); }
        KT_NODISCARD auto GetSwapChainImageFormat() -> VkFormat { return m_SwapChainImageFormat; }
        KT_NODISCARD auto GetSwapChainExtent() -> VkExtent2D { return m_SwapChainExtent; }

        KT_NODISCARD auto GetWidth() const -> UInt32_T { return m_SwapChainExtent.width; }
        KT_NODISCARD auto GetHeight() const -> UInt32_T { return m_SwapChainExtent.height; }
        KT_NODISCARD auto GetExtentAspectRatio() const -> float { return static_cast<float>(m_SwapChainExtent.width) / static_cast<float>(m_SwapChainExtent.height); }
        KT_NODISCARD auto GetSwapChainCreateInfo() const -> const VulkanSwapChainCreateInfo& { return m_SwapChainDetails; }
        KT_NODISCARD auto AcquireNextImage(UInt32_T* imageIndex) -> VkResult;
        KT_NODISCARD auto SubmitCommandBuffers(const VkCommandBuffer* buffers, UInt32_T imageIndex) -> VkResult;
        KT_NODISCARD auto GetCurrentFrame() const -> UInt32_T { return m_CurrentFrame; }
        KT_NODISCARD auto GetFrameBufferAtIndex(Size_T index) -> VkFramebuffer { return m_SwapChainFrameBuffers[index]; }

        KT_NODISCARD static auto FindDepthFormat() -> VkFormat;
        KT_NODISCARD static auto GetDefaultCreateInfo() -> VulkanSwapChainCreateInfo;

        auto OnRelease() const -> void;

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
        auto OnCreate() -> void;

        KT_NODISCARD static auto ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) -> VkSurfaceFormatKHR;
        KT_NODISCARD auto ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const -> VkPresentModeKHR;
        KT_NODISCARD auto ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) -> VkExtent2D;

        static auto CreateImageWithInfo(const VkImageCreateInfo &imageInfo, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) -> void;
    private:
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

#endif //KATE_ENGINE_VULKAN_SWAP_CHAIN_HH