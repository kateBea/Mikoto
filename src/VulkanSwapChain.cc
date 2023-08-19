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
#include <Utility/Common.hh>
#include <Core/Application.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanSwapChain.hh>

namespace Mikoto {
    VulkanSwapChain::VulkanSwapChain(const VulkanSwapChainCreateInfo& createInfo)
        : m_SwapChainDetails{ createInfo }, m_WindowExtent{ createInfo.Extent }
    {
        OnCreate();
    }

    auto VulkanSwapChain::OnCreate() -> void {
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
        CreateImageViews();
        CreateRenderPass();
        CreateDepthResources();
        CreateFrameBuffers();
        CreateSyncObjects();
    }

    auto VulkanSwapChain::GetNextImage(UInt32_T* imageIndex) -> VkResult {
        vkWaitForFences(VulkanContext::GetPrimaryLogicalDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, std::numeric_limits<UInt64_T>::max());
        // m_ImageAvailableSemaphores[m_CurrentFrame] must be a not signaled semaphore
        VkResult result{ vkAcquireNextImageKHR(VulkanContext::GetPrimaryLogicalDevice(), m_SwapChain, std::numeric_limits<UInt32_T>::max(), m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, imageIndex) };
        return result;
    }

    auto VulkanSwapChain::OnCreate(const VulkanSwapChainCreateInfo& createInfo) -> void {
        // TODO: move ctor functionality here for more initialization control
    }

