/**
 * ImGuiVulkanBackend.cc
 * Created by kate on 9/14/23.
 * */

// C++ Standard Library
#include <any>
#include <array>

// Third-Party Libraries
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

// Important to include after imgui
#include "ImGuizmo.h"

// Project Headers
#include <vulkan/vulkan_core.h>

#include <Common/VulkanUtils.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>

#include "Core/Logger.hh"
#include "GUI/ImGuiVulkanBackend.hh"
#include "Renderer/Vulkan/VulkanContext.hh"

namespace Mikoto {
    auto ImGuiVulkanBackend::Init(std::any windowHandle) -> void {
        // At the moment we are using Vulkan with GLFW windows
        GLFWwindow* window{ nullptr };

        try {
            window = std::any_cast<GLFWwindow*>(windowHandle);
        } catch (const std::bad_any_cast& e) {
            MKT_CORE_LOGGER_ERROR("Failed on any cast std::any windowHandle to GLFWwindow* for ImGuiVulkanBackend::Init. what(): {}", e.what());
            return;
        }

        PrepareForRender();

        std::array<VkDescriptorPoolSize, 11> poolSizes{};

        poolSizes[0] = { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 };
        poolSizes[1] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 };
        poolSizes[2] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 };
        poolSizes[3] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 };
        poolSizes[4] = { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 };
        poolSizes[5] = { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 };
        poolSizes[6] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 };
        poolSizes[7] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 };
        poolSizes[8] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 };
        poolSizes[9] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 };
        poolSizes[10] = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 };

        VkDescriptorPoolCreateInfo poolCreateInfo{ VulkanUtils::Initializers::DescriptorPoolCreateInfo() };
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = 1000;
        poolCreateInfo.poolSizeCount = static_cast<UInt32_T>(poolSizes.size());
        poolCreateInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), &poolCreateInfo, nullptr, &m_ImGuiDescriptorPool) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create descriptor pool for ImGui!");
        }

        DeletionQueue::Push([descriptorPool = m_ImGuiDescriptorPool]() -> void {
            vkDestroyDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), descriptorPool, nullptr);
        });

        ImGui_ImplVulkan_LoadFunctions(
                [](const char *functionName, void *vulkanInstance) {
                    return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkanInstance)), functionName);
                }, std::addressof(VulkanContext::GetInstance()));

        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = VulkanContext::GetInstance();
        initInfo.PhysicalDevice = VulkanContext::GetPrimaryPhysicalDevice();
        initInfo.Device = VulkanContext::GetPrimaryLogicalDevice();
        initInfo.Queue = VulkanContext::GetPrimaryLogicalDeviceQueuesData().GraphicsQueue;
        initInfo.DescriptorPool = m_ImGuiDescriptorPool;
        initInfo.MinImageCount = 3;
        initInfo.ImageCount = 3;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        if (!ImGui_ImplVulkan_Init(&initInfo, m_ImGuiRenderPass)) {
            MKT_THROW_RUNTIME_ERROR("Failed to initialize Vulkan for ImGui");
        }

        // execute a gpu command to upload imgui font textures
        VulkanContext::ImmediateSubmit(ImGui_ImplVulkan_CreateFontsTexture);

        // clear font textures from gpu
        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    auto ImGuiVulkanBackend::Shutdown() -> void {
        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

        ImGui_ImplVulkan_Shutdown();
    }

    auto ImGuiVulkanBackend::BeginFrame() -> void {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiVulkanBackend::EndFrame() -> void {
        ImGuiIO& io{ ImGui::GetIO()};

        ImGui::Render();
        BuildCommandBuffers();

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    auto ImGuiVulkanBackend::RecordImGuiCommandBuffers( VkCommandBuffer cmd ) -> void {
        // Begin ImGui-specific render pass
        VkRenderPassBeginInfo renderPassInfo{ VulkanUtils::Initializers::RenderPassBeginInfo() };
        renderPassInfo.renderPass = m_ImGuiRenderPass; // Use the render pass for ImGui
        renderPassInfo.framebuffer = m_DrawFrameBuffer.Get();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_Extent2D;

        std::array<VkClearValue, 2> clearValues{};                                  // Only one clear value for the color attachment
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};   // Clear color for ImGui
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<UInt32_T>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        renderPassInfo.clearValueCount = static_cast<UInt32_T>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Set Viewport and Scissor
        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(VulkanContext::GetSwapChain()->GetSwapChainExtent().width),
            .height = static_cast<float>(VulkanContext::GetSwapChain()->GetSwapChainExtent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor{
            .offset{ 0, 0 },
            .extent{ VulkanContext::GetSwapChain()->GetSwapChainExtent() },
        };

        vkCmdSetViewport(cmd, 0, 1, std::addressof(viewport));
        vkCmdSetScissor(cmd, 0, 1, std::addressof(scissor));

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        // End ImGui-specific render pass
        vkCmdEndRenderPass(cmd);
    }

    auto ImGuiVulkanBackend::BuildCommandBuffers() -> void {
        UInt32_T swapChainImageIndex{ VulkanContext::GetCurrentImageIndex() };

        auto& cmdHandles{ VulkanContext::GetDrawCommandBuffersHandles() };

        // Record draw commands
        DrawImGui(cmdHandles[swapChainImageIndex], VulkanContext::GetSwapChain()->GetCurrentImage( swapChainImageIndex ));

        // Submit commands for execution
        VulkanContext::BatchCommandBuffer(cmdHandles[swapChainImageIndex]);
    }

    auto ImGuiVulkanBackend::PrepareForRender() -> void {
        m_ColorAttachmentFormat = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

        m_DepthAttachmentFormat = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        CreateImages();
        CreateRenderPass();
        CreateFrameBuffer();
    }

    auto ImGuiVulkanBackend::CreateRenderPass() -> void {
        // Color Attachment
        VkAttachmentDescription colorAttachmentDesc{};
        colorAttachmentDesc.format = m_ColorAttachmentFormat;
        colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth Attachment
        VkAttachmentDescription depthAttachmentDesc{};
        depthAttachmentDesc.flags = 0;
        depthAttachmentDesc.format = m_DepthAttachmentFormat;
        depthAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency colorAttachmentDependency{};
        colorAttachmentDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorAttachmentDependency.dstSubpass = 0;
        colorAttachmentDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorAttachmentDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorAttachmentDependency.srcAccessMask = 0;
        colorAttachmentDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Add a new subpass dependency that synchronizes accesses to depth attachments.
        // This dependency tells Vulkan that the depth attachment in a renderpass cannot be used before previous render-passes have finished using it.
        VkSubpassDependency deptAttachmentDependency{};
        deptAttachmentDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        deptAttachmentDependency.dstSubpass = 0;
        deptAttachmentDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deptAttachmentDependency.srcAccessMask = 0;
        deptAttachmentDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deptAttachmentDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;


        std::array<VkSubpassDependency, 2> attachmentDependencies{ colorAttachmentDependency, deptAttachmentDependency };
        std::array<VkAttachmentDescription, 2> attachmentDescriptions{ colorAttachmentDesc, depthAttachmentDesc };

        VkRenderPassCreateInfo info{ VulkanUtils::Initializers::RenderPassCreateInfo() };
        info.attachmentCount = static_cast<UInt32_T>(attachmentDescriptions.size());
        info.pAttachments = attachmentDescriptions.data();

        info.dependencyCount = static_cast<UInt32_T>(attachmentDependencies.size());
        info.pDependencies = attachmentDependencies.data();

        info.subpassCount = 1;
        info.pSubpasses = &subpass;

        if (vkCreateRenderPass(VulkanContext::GetPrimaryLogicalDevice(), &info, nullptr, std::addressof(m_ImGuiRenderPass)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create render pass for the Vulkan Renderer!");
        }

        DeletionQueue::Push([renderPass = m_ImGuiRenderPass]() -> void {
            vkDestroyRenderPass(VulkanContext::GetPrimaryLogicalDevice(), renderPass, nullptr);
        });
    }

    auto ImGuiVulkanBackend::CreateFrameBuffer() -> void {
        std::array<VkImageView, 2> attachments{ m_ColorImage.GetView(), m_DepthImage.GetView() };

        VkFramebufferCreateInfo createInfo{ VulkanUtils::Initializers::FramebufferCreateInfo() };
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_ImGuiRenderPass;

        createInfo.width = m_Extent2D.width;
        createInfo.height = m_Extent2D.height;
        createInfo.layers = 1;

        createInfo.attachmentCount = static_cast<UInt32_T>( attachments.size() );
        createInfo.pAttachments = attachments.data();

        m_DrawFrameBuffer.OnCreate( createInfo );
    }

    auto ImGuiVulkanBackend::CreateImages() -> void {
        // Color Buffer attachment
        VkImageCreateInfo colorAttachmentCreateInfo{ VulkanUtils::Initializers::ImageCreateInfo() };
        colorAttachmentCreateInfo.pNext = nullptr;
        colorAttachmentCreateInfo.flags = 0;
        colorAttachmentCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        colorAttachmentCreateInfo.format = m_ColorAttachmentFormat;
        colorAttachmentCreateInfo.extent.width = m_Extent2D.width;
        colorAttachmentCreateInfo.extent.height = m_Extent2D.height;
        colorAttachmentCreateInfo.extent.depth = 1;
        colorAttachmentCreateInfo.mipLevels = 1;
        colorAttachmentCreateInfo.arrayLayers = 1;
        colorAttachmentCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        colorAttachmentCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkImageViewCreateInfo colorAttachmentViewCreateInfo{ VulkanUtils::Initializers::ImageViewCreateInfo() };
        colorAttachmentViewCreateInfo.pNext = nullptr;
        colorAttachmentViewCreateInfo.flags = 0;
        colorAttachmentViewCreateInfo.image = VK_NULL_HANDLE; // Set by OnCreate()
        colorAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentViewCreateInfo.format = colorAttachmentCreateInfo.format; // match formats for simplicity

        colorAttachmentViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        colorAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        colorAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        colorAttachmentViewCreateInfo.subresourceRange.layerCount = 1;

        m_ColorImage.OnCreate({ colorAttachmentCreateInfo , colorAttachmentViewCreateInfo });

        // Depth attachment
        VkImageCreateInfo depthAttachmentCreateInfo{ VulkanUtils::Initializers::ImageCreateInfo() };
        depthAttachmentCreateInfo.pNext = nullptr;
        depthAttachmentCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        depthAttachmentCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        depthAttachmentCreateInfo.extent.width = m_Extent2D.width;
        depthAttachmentCreateInfo.extent.height = m_Extent2D.height;
        depthAttachmentCreateInfo.extent.depth = 1;
        depthAttachmentCreateInfo.format = m_DepthAttachmentFormat;

        depthAttachmentCreateInfo.mipLevels = 1;
        depthAttachmentCreateInfo.arrayLayers = 1;
        depthAttachmentCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

        VkImageViewCreateInfo depthAttachmentViewCreateInfo{ VulkanUtils::Initializers::ImageViewCreateInfo() };

        depthAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthAttachmentViewCreateInfo.image = VK_NULL_HANDLE; // Set by OnCreate()
        depthAttachmentViewCreateInfo.format = depthAttachmentCreateInfo.format;

        depthAttachmentViewCreateInfo.flags = 0;
        depthAttachmentViewCreateInfo.subresourceRange = {};
        depthAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        depthAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        depthAttachmentViewCreateInfo.subresourceRange.layerCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.aspectMask =
                depthAttachmentCreateInfo.format < VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_DEPTH_BIT : (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

        m_DepthImage.OnCreate({ depthAttachmentCreateInfo , depthAttachmentViewCreateInfo });
    }

    auto ImGuiVulkanBackend::DrawImGui( VkCommandBuffer cmd, VkImage currentSwapChainImage ) -> void {
        // Begin recording command buffer
        VkCommandBufferBeginInfo beginInfo{ VulkanUtils::Initializers::CommandBufferBeginInfo() };

        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to begin recording ImGui command buffer");
        }

        // Record imgui draw commands
        RecordImGuiCommandBuffers( cmd );


        // the transition the draw image and the swapchain image into their correct transfer layouts
        VulkanUtils::TransitionImage(cmd, m_ColorImage.Get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VulkanUtils::TransitionImage(cmd, currentSwapChainImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkExtent3D extent{};
        extent.height = VulkanContext::GetSwapChain()->GetSwapChainExtent().height;
        extent.width = VulkanContext::GetSwapChain()->GetSwapChainExtent().width;
        extent.depth = 1;

        // execute a copy from the draw image into the swapchain
        VulkanUtils::CopyImageToImage(cmd, m_ColorImage.Get(), currentSwapChainImage, extent);

        // convert color image to original layout
        VulkanUtils::TransitionImage(cmd, m_ColorImage.Get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // set swapchain image layout to Present, so we can show it on the screen
        VulkanUtils::TransitionImage(cmd, currentSwapChainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        // End recording command buffer
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to record ImGui command buffer!");
        }
    }
}