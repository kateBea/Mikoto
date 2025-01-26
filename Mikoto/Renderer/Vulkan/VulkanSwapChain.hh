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

namespace Mikoto {
    /**
     * This structure holds the data required to create a VulkanSwapChain object.
     * */
    struct VulkanSwapChainCreateInfo {
        VkSwapchainKHR OldSwapChain{};  /**< The old swap chain, if applicable. */
        VkExtent2D Extent{};            /**< The extent (width and height) of the swap chain. */
        bool VSyncEnable{};             /**< Flag indicating whether vertical synchronization (VSync) is enabled. */
    };


    /**
     * @brief A wrapper class for managing Vulkan swap chains, including
     * creation, retrieval of images, and presentation.
     * */
    class VulkanSwapChain {
    public:
        /**
         * Default constructor for VulkanSwapChain.
         * */
        explicit VulkanSwapChain() = default;


        /**
         * Creates a VulkanSwapChain object with the provided creation information.
         * @param createInfo Details required for creating the swap chain.
         * */
        auto OnCreate(VulkanSwapChainCreateInfo&& createInfo) -> void;


        /**
         * Retrieves the number of images in the swap chain.
         * @return The number of images in the swap chain.
         * */
        MKT_NODISCARD auto GetImageCount() const -> Size_T { return m_SwapChainImages.size(); }


        /**
         * Retrieves the Vulkan image at the specified index in the swap chain.
         * @param index The index of the image to retrieve.
         * @return The Vulkan image at the specified index.
         * */
        MKT_NODISCARD auto GetImage( const Size_T index ) const -> VkImage { return m_SwapChainImages[index]; }


        /**
         * Retrieves the extent (width and height) of the swap chain.
         * @return The extent of the swap chain.
         * */
        MKT_NODISCARD auto GetSwapChainExtent() const -> VkExtent2D { return m_SwapChainExtent; }


        /**
         * Retrieves the Vulkan SwapchainKHR handle.
         * @return The Vulkan SwapchainKHR handle.
         * */
        MKT_NODISCARD auto GetSwapChainKHR() const -> VkSwapchainKHR { return m_SwapChain; }


        /**
         * Checks if vertical synchronization (VSync) is enabled for the swap chain.
         * @return True if VSync is enabled, false otherwise.
         * */
        MKT_NODISCARD auto IsVsyncEnabled() const -> bool { return m_SwapChainDetails.VSyncEnable; }


        /**
         * Presents the specified image to the screen.
         * @param imageIndex The index of the image to present.
         * @param renderFinished The semaphore indicating that rendering has finished for the presented image.
         * @return The VulkanResult indicating the success or failure of the presentation.
         * */
        MKT_NODISCARD auto Present(UInt32_T imageIndex, VkSemaphore renderFinished) -> VkResult;


        /**
         * Retrieves the default creation information for VulkanSwapChain.
         * @return The default VulkanSwapChainCreateInfo.
         * */
        MKT_NODISCARD auto GetSwapChainCreateInfo() const -> const VulkanSwapChainCreateInfo& { return m_SwapChainDetails; }


        /**
         * Acquires the index of the next image that we can render to, along with optional synchronization objects. Necessary as there's no guarantee what image is available for it.
         * @param imageIndex Reference to store the acquired image index.
         * @param fence The fence to wait on, if provided (default is VK_NULL_HANDLE).
         * @param imageAvailable The semaphore for signaling image availability, if provided (default is VK_NULL_HANDLE).
         * @return The VulkanResult indicating the success or failure of the operation.
         * */
        MKT_NODISCARD auto GetNextRenderableImage(UInt32_T &imageIndex, VkFence fence = VK_NULL_HANDLE, VkSemaphore imageAvailable = VK_NULL_HANDLE ) const -> VkResult;


        /**
         * Retrieves the default creation information for VulkanSwapChain.
         * @return The default VulkanSwapChainCreateInfo.
         * */
        MKT_NODISCARD static auto GetDefaultCreateInfo() -> VulkanSwapChainCreateInfo;


        /**
         * Releases resources associated with the VulkanSwapChain.
         * */
        auto OnRelease() -> void;


        /**
         * Default destructor for VulkanSwapChain.
         * */
        ~VulkanSwapChain() = default;


    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanSwapChain);


    private:
        /**
         * Creates the Vulkan swap chain.
         * */
        auto CreateSwapChain() -> void;


        /**
         * Chooses the optimal surface format for the swap chain.
         * @param availableFormats The available surface formats.
         * @return The chosen surface format.
         * */
        MKT_NODISCARD static auto ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) -> VkSurfaceFormatKHR;


        /**
         * Chooses the optimal present mode for the swap chain.
         * @param availablePresentModes The available present modes.
         * @return The chosen present mode.
         * */
        MKT_NODISCARD auto ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const -> VkPresentModeKHR;


        /**
         * Chooses the optimal extent for the swap chain.
         * @param capabilities The capabilities of the surface.
         * @return The chosen extent.
         * */
        MKT_NODISCARD auto ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) -> VkExtent2D;


        /**
         * Acquires the swap chain images.
         * */
        auto AcquireSwapchainImages() -> void;


    private:
        /**
         * We may sometimes have to wait on the driver to complete internal operations
         * before we can acquire another image to render to. Therefore, it is recommended
         * to request at least one more image
         * */
        static constexpr UInt32_T EXTRA_IMAGE_REQUEST{ 1 };    /**< Additional images to request for waiting on driver operations. */
        static constexpr Int32_T MAX_FRAMES_IN_FLIGHT{ 2 };    /**< Maximum number of frames that can be processed concurrently. */

        VulkanSwapChainCreateInfo       m_SwapChainDetails{}; /**< Details for creating the VulkanSwapChain. */
        VkExtent2D                      m_SwapChainExtent{};  /**< Extent of the VulkanSwapChain. */
        VkSwapchainKHR                  m_SwapChain{};        /**< Vulkan SwapchainKHR handle. */
        std::vector<VkImage>            m_SwapChainImages{};  /**< Vulkan images in the swap chain. */
        VkExtent2D                      m_WindowExtent{};     /**< Extent of the window associated with the swap chain. */
        Size_T                          m_CurrentFrame{};     /**< Index of the current frame being processed. */
    };
}

#endif // MIKOTO_VULKAN_SWAP_CHAIN_HH