/**
 * ImGuiVulkanBackend.cc
 * Created by kate on 9/14/23.
 * */

// C++ Standard Library
#include <any>
#include <array>

// Third-Party Libraries
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <volk.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

// Important to include after imgui
#include <ImGuizmo.h>

#include <Core/Logging/Logger.hh>
#include <GUI/ImGuiVulkanBackend.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace Mikoto {

    auto ImGuiVulkanBackend::Init( ) -> bool {
        try {
            InitializeImGuiRender();

            CreateImages();
            CreateRenderPass();
            CreateFrameBuffer();

            InitImGuiForVulkan();

            InitializeCommands();

            InitCommandBuffers();

        } catch ( const std::exception& e ) {
            MKT_CORE_LOGGER_ERROR( "ImGuiVulkanBackend::Init - Except: what(): {}", e.what() );
            return false;
        }

        return true;
    }

    auto ImGuiVulkanBackend::Shutdown() -> void {
        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        // Wait for remaining operations to complete
        device.WaitIdle();

        ImGui_ImplVulkan_Shutdown();
    }

    auto ImGuiVulkanBackend::BeginFrame() -> void {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiVulkanBackend::EndFrame() -> void {
        // If the swapchain has been resized we need to recreate the framebuffers and images
        VulkanSwapChain& swapChain{ VulkanContext::Get().GetSwapChain() };
        if ( swapChain.GetExtent().width != m_Extent2D.width ||
             swapChain.GetExtent().height != m_Extent2D.height )
        {
            m_Extent2D = swapChain.GetExtent();
            m_Extent3D = { m_Extent2D.width, m_Extent2D.height, 1 };

            CreateImages();
            CreateFrameBuffer();
        }

        ImGui::Render();

        const UInt32_T swapChainImageIndex{ VulkanContext::Get().GetCurrentRenderableImageIndex() };

        RecordCommands( m_DrawCommandBuffers[swapChainImageIndex], VulkanContext::Get().GetSwapChain().GetImage( swapChainImageIndex ) );

        VulkanDevice& device{ VulkanContext::Get().GetDevice() };
        device.RegisterCommand( m_DrawCommandBuffers[swapChainImageIndex]);

        ImGuiIO& io{ ImGui::GetIO() };
        if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    auto ImGuiVulkanBackend::InitImGuiForVulkan() -> void {
        GLFWwindow* window{ std::any_cast<GLFWwindow*>( m_Window->GetNativeWindow() ) };
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

        VkDescriptorPoolCreateInfo poolCreateInfo{ VulkanHelpers::Initializers::DescriptorPoolCreateInfo() };
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = 1000;
        poolCreateInfo.poolSizeCount = static_cast<UInt32_T>( poolSizes.size() );
        poolCreateInfo.pPoolSizes = poolSizes.data();

        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        if ( vkCreateDescriptorPool( device.GetLogicalDevice(),
                                     std::addressof( poolCreateInfo ),
                                     nullptr,
                                     std::addressof( m_ImGuiDescriptorPool ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "ImGuiVulkanBackend::Init - Failed to create descriptor pool for ImGui." );
        }

        VulkanDeletionQueue::Push( [descriptorPool = m_ImGuiDescriptorPool, device = device.GetLogicalDevice()]() -> void {
            vkDestroyDescriptorPool( device, descriptorPool, nullptr );
        } );

        // TODO: fetch api from somehwre else
        ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3,
                []( const char* functionName, void* vulkanInstance ) {
                    return vkGetInstanceProcAddr( *static_cast<VkInstance*>( vulkanInstance ), functionName );
                },
                std::addressof( VulkanContext::Get().GetInstance() ) );

        ImGui_ImplGlfw_InitForVulkan( window, true );

        ImGui_ImplVulkan_InitInfo initInfo{
            .Instance = VulkanContext::Get().GetInstance(),
            .PhysicalDevice = device.GetPhysicalDevice(),
            .Device = device.GetLogicalDevice(),
            .Queue = device.GetLogicalDeviceQueues().Graphics->Queue,
            .DescriptorPool = m_ImGuiDescriptorPool,
            .RenderPass = m_ImGuiRenderPass,
            .MinImageCount = 3,
            .ImageCount = 3,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT
        };


        if ( !ImGui_ImplVulkan_Init( std::addressof( initInfo ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "ImGuiVulkanBackend - Failed to initialize Vulkan for ImGui" );
        }

        if ( ImGui_ImplVulkan_CreateFontsTexture() ) {
            MKT_CORE_LOGGER_DEBUG( "Successfully created ImGui fonts!" );
        } else {
            MKT_THROW_RUNTIME_ERROR( "Error creating ImGui fonts!" );
        }
    }

    auto ImGuiVulkanBackend::PrepareMainRenderPass( const VkCommandBuffer cmd ) const -> void {
        // Begin ImGui-specific render pass
        VkRenderPassBeginInfo renderPassInfo{ VulkanHelpers::Initializers::RenderPassBeginInfo() };
        renderPassInfo.renderPass = m_ImGuiRenderPass;// Use the render pass for ImGui
        renderPassInfo.framebuffer = m_DrawFrameBuffer->Get();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_Extent2D;

        std::array<VkClearValue, 2> clearValues{};            // Only one clear value for the color attachment
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };// Clear color for ImGui
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<UInt32_T>( clearValues.size() );
        renderPassInfo.pClearValues = clearValues.data();
        renderPassInfo.clearValueCount = static_cast<UInt32_T>( clearValues.size() );
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass( cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

        VulkanSwapChain& vulkanSwapChain{ VulkanContext::Get().GetSwapChain() };


        // Set Viewport and Scissor
        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>( vulkanSwapChain.GetExtent().width ),
            .height = static_cast<float>( vulkanSwapChain.GetExtent().height ),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor{
            .offset{ 0, 0 },
            .extent{ vulkanSwapChain.GetExtent() },
        };

        vkCmdSetViewport( cmd, 0, 1, std::addressof( viewport ) );
        vkCmdSetScissor( cmd, 0, 1, std::addressof( scissor ) );

        ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmd );

        // End ImGui-specific render pass
        vkCmdEndRenderPass( cmd );
    }

    auto ImGuiVulkanBackend::InitializeImGuiRender() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        m_ColorAttachmentFormat = device.FindSupportedFormat(
                { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT );

        m_DepthAttachmentFormat = device.FindSupportedFormat(
                { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
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
        colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;// VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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


        std::array attachmentDependencies{ colorAttachmentDependency, deptAttachmentDependency };
        std::array attachmentDescriptions{ colorAttachmentDesc, depthAttachmentDesc };

        VkRenderPassCreateInfo info{ VulkanHelpers::Initializers::RenderPassCreateInfo() };
        info.attachmentCount = static_cast<UInt32_T>( attachmentDescriptions.size() );
        info.pAttachments = attachmentDescriptions.data();

        info.dependencyCount = static_cast<UInt32_T>( attachmentDependencies.size() );
        info.pDependencies = attachmentDependencies.data();

        info.subpassCount = 1;
        info.pSubpasses = &subpass;

        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        if ( vkCreateRenderPass( device.GetLogicalDevice(), &info, nullptr, std::addressof( m_ImGuiRenderPass ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create render pass for the Vulkan Renderer!" );
        }

        VulkanDeletionQueue::Push( [renderPass = m_ImGuiRenderPass, device = device.GetLogicalDevice()]() -> void {
            vkDestroyRenderPass( device, renderPass, nullptr );
        } );
    }

    auto ImGuiVulkanBackend::CreateFrameBuffer() -> void {
        const std::array attachments{ m_ColorImage->GetView(), m_DepthImage->GetView() };

        VkFramebufferCreateInfo createInfo{ VulkanHelpers::Initializers::FramebufferCreateInfo() };
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_ImGuiRenderPass;

        createInfo.width = m_Extent2D.width;
        createInfo.height = m_Extent2D.height;
        createInfo.layers = 1;

        createInfo.attachmentCount = static_cast<UInt32_T>( attachments.size() );
        createInfo.pAttachments = attachments.data();

        m_DrawFrameBuffer = VulkanFrameBuffer::Create( VulkanFrameBufferCreateInfo{ .CreateInfo { createInfo } } );
    }

    auto ImGuiVulkanBackend::CreateImages() -> void {
        // Color Buffer attachment
        VkImageCreateInfo colorAttachmentCreateInfo{ VulkanHelpers::Initializers::ImageCreateInfo() };
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
        colorAttachmentCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkImageViewCreateInfo colorAttachmentViewCreateInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };
        colorAttachmentViewCreateInfo.pNext = nullptr;
        colorAttachmentViewCreateInfo.flags = 0;
        colorAttachmentViewCreateInfo.image = VK_NULL_HANDLE;
        colorAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentViewCreateInfo.format = colorAttachmentCreateInfo.format;// match formats for simplicity

        colorAttachmentViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        colorAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        colorAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        colorAttachmentViewCreateInfo.subresourceRange.layerCount = 1;

        VulkanImageCreateInfo colorImageCreateInfo{
            .Image{ VK_NULL_HANDLE },
            .ImageCreateInfo{ colorAttachmentCreateInfo },
            .ImageViewCreateInfo{ colorAttachmentViewCreateInfo }
        };
        m_ColorImage = VulkanImage::Create( colorImageCreateInfo );

        // Depth attachment
        VkImageCreateInfo depthAttachmentCreateInfo{ VulkanHelpers::Initializers::ImageCreateInfo() };
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

        VkImageViewCreateInfo depthAttachmentViewCreateInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };

        depthAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthAttachmentViewCreateInfo.image = VK_NULL_HANDLE;// Set by OnCreate()
        depthAttachmentViewCreateInfo.format = depthAttachmentCreateInfo.format;

        depthAttachmentViewCreateInfo.flags = 0;
        depthAttachmentViewCreateInfo.subresourceRange = {};
        depthAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        depthAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        depthAttachmentViewCreateInfo.subresourceRange.layerCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.aspectMask =
                depthAttachmentCreateInfo.format < VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_DEPTH_BIT : ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT );

        VulkanImageCreateInfo depthImageCreateInfo{
            .Image{ VK_NULL_HANDLE },
            .ImageCreateInfo{ depthAttachmentCreateInfo },
            .ImageViewCreateInfo{ depthAttachmentViewCreateInfo }
        };
        m_DepthImage = VulkanImage::Create( depthImageCreateInfo );
    }

    auto ImGuiVulkanBackend::RecordCommands( const VkCommandBuffer cmd, VulkanImage& currentSwapChainImage ) const -> void {
        // Begin recording command buffer
        VkCommandBufferBeginInfo beginInfo{ VulkanHelpers::Initializers::CommandBufferBeginInfo() };

        if ( vkBeginCommandBuffer( cmd, std::addressof( beginInfo ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to begin recording ImGui command buffer" );
        }

        // Record imgui draw commands
        PrepareMainRenderPass( cmd );

        // the transition the draw image and the swapchain image into their correct transfer layouts
        // the first time we enter here the layouts are undefined
        m_ColorImage->LayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmd );
        currentSwapChainImage.LayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd );

        VulkanSwapChain& swapChain{ VulkanContext::Get().GetSwapChain() };

        VkExtent3D extent{};
        extent.height = swapChain.GetExtent().height;
        extent.width = swapChain.GetExtent().width;
        extent.depth = 1;

        // execute a copy from the draw image into the swapchain
        VulkanHelpers::CopyImageToImage( cmd, m_ColorImage->Get(), currentSwapChainImage.Get(), extent );

        // Reset layouts
        m_ColorImage->LayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd );
        currentSwapChainImage.LayoutTransition( VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, cmd );

        // End recording command buffer
        if ( vkEndCommandBuffer( cmd ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to record ImGui command buffer!" );
        }
    }

    auto ImGuiVulkanBackend::InitializeCommands() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        VkCommandPoolCreateInfo createInfo{ VulkanHelpers::Initializers::CommandPoolCreateInfo() };
        createInfo.flags = 0;
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = device.GetLogicalDeviceQueues().Graphics->FamilyIndex;

        m_CommandPool = VulkanCommandPool::Create( VulkanCommandPoolCreateInfo{ .CreateInfo{ createInfo } } );
    }

    auto ImGuiVulkanBackend::InitCommandBuffers() -> void {
        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };
        const VulkanSwapChain& swapChain{ VulkanContext::Get().GetSwapChain() };

        Size_T swapchainImagesCount{ swapChain.GetImageCount() };

        for (Size_T count{}; count < swapchainImagesCount; ++count) {
            VkCommandBufferAllocateInfo allocInfo{ VulkanHelpers::Initializers::CommandBufferAllocateInfo() };
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = m_CommandPool->Get();
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer{};
            if ( vkAllocateCommandBuffers(
                    device.GetLogicalDevice(),
                    std::addressof(allocInfo),
                    std::addressof(commandBuffer) ) != VK_SUCCESS )
            {
                MKT_THROW_RUNTIME_ERROR( "ImGuiVulkanBackend::InitCommandBuffers - Failed to allocate command buffer" );
            }

            m_DrawCommandBuffers.emplace_back( commandBuffer );

            VulkanDeletionQueue::Push( [cmdPoolHandle = m_CommandPool->Get(), cmdHandle = commandBuffer, device = device.GetLogicalDevice()]() -> void {
                vkFreeCommandBuffers( device, cmdPoolHandle, 1, std::addressof( cmdHandle ) );
            } );
        }
    }
}