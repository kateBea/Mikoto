/**
 * VulkanSwapChain.cc
 * Created by kate on 12/7/23.
 * */

// C++ Standard Library
#include <limits>
#include <stdexcept>
#include <utility>
#include <array>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Common/VulkanUtils.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanSwapChain.hh>

namespace Mikoto {
    auto VulkanSwapChain::GetNextImageIndex(UInt32_T &imageIndex, VkFence fence, VkSemaphore imageAvailable) -> VkResult {
        //if (fence == VK_NULL_HANDLE) { fence = m_InFlightFences[m_CurrentFrame]; }
        // if (imageAvailable == VK_NULL_HANDLE) { imageAvailable = m_ImageAvailableSemaphores[m_CurrentFrame]; }

        vkWaitForFences(VulkanContext::GetPrimaryLogicalDevice(), 1, &fence, VK_TRUE, std::numeric_limits<UInt64_T>::max());
        vkResetFences(VulkanContext::GetPrimaryLogicalDevice(), 1, &fence);

        return vkAcquireNextImageKHR(VulkanContext::GetPrimaryLogicalDevice(), m_SwapChain, std::numeric_limits<UInt32_T>::max(), imageAvailable, VK_NULL_HANDLE, std::addressof(imageIndex));
    }


    auto VulkanSwapChain::OnCreate(VulkanSwapChainCreateInfo&& createInfo) -> void {
        m_SwapChainDetails = std::move(createInfo);
        m_WindowExtent = m_SwapChainDetails.Extent;

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
    }


    auto VulkanSwapChain::Present(UInt32_T imageIndex, VkSemaphore renderFinished) -> VkResult {
        std::array<VkSwapchainKHR, 1> swapChains{ m_SwapChain };
        std::array<VkSemaphore, 1> signalSemaphores{ renderFinished };

        VkPresentInfoKHR presentInfo{ VulkanUtils::Initializers::PresentInfoKHR() };

        presentInfo.swapchainCount = swapChains.size();
        presentInfo.pSwapchains = swapChains.data();

        presentInfo.pImageIndices = &imageIndex;

        // specifies the semaphores to wait for before issuing the present request.
        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();

        m_CurrentFrame = (m_CurrentFrame + EXTRA_IMAGE_REQUEST) % MAX_FRAMES_IN_FLIGHT;

        return vkQueuePresentKHR(VulkanContext::GetPrimaryLogicalDevicePresentQueue(), &presentInfo);
    }


    auto VulkanSwapChain::CreateSwapChain() -> void {
        auto swapChainSupport{ VulkanContext::GetPrimaryLogicalDeviceSwapChainSupport() };

        const VkSurfaceFormatKHR surfaceFormat{ ChooseSwapSurfaceFormat(swapChainSupport.Formats) };
        const VkPresentModeKHR presentMode{ ChooseSwapPresentMode(swapChainSupport.PresentModes) };
        const VkExtent2D extent{ ChooseSwapExtent(swapChainSupport.Capabilities) };

        UInt32_T imageCount{ swapChainSupport.Capabilities.minImageCount + EXTRA_IMAGE_REQUEST };

        if (swapChainSupport.Capabilities.maxImageCount > 0 && imageCount > swapChainSupport.Capabilities.maxImageCount)
            imageCount = swapChainSupport.Capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{ VulkanUtils::Initializers::SwapchainCreateInfoKHR() };
        createInfo.surface = VulkanContext::GetSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // we want to copy rendered images to this one (we don't render directly to the swapchain images)

        auto indices{ VulkanContext::GetPrimaryLogicalDeviceQueuesData() };
        std::array<UInt32_T, 2> queueFamilyIndices{ indices.GraphicsFamilyIndex, indices.PresentFamilyIndex };

        if (indices.GraphicsFamilyIndex != indices.PresentFamilyIndex) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = queueFamilyIndices.size();
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE; // pass old swapchain (needs debug current old swapchain becoming retired which can't be passed here)

        m_SwapChainExtent = extent;

        if (vkCreateSwapchainKHR(VulkanContext::GetPrimaryLogicalDevice(), &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create swap chain!");
        }

        AcquireSwapchainImages();
    }


    auto VulkanSwapChain::AcquireSwapchainImages() -> void {
        UInt32_T imageCount{};

        // We only specified a minimum number of images in the swap chain, even though the implementation is
        // allowed to create a swap chain with more. That's why we'll first query the final number of
        // images with vkGetSwapchainImagesKHR with the last parameter as nullptr, then resize the container and finally call it again to
        // retrieve the handles.
        vkGetSwapchainImagesKHR(VulkanContext::GetPrimaryLogicalDevice(), m_SwapChain, &imageCount, nullptr);

        m_SwapChainImages = std::vector<VkImage>(imageCount);
        vkGetSwapchainImagesKHR(VulkanContext::GetPrimaryLogicalDevice(), m_SwapChain, &imageCount, m_SwapChainImages.data());
    }


    auto VulkanSwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) -> VkSurfaceFormatKHR {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }


    auto VulkanSwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const -> VkPresentModeKHR {
        if (m_SwapChainDetails.VSyncEnable)
            return VK_PRESENT_MODE_FIFO_KHR;
        else {
            for (const auto& availablePresentMode : availablePresentModes) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR || availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    return availablePresentMode;
                }
            }

            return VK_PRESENT_MODE_FIFO_KHR;
        }
    }


    auto VulkanSwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) -> VkExtent2D {
        if (capabilities.currentExtent.width != std::numeric_limits<UInt32_T>::max()) {
            return capabilities.currentExtent;
        }
        else {
            VkExtent2D actualExtent{ m_WindowExtent };
            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }


    auto VulkanSwapChain::OnRelease() -> void {
        // Wait on outstanding queue operations because there might be some objects still in use by the GPU
        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

        // Destroy handles
        vkDestroySwapchainKHR(VulkanContext::GetPrimaryLogicalDevice(), m_SwapChain, nullptr);
    }


    auto VulkanSwapChain::GetDefaultCreateInfo() -> VulkanSwapChainCreateInfo {
        const auto windowExtent{ std::make_pair(1920, 1080) };

        VulkanSwapChainCreateInfo createInfo{
                .OldSwapChain = VK_NULL_HANDLE,
                .Extent = { (UInt32_T)windowExtent.first, (UInt32_T)windowExtent.second },
                .VSyncEnable = false,
        };

        return createInfo;
    }
}