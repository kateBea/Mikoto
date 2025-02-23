/**
 * VulkanSwapChain.cc
 * Created by kate on 12/7/23.
 * */

// C++ Standard Library
#include <limits>
#include <stdexcept>
#include <utility>
#include <array>
#include <algorithm>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanSwapChain.hh>

namespace Mikoto {

    VulkanSwapChain::VulkanSwapChain( const VulkanSwapChainCreateInfo& createInfo )
        :   m_Extent{ createInfo.Extent },
            m_OldSwapChain{ createInfo.OldSwapChain },
            m_Surface{ createInfo.Surface },
            m_IsVsyncEnabled{ createInfo.EnableVsync }
    {

    }

    auto VulkanSwapChain::Init() -> void {
        if (m_Surface == nullptr) {
            MKT_THROW_RUNTIME_ERROR( "VulkanSwapChain::Init - Error the surface for the swapchain is null." );
        }

        /**
         * [00:11:36] CORE LOG [thread 10211] Validation layer: Validation Error: [ VUID-VkSwapchainCreateInfoKHR-imageExtent-01274 ] Object 0:
         * handle = 0x62e000018450, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x7cd0911d | vkCreateSwapchainKHR() called with imageExtent = (1494,921),
         * which is outside the bounds returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR(): currentExtent = (1495,925), minImageExtent = (1495,925),
         * maxImageExtent = (1495,925). The Vulkan spec states: imageExtent must be between minImageExtent and maxImageExtent, inclusive, where
         * minImageExtent and maxImageExtent are members of the VkSurfaceCapabilitiesKHR structure returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR
         * for the surface (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkSwapchainCreateInfoKHR-imageExtent-01274)
         *
         * this validation error is triggered at times when resizing the main window (GLFW window)
         * */
        CreateSwapChain();

        AcquireSwapchainImages();
    }

    auto VulkanSwapChain::CreateSwapChain() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        const auto [Capabilities, Formats, PresentModes] {
            VulkanHelpers::GetSwapChainSupport(device.GetPhysicalDevice(), *m_Surface)
        };

        const auto [format, colorSpace]{ ChooseSurfaceFormat(Formats) };
        const VkPresentModeKHR presentMode{ ChoosePresentMode(PresentModes) };
        const VkExtent2D extent{ ChooseExtent(Capabilities) };

        // Save for later use
        m_SwapchainInfo.Format = format;
        m_SwapchainInfo.PresentMode = presentMode;

        /**
         * We may sometimes have to wait on the driver to complete internal operations
         * before we can acquire another image to render to. Therefore, it is recommended
         * to request at least one more image, hence why we add 1. Likely the image count
         * results in the maximum swap chain image count so we do the check and clamp the resulting image count
         * */
        UInt32_T imageCount{ Capabilities.minImageCount + 1 };
        if (Capabilities.maxImageCount > 0 && imageCount > Capabilities.maxImageCount) {
            imageCount = Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{ VulkanHelpers::Initializers::SwapchainCreateInfoKHR() };
        createInfo.surface = *m_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = format;
        createInfo.imageColorSpace = colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;

        // Only the GUI is directly rendering to the swapchain images at the moment.
        // Generally, the renderer is drawing to a texture which can then be copied to a
        // swap chain image ready for render and then be presented
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const auto& [Present, Graphics] {
            device.GetLogicalDeviceQueues()
        };

        // Let swapchain to share images between queues or not. We need to account for it
        // in the case the present queue and the graphics queue are not actually the same
        const std::array queueFamilyIndices{ Graphics->FamilyIndex, Present->FamilyIndex };
        if (Graphics->FamilyIndex != Present->FamilyIndex) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = queueFamilyIndices.size();
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = Capabilities.currentTransform; // Image transform ot perform on swapchain images
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Handle blending, just draw as it is (perform no blending)
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE; // TODO: pass old swapchain (need debug currently old swapchain becoming retired which can't be passed here)

        if (vkCreateSwapchainKHR(device.GetLogicalDevice(), std::addressof( createInfo ), nullptr, std::addressof( m_Swapchain ) ) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("VulkanSwapChain::CreateSwapChain - Failed to create swap chain.");
        }
    }

    auto VulkanSwapChain::CreateSwapchainImageViewCreateInfo( const VkImage& image, const VkFormat& format ) -> VkImageViewCreateInfo {
        VkImageViewCreateInfo createInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.baseMipLevel = 0;

        return createInfo;
    }