    auto VulkanSwapChain::SubmitCommandBuffers(const VkCommandBuffer* buffers, const UInt32_T imageIndex) -> VkResult {
        // TODO: This should most likely be part of the present function from the VulkanContext
        if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE)
            vkWaitForFences(VulkanContext::GetPrimaryLogicalDevice(), 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

        m_ImagesInFlight[imageIndex] = m_InFlightFences[m_CurrentFrame];

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        std::array<VkSemaphore, 1> waitSemaphores{ m_ImageAvailableSemaphores[m_CurrentFrame] };
        std::array<VkPipelineStageFlags, 1> waitStages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = waitSemaphores.size();
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = buffers;

        std::array<VkSemaphore, 1> signalSemaphores{ m_RenderFinishedSemaphores[m_CurrentFrame] };
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores = signalSemaphores.data();

        vkResetFences(VulkanContext::GetPrimaryLogicalDevice(), 1, &m_InFlightFences[m_CurrentFrame]);
        if (vkQueueSubmit(VulkanContext::GetPrimaryLogicalDeviceGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS)
            throw std::runtime_error("failed to submit draw command buffer!");

        // Prepare presentation
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();

        std::array<VkSwapchainKHR, 1> swapChains{ m_SwapChain };
        presentInfo.swapchainCount = swapChains.size();
        presentInfo.pSwapchains = swapChains.data();
        presentInfo.pImageIndices = &imageIndex;

        auto result{ vkQueuePresentKHR(VulkanContext::GetPrimaryLogicalDevicePresentQueue(), &presentInfo) };



        m_CurrentFrame = (m_CurrentFrame + EXTRA_IMAGE_REQUEST) % MAX_FRAMES_IN_FLIGHT;

        return result;
    }

    auto VulkanSwapChain::CreateSwapChain() -> void {
        auto swapChainSupport{ VulkanContext::GetPrimaryLogicalDeviceSwapChainSupport() };

        VkSurfaceFormatKHR surfaceFormat{ ChooseSwapSurfaceFormat(swapChainSupport.Formats) };
        VkPresentModeKHR presentMode{ ChooseSwapPresentMode(swapChainSupport.PresentModes) };
        VkExtent2D extent{ ChooseSwapExtent(swapChainSupport.Capabilities) };

        UInt32_T imageCount{ swapChainSupport.Capabilities.minImageCount + EXTRA_IMAGE_REQUEST };

        if (swapChainSupport.Capabilities.maxImageCount > 0 && imageCount > swapChainSupport.Capabilities.maxImageCount)
            imageCount = swapChainSupport.Capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = VulkanContext::GetSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        auto result{ vkCreateSwapchainKHR(VulkanContext::GetPrimaryLogicalDevice(), &createInfo, nullptr, &m_SwapChain) };

        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to create swap chain!");

        // We only specified a minimum number of images in the swap chain, so the implementation is
        // allowed to create a swap chain with more. That's why we'll first query the final number of
        // images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
        // retrieve the handles.
        vkGetSwapchainImagesKHR(VulkanContext::GetPrimaryLogicalDevice(), m_SwapChain, &imageCount, nullptr);
        m_SwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(VulkanContext::GetPrimaryLogicalDevice(), m_SwapChain, &imageCount, m_SwapChainImages.data());

        m_SwapChainImageFormat = surfaceFormat.format;
        m_SwapChainExtent = extent;
    }

    auto VulkanSwapChain::CreateImageViews() -> void {
        m_SwapChainImageViews.resize(m_SwapChainImages.size());
        for (Size_T i{}; i < m_SwapChainImages.size(); i++) {
            VkImageViewCreateInfo viewInfo{};

            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_SwapChainImages[i];

            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_SwapChainImageFormat;
            viewInfo.flags = 0;

            if (vkCreateImageView(VulkanContext::GetPrimaryLogicalDevice(), &viewInfo, nullptr, &m_SwapChainImageViews[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create texture image view!");

        }
    }

    auto VulkanSwapChain::CreateRenderPass() -> void {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = FindDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = GetSwapChainImageFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments{ colorAttachment, depthAttachment };

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<UInt32_T>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(VulkanContext::GetPrimaryLogicalDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass!");

    }

    auto VulkanSwapChain::CreateFrameBuffers() -> void {
        m_SwapChainFrameBuffers.resize(GetImageCount());
        for (Size_T i{}; i < GetImageCount(); i++) {
            std::array<VkImageView, 2> attachments{ m_SwapChainImageViews[i], m_DepthImageViews[i] };

            VkExtent2D swapChainExtent{ GetSwapChainExtent() };


            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = static_cast<UInt32_T>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(VulkanContext::GetPrimaryLogicalDevice(), &framebufferInfo, nullptr, &m_SwapChainFrameBuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create framebuffer!");

        }
    }

    auto VulkanSwapChain::CreateImageWithInfo(const VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) -> void {
        if (vkCreateImage(VulkanContext::GetPrimaryLogicalDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("failed to create image!");

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(VulkanContext::GetPrimaryLogicalDevice(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(memRequirements.memoryTypeBits, properties, VulkanContext::GetPrimaryPhysicalDevice());

        if (vkAllocateMemory(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory!");

        if (vkBindImageMemory(VulkanContext::GetPrimaryLogicalDevice(), image, imageMemory, 0) != VK_SUCCESS)
            throw std::runtime_error("failed to bind image memory!");
    }

    auto VulkanSwapChain::CreateDepthResources() -> void {
        VkFormat depthFormat = FindDepthFormat();
        VkExtent2D swapChainExtent = GetSwapChainExtent();

        m_DepthImages.resize(GetImageCount());
        m_DepthImageMemories.resize(GetImageCount());
        m_DepthImageViews.resize(GetImageCount());

        for (std::size_t i{}; i < m_DepthImages.size(); i++) {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = swapChainExtent.width;
            imageInfo.extent.height = swapChainExtent.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.format = depthFormat;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.flags = 0;

            CreateImageWithInfo(
                    imageInfo,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    m_DepthImages[i],
                    m_DepthImageMemories[i]);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_DepthImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = depthFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(VulkanContext::GetPrimaryLogicalDevice(), &viewInfo, nullptr, &m_DepthImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create texture image view!");
            }
        }
    }

    auto VulkanSwapChain::CreateSyncObjects() -> void {
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        m_ImagesInFlight.resize(GetImageCount(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::size_t i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(VulkanContext::GetPrimaryLogicalDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(VulkanContext::GetPrimaryLogicalDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(VulkanContext::GetPrimaryLogicalDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    auto VulkanSwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) -> VkSurfaceFormatKHR {
        for (const auto& availableFormat: availableFormats) {
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
            for (const auto &availablePresentMode: availablePresentModes) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return availablePresentMode;
                }
            }

            // if VK_PRESENT_MODE_MAILBOX_KHR is not supported
            return VK_PRESENT_MODE_FIFO_KHR;
        }
    }

    auto VulkanSwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) -> VkExtent2D {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            VkExtent2D actualExtent{ m_WindowExtent };
            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }

    auto VulkanSwapChain::FindDepthFormat() -> VkFormat {
        return VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    auto VulkanSwapChain::OnRelease() const -> void {
        vkDeviceWaitIdle(VulkanContext::GetPrimaryLogicalDevice());

        for (auto imageView: m_SwapChainImageViews)
            vkDestroyImageView(VulkanContext::GetPrimaryLogicalDevice(), imageView, nullptr);

        vkDestroySwapchainKHR(VulkanContext::GetPrimaryLogicalDevice(), m_SwapChain, nullptr);

        // Depth resources cleanup
        for (Size_T i{}; i < m_DepthImages.size(); i++) {
            vkDestroyImageView(VulkanContext::GetPrimaryLogicalDevice(), m_DepthImageViews[i], nullptr);
            vkDestroyImage(VulkanContext::GetPrimaryLogicalDevice(), m_DepthImages[i], nullptr);
            vkFreeMemory(VulkanContext::GetPrimaryLogicalDevice(), m_DepthImageMemories[i], nullptr);
        }

        for (auto framebuffer: m_SwapChainFrameBuffers)
            vkDestroyFramebuffer(VulkanContext::GetPrimaryLogicalDevice(), framebuffer, nullptr);

        vkDestroyRenderPass(VulkanContext::GetPrimaryLogicalDevice(), m_RenderPass, nullptr);

        // cleanup synchronization objects
        for (Size_T i{}; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(VulkanContext::GetPrimaryLogicalDevice(), m_RenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(VulkanContext::GetPrimaryLogicalDevice(), m_ImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(VulkanContext::GetPrimaryLogicalDevice(), m_InFlightFences[i], nullptr);
        }
    }
    auto VulkanSwapChain::GetDefaultCreateInfo() -> VulkanSwapChainCreateInfo {
        auto appWindowExtent{ Application::Get().GetMainWindowPtr()->GetExtent() };
        VkExtent2D extent{ (UInt32_T)appWindowExtent.first, (UInt32_T)appWindowExtent.second };
        VulkanSwapChainCreateInfo createInfo{
                .Extent = extent,
                .VSyncEnable = false,
        };
        return createInfo;
    }
}