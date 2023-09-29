/**
 * ImGuiVulkanBackend.cc
 * Created by kate on 9/14/23.
 * */

// C++ Standard Library
#include <any>
#include <array>

// Third-Party Libraries
#include <imgui.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

// Project Headers
#include <Utility/VulkanUtils.hh>
#include <Core/Logger.hh>
#include <Core/Application.hh>
#include <ImGui/ImGuiVulkanBackend.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

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

        if (vkCreateDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), &poolCreateInfo, nullptr, &m_ImGuiDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool for ImGui!");

        ImGui_ImplVulkan_LoadFunctions(
                [](const char *function_name, void *vulkan_instance) {
                    return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance *>(vulkan_instance)), function_name);
                }, &VulkanContext::GetInstance());

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

        CreateImGuiRenderPass();
        CreateImGuiCommandPool();
        CreateImGuiCommandBuffers();

        if (!ImGui_ImplVulkan_Init(&initInfo, VulkanContext::GetSwapChain()->GetRenderPass()/*m_ImGuiRenderPass*/))
            throw std::runtime_error("Failed to initialize Vulkan for ImGui");

        // execute a gpu command to upload imgui font textures
        auto command{ m_CommandPool->BeginSingleTimeCommands() };
        ImGui_ImplVulkan_CreateFontsTexture(command);
        m_CommandPool->EndSingleTimeCommands(command);

        // clear font textures from cpu data
        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    auto ImGuiVulkanBackend::ShutDown() -> void {
        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

        m_CommandPool->OnRelease();
        vkDestroyDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), m_ImGuiDescriptorPool, nullptr);

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
        Window& window{ Application::Get().GetMainWindow()};
        io.DisplaySize = ImVec2{(float) window.GetWidth(), (float) window.GetHeight() };

        ImGui::Render();
        BuildCommandBuffers();

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    auto ImGuiVulkanBackend::CreateImGuiRenderPass() -> void {
        VkAttachmentDescription attachment{};
        attachment.format = VulkanContext::GetSwapChain()->GetSwapChainImageFormat();
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachment{};
        colorAttachment.attachment = 0;
        colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachment;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;// or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        if (vkCreateRenderPass(VulkanContext::GetPrimaryLogicalDevice(), &info, nullptr, &m_ImGuiRenderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create render pass!");
    }

    auto ImGuiVulkanBackend::CreateImGuiCommandPool() -> void {
        m_CommandPool = std::make_shared<VulkanCommandPool>();
        MKT_ASSERT(m_CommandPool, "Command Pool pointer is NULL");
        m_CommandPool->OnCreate(VkCommandPoolCreateInfo() /* temporary to add more flexibility to command pool creation*/);
    }

    auto ImGuiVulkanBackend::CreateImGuiCommandBuffers() -> void {
        m_ImGuiCommandBuffers.resize(VulkanContext::GetSwapChain()->GetImageCount());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool->GetCommandPool();
        allocInfo.commandBufferCount = static_cast<UInt32_T>(m_ImGuiCommandBuffers.size());

        if (vkAllocateCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, m_ImGuiCommandBuffers.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate command buffers");
    }

    auto ImGuiVulkanBackend::AcquireNextSwapChainImage(UInt32_T& imageIndex) -> VkResult {
        return VulkanContext::GetSwapChain()->GetNextImage(&imageIndex);
    }

    auto ImGuiVulkanBackend::RecordImGuiCommandBuffers(UInt32_T imageIndex) -> void {
        // Begin recording command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(m_ImGuiCommandBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin recording ImGui command buffer");

        // Begin ImGui-specific render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = VulkanContext::GetSwapChain()->GetRenderPass(); /*m_ImGuiRenderPass*/ // Use the render pass for ImGui
        renderPassInfo.framebuffer = VulkanContext::GetSwapChain()->GetFrameBufferAtIndex(imageIndex);
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = VulkanContext::GetSwapChain()->GetSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};                                  // Only one clear value for the color attachment
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};   // Clear color for ImGui
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<UInt32_T>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        renderPassInfo.clearValueCount = static_cast<UInt32_T>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(m_ImGuiCommandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

            vkCmdSetViewport(m_ImGuiCommandBuffers[imageIndex], 0, 1, &viewport);
            vkCmdSetScissor(m_ImGuiCommandBuffers[imageIndex], 0, 1, &scissor);
        }

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_ImGuiCommandBuffers[imageIndex]);

        // End ImGui-specific render pass
        vkCmdEndRenderPass(m_ImGuiCommandBuffers[imageIndex]);

        // End recording command buffer
        if (vkEndCommandBuffer(m_ImGuiCommandBuffers[imageIndex]) != VK_SUCCESS)
            throw std::runtime_error("Failed to record ImGui command buffer");
    }

    auto ImGuiVulkanBackend::SubmitImGuiCommandBuffers(UInt32_T& imageIndex) -> VkResult {
        return VulkanContext::GetSwapChain()->SubmitCommandBuffers(&m_ImGuiCommandBuffers[imageIndex], imageIndex);
    }

    auto ImGuiVulkanBackend::BuildCommandBuffers() -> void {
        UInt32_T swapChainImageIndex{};
        auto ret{ AcquireNextSwapChainImage(swapChainImageIndex) };

        if (ret == VK_ERROR_OUT_OF_DATE_KHR) {
            VulkanSwapChainCreateInfo createInfo{ VulkanContext::GetSwapChain()->GetSwapChainCreateInfo() };
            VulkanContext::RecreateSwapChain(std::move(createInfo));
            return;
        }

        if (ret != VK_SUCCESS)
            throw std::runtime_error("failed to acquire swap chain image!");

        RecordImGuiCommandBuffers(swapChainImageIndex);
        ret = SubmitImGuiCommandBuffers(swapChainImageIndex);
        if (ret == VK_ERROR_OUT_OF_DATE_KHR || ret == VK_SUBOPTIMAL_KHR) {
            VulkanSwapChainCreateInfo createInfo{ VulkanContext::GetSwapChain()->GetSwapChainCreateInfo() };
            VulkanContext::RecreateSwapChain(std::move(createInfo));
            return;
        }

        if (ret != VK_SUCCESS)
            throw std::runtime_error("Failed to submit imgui command buffers!");
    }

}