    auto VulkanSwapChain::AcquireSwapchainImages() -> void {
        static VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        UInt32_T imageCount{};

        // We only specified a minimum number of images in the swap chain, even though the implementation is
        // allowed to create a swap chain with more. That's why we'll first query the final number of
        // images with vkGetSwapchainImagesKHR with the last parameter as nullptr, then resize the container and finally call it again to
        // retrieve the handles.
        vkGetSwapchainImagesKHR(device.GetLogicalDevice(), m_Swapchain, std::addressof( imageCount ), nullptr);

        auto images{ std::vector<VkImage>(imageCount) };
        vkGetSwapchainImagesKHR(device.GetLogicalDevice(), m_Swapchain, std::addressof( imageCount ), images.data());

        for (const VkImage& image : images) {
            VulkanImageCreateInfo createInfo{
                .Image{ image },
                .ImageViewCreateInfo{ CreateSwapchainImageViewCreateInfo(image, m_SwapchainInfo.Format) },
            };

            m_Images.emplace_back( VulkanImage::Create( createInfo ) );
        }
    }

    auto VulkanSwapChain::GetNextRenderableImage( UInt32_T &imageIndex, const VkFence fence, const VkSemaphore imageAvailable ) const -> VkResult {
        static VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        // For simplicity, parenthesize std::numeric_limits<std::uint64_t>::max because windows has a macro literally called max that causes conflicts
        vkWaitForFences( device.GetLogicalDevice(), 1, std::addressof( fence ), VK_TRUE, ( std::numeric_limits<std::uint64_t>::max )() );
        vkResetFences( device.GetLogicalDevice(), 1, std::addressof( fence ) );

        return vkAcquireNextImageKHR( device.GetLogicalDevice(), m_Swapchain, ( std::numeric_limits<UInt64_T>::max )(), imageAvailable, VK_NULL_HANDLE, std::addressof( imageIndex ) );
    }

    auto VulkanSwapChain::Present( const UInt32_T imageIndex, const VkSemaphore& renderFinished ) -> VkResult {
        static VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        const std::array swapChains{ m_Swapchain };
        const std::array signalSemaphores{ renderFinished };

        VkPresentInfoKHR presentInfo{ VulkanHelpers::Initializers::PresentInfoKHR() };

        presentInfo.swapchainCount = swapChains.size();
        presentInfo.pSwapchains = swapChains.data();

        presentInfo.pImageIndices = &imageIndex;

        // specifies the semaphores to wait for before issuing the present request.
        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();

        // Only the GUI is directly rendering to the swapchain images at the moment.
        // Generally, the renderer is drawing to a texture which can then be copied to a
        // swap chain image ready for render and then be presented
        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        const auto& [Present, Graphics]{ device.GetLogicalDeviceQueues() };
        VkQueue presentQueue{ Graphics->Queue };

        // Present and graphics queue may have same index
        if (Present.has_value() && Present->Queue != VK_NULL_HANDLE) {
            presentQueue = Present->Queue;
        }

        return vkQueuePresentKHR(presentQueue, std::addressof( presentInfo ) );
    }

    auto VulkanSwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) -> VkSurfaceFormatKHR {
        // NOTE: if we only have one format, and it is VK_FORMAT_UNDEFINED it means the surface supports all formats
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    auto VulkanSwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const -> VkPresentModeKHR {
        if (m_IsVsyncEnabled) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR || availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return availablePresentMode;
            }
        }

        // We return this one because if we do not find the present mode we are looking for,
        // VK_PRESENT_MODE_FIFO_KHR is guaranteed to be always available
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    auto VulkanSwapChain::ChooseExtent( const VkSurfaceCapabilitiesKHR& capabilities ) const -> VkExtent2D {
        // Determine if the currentExtent is set to a special value indicating that the surface size is undefined.
        // This special value is (std::numeric_limits<UInt32_T>::max)(). If the currentExtent width is equal to
        // this value, it means the surface size can be defined by the application, otherwise, the currentExtent
        // provided by the surface capabilities should be used.

        // If capabilities.currentExtent.width is not equal to the maximum unsigned integer, it means the surface
        // size is defined, and you should use currentExtent.
        // If it is equal to the maximum unsigned integer, you need to define the extent yourself within the bounds
        // of minImageExtent and maxImageExtent.

        if ( capabilities.currentExtent.width != ( std::numeric_limits<UInt32_T>::max )() ) {
            return capabilities.currentExtent;
        }

        const VkExtent2D actualExtent{
            .width{ ( std::max )( capabilities.minImageExtent.width, std::min( capabilities.maxImageExtent.width, m_Extent.width ) ) },
            .height{ ( std::max )( capabilities.minImageExtent.height, std::min( capabilities.maxImageExtent.height, m_Extent.height ) ) },
        };

        return actualExtent;
    }

    VulkanSwapChain::~VulkanSwapChain() {
        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }

    auto VulkanSwapChain::Release() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        // Wait on outstanding queue operations because there might be some objects still in use by the GPU
        device.WaitIdle();

        m_Surface = nullptr;

        m_Images.clear();

        // Destroy handles
        // The device is owned by the context and is destroyed before the instance and after any object is
        // created from it has finished being used
        vkDestroySwapchainKHR( device.GetLogicalDevice(), m_Swapchain, nullptr );
    }
}// namespace Mikoto