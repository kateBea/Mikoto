/**
 * ImGuiVulkanBackend.cc
 * Created by kate on 9/14/23.
 * */

// C++ Standard Library
#include <any>
#include <array>

// Third-Party Libraries
#include "GLFW/glfw3.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

// Important to include after imgui
#include "ImGuizmo.h"

// Project Headers
#include "../Common/VulkanUtils.hh"
#include "Core/Application.hh"
#include "Core/Logger.hh"
#include "GUI/ImGuiVulkanBackend.hh"
#include <Renderer/Vulkan/DeletionQueue.hh>
#include "Renderer/Vulkan/VulkanContext.hh"

namespace Mikoto {
    auto ImGuiVulkanBackend::Init(std::any windowHandle) -> void {
        // At the moment we are using Vulkan with GLFW windows
        GLFWwindow* window{ nullptr };
        try {
            window = std::any_cast<GLFWwindow*>(windowHandle);
        }
        catch (std::bad_any_cast& e) {
            MKT_CORE_LOGGER_ERROR("Failed on any cast std::any windowHandle to GLFWwindow* for ImGuiVulkanBackend::Init");
            return;
        }

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

        if (!ImGui_ImplVulkan_Init(&initInfo, VulkanContext::GetSwapChain()->GetRenderPass()/*m_ImGuiRenderPass*/)) {
            MKT_THROW_RUNTIME_ERROR("Failed to initialize Vulkan for ImGui");
        }

        // execute a gpu command to upload imgui font textures
        VulkanContext::ImmediateSubmit(ImGui_ImplVulkan_CreateFontsTexture);

        // clear font textures from cpu data
        // VulkanContext::ImmediateSubmit, by default, submits work to VulkanContext::GetPrimaryLogicalDevice()
        // see declaration, we wait on that queue before calling ImGui_ImplVulkan_DestroyFontUploadObjects
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

    auto ImGuiVulkanBackend::RecordImGuiCommandBuffers(UInt32_T imageIndex) -> void {
        auto& cmdHandles{ VulkanContext::GetDrawCommandBuffersHandles() };

        // Begin recording command buffer
        VkCommandBufferBeginInfo beginInfo{ VulkanUtils::Initializers::CommandBufferBeginInfo() };

        if (vkBeginCommandBuffer(cmdHandles[imageIndex], &beginInfo) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to begin recording ImGui command buffer");
        }

        // Begin ImGui-specific render pass
        VkRenderPassBeginInfo renderPassInfo{ VulkanUtils::Initializers::RenderPassBeginInfo() };
        renderPassInfo.renderPass = VulkanContext::GetSwapChain()->GetRenderPass(); /*m_ImGuiRenderPass*/ // Use the render pass for ImGui
        renderPassInfo.framebuffer = VulkanContext::GetSwapChain()->GetFrameBufferAtIndex(imageIndex);
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = VulkanContext::GetSwapChain()->GetSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};                                  // Only one clear value for the color attachment
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};   // Clear color for ImGui
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<UInt32_T>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        renderPassInfo.clearValueCount = static_cast<UInt32_T>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmdHandles[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        {
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

            vkCmdSetViewport(cmdHandles[imageIndex], 0, 1, &viewport);
            vkCmdSetScissor(cmdHandles[imageIndex], 0, 1, &scissor);
        }

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdHandles[imageIndex]);

        // End ImGui-specific render pass
        vkCmdEndRenderPass(cmdHandles[imageIndex]);

        // End recording command buffer
        if (vkEndCommandBuffer(cmdHandles[imageIndex]) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to record ImGui command buffer!");
        }
    }

    auto ImGuiVulkanBackend::BuildCommandBuffers() -> void {
        UInt32_T swapChainImageIndex{ VulkanContext::GetCurrentImageIndex() };
        auto& cmdHandles{ VulkanContext::GetDrawCommandBuffersHandles() };

        // Record draw commands
        RecordImGuiCommandBuffers(swapChainImageIndex);

        // Submit commands for execution
        VulkanContext::BatchCommandBuffer(cmdHandles[swapChainImageIndex]);
    }
}