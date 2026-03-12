//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// C++ Standard Library
#include <any>
#include <array>
#include <ranges>

#include <volk.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <ImGuizmo.h>

#include <Core/Profiler.hh>
#include <ImGui/ImGuiVulkanBackend.hh>
#include <Logging/Logger.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace Mikoto {

    auto ImGuiVulkanBackend::Init() -> void {
        CreateImages();

        if (!m_UseDynamicRendering) {
            CreateRenderPass();

            // Frame buffer requires render pass if we
            // do not use dynamic rendering
            CreateFrameBuffer();
        }

        InitImGuiForVulkan();

        m_IsInitialized = true;
    }

    auto ImGuiVulkanBackend::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_IsInitialized ) {
            return;
        }

        // Handles need to be disabled as the destruction of the graphics context
        // is deferred to the ImGuiService destruction where we might not have a context ready
        // Services in Mikoto do not do their cleanup in the destructor they do it on the Shutdown method
        m_ColorImage.Reset();
        m_DepthImage.Reset();
        m_DrawFrameBuffer.Reset();

        // Wait for remaining operations to complete
        TO_VK_DEVICE( m_GpuDevice )->WaitIdle();

        // Clear texture IDs
        for (auto& [descriptorSet] : m_ImGuiSets | std::ranges::views::values ) {
            ImGui_ImplVulkan_RemoveTexture( descriptorSet );
        }

        m_ImGuiSets.clear();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        vkDestroyRenderPass( VK_DEVICE( m_GpuDevice ), m_ImGuiRenderPass, nullptr );
        vkDestroyDescriptorPool( VK_DEVICE( m_GpuDevice ), m_ImGuiDescriptorPool, nullptr );

        m_IsInitialized = false;
    }

    auto ImGuiVulkanBackend::BeginFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiVulkanBackend::CreateRenderPass() -> void {
        const std::initializer_list<const VkFormat> targetColorFormats{
            VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB
        };

        const std::initializer_list<const VkFormat> targetDepthFormats{
            VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT
        };

        VkFormat colorFormat{ VulkanHelpers::FindSupportedFormat(
                TO_VK_DEVICE( m_GpuDevice )->GetPhysicalDevice(),
                targetColorFormats,
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT ) };

        VkFormat depthFormat{ VulkanHelpers::FindSupportedFormat(
                TO_VK_DEVICE( m_GpuDevice )->GetPhysicalDevice(),
                targetDepthFormats,
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) };

        // Color Attachment
        VkAttachmentDescription colorAttachmentDesc{};
        colorAttachmentDesc.format = colorFormat;
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
        depthAttachmentDesc.format = depthFormat;
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
        info.attachmentCount = static_cast<UInt32>( attachmentDescriptions.size() );
        info.pAttachments = attachmentDescriptions.data();

        info.dependencyCount = static_cast<UInt32>( attachmentDependencies.size() );
        info.pDependencies = attachmentDependencies.data();

        info.subpassCount = 1;
        info.pSubpasses = &subpass;

        if ( vkCreateRenderPass( VK_DEVICE( m_GpuDevice ), std::addressof( info ), nullptr, std::addressof( m_ImGuiRenderPass ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "ImGuiVulkanBackend::CreateRenderPass - Failed to create render pass for the Vulkan Renderer!" );
        }
    }

    auto ImGuiVulkanBackend::InitImGuiForVulkan() -> void {
        const auto window{ std::any_cast<GLFWwindow*>( m_Window->GetNativeWindow() ) };

        std::array<VkDescriptorPoolSize, 11> poolSizes{
            VK_DESCRIPTOR_TYPE_SAMPLER, 1000,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000,
            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000,
            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000,
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000
        };

        VkDescriptorPoolCreateInfo poolCreateInfo{ VulkanHelpers::Initializers::DescriptorPoolCreateInfo() };
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = 1000;
        poolCreateInfo.poolSizeCount = static_cast<UInt32>( poolSizes.size() );
        poolCreateInfo.pPoolSizes = poolSizes.data();

        const VulkanDevice* device{ MKT_VK_DEVICE( m_GpuDevice ) };

        if ( vkCreateDescriptorPool( device->GetLogicalDevice(),
                                     std::addressof( poolCreateInfo ),
                                     nullptr,
                                     std::addressof( m_ImGuiDescriptorPool ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "ImGuiVulkanBackend::InitImGuiForVulkan - Failed to create descriptor pool for ImGui." );
        }

        VulkanContext* context{ MKT_VK_CTX( RenderService::Get()->GetContext() ) };

#if __linux__
        ImGui_ImplVulkan_LoadFunctions(VulkanContext::Get()->GetApiVersion(),
                []( const char* functionName, void* vulkanInstance ) {
                    return vkGetInstanceProcAddr( *static_cast<VkInstance*>( vulkanInstance ), functionName );
                },
                std::addressof( VulkanContext::Get()->GetInstance() ) );
#else
        ImGui_ImplVulkan_LoadFunctions( context->GetApiVersion(),
            []( const char* functionName, void* vulkanInstance ) {
            return vkGetInstanceProcAddr( *static_cast<VkInstance*>( vulkanInstance ), functionName );
        }, std::addressof( context->GetInstance() ) );
#endif

        ImGui_ImplGlfw_InitForVulkan( window, true );

        ImGui_ImplVulkan_InitInfo initInfo{
            .ApiVersion{ context->GetApiVersion() },
            .Instance = context->GetInstance(),
            .PhysicalDevice = device->GetPhysicalDevice(),
            .Device = VK_DEVICE(m_GpuDevice),
            .Queue = device->GetLogicalDeviceQueues().Graphics->Queue,
            .DescriptorPool = m_ImGuiDescriptorPool,
            .MinImageCount{ 3 },
            .ImageCount{ 3 },
            .PipelineInfoMain{
                .RenderPass{ m_ImGuiRenderPass },
                .Subpass{ 0 },
                .MSAASamples{ VK_SAMPLE_COUNT_1_BIT } 
            },
        };

        if (m_UseDynamicRendering) {
            initInfo.UseDynamicRendering = true;

            VkFormat* colorFormat{ m_ColorImage->GetNativeHandle( ObjectType::Vk_Format ) };
            VkFormat* depthFormat{ m_DepthImage->GetNativeHandle( ObjectType::Vk_Format ) };

            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {
                .sType{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO },
                .colorAttachmentCount{ 1 },
                .pColorAttachmentFormats{ colorFormat },
                .depthAttachmentFormat{ *depthFormat },
            };
        }

        if ( !ImGui_ImplVulkan_Init( std::addressof( initInfo ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "ImGuiVulkanBackend - Failed to initialize Vulkan for ImGui" );
        }
    }

    auto ImGuiVulkanBackend::CreateImages() -> void {

        // Color Device attachment
        TextureDescription colorDesc{};
        colorDesc.WithWidth( static_cast<Int32>( m_Extent2D.width ) )
                .WithHeight( static_cast<Int32>( m_Extent2D.height ) )
                .WithChannelCount( 4 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::COLOR )
                .WithFormat( TextureFormat::RGBA8_UNORM )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

        m_ColorImage = m_GpuDevice->CreateTexture( colorDesc );
        m_ColorImage->SetDebugName( "ImGui Color image" );

        // Depth attachment
        TextureDescription depthDesc{};
        depthDesc.WithWidth( static_cast<Int32>(m_Extent2D.width) )
                .WithHeight( static_cast<Int32>(m_Extent2D.height) )
                .WithChannelCount( 1 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::DEPTH )
                .WithFormat( TextureFormat::D32_FLOAT_S8_UINT )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC);

        m_DepthImage = m_GpuDevice->CreateTexture( depthDesc );
        m_DepthImage->SetDebugName( "ImGui Depth image" );
    }

    auto ImGuiVulkanBackend::EndFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // If the swap chain has been resized, we need to recreate the framebuffers and images
        SwapChainHandle swapChain{ VulkanContext::Get()->GetSwapchain() };
        if ( swapChain->GetExtent().width != m_Extent2D.width ||
             swapChain->GetExtent().height != m_Extent2D.height ) {

            // Save new dimensions
            m_Extent2D = swapChain->GetExtent();
            m_Extent3D = { m_Extent2D.width, m_Extent2D.height, 1 };

            CreateImages();
            CreateFrameBuffer();
        }

        ImGui::Render();

        CommandListHandle commandList{ m_GpuDevice->CreateCommandList( QueueType::GRAPHICS_QUEUE, false ) };
        commandList->Begin();

        RecordCommands( commandList );

        commandList->End();
        m_GpuDevice->SubmitCommands( commandList );

        if ( const ImGuiIO & io{ ImGui::GetIO() }; io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    auto ImGuiVulkanBackend::GetFinalComposition() -> TextureHandle {
        return m_ColorImage;
    }

    auto ImGuiVulkanBackend::ConstructImGuiTextureID( const Texture* texture ) -> ImTextureID {
        if (texture == nullptr) {
            return 0;
        }

        ImTextureID result{};

         if ( dynamic_cast<const VulkanTexture*>( texture )->GetCurrentLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            return result;
        }

        auto itFind{ m_ImGuiSets.find( texture ) };
        if ( itFind != m_ImGuiSets.end() ) {
            result = reinterpret_cast<ImTextureID>(itFind->second.descriptorSet);
        } else {
            SamplerHandle sampler{ m_GpuDevice->GetDummySampler() };
            const auto color{ dynamic_cast<const VulkanTexture*>( texture ) };

            VkImageView view{ color->GetNativeHandle(ObjectType::Vk_ImageView) };
            VkDescriptorSet ds{ ImGui_ImplVulkan_AddTexture( sampler->GetNativeHandle( ObjectType::Vk_Sampler ), view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) };

            const auto [it, success] {
                m_ImGuiSets.try_emplace( texture, ImGuiTextIDInfo{ ds } )
            };

            result = reinterpret_cast<ImTextureID>(it->second.descriptorSet);
        }

        return result;
    }

    auto ImGuiVulkanBackend::ConstructImGuiTextureID(TextureHandle texture) -> ImTextureID {
        return ConstructImGuiTextureID( texture.GetRaw() );
    }

    auto ImGuiVulkanBackend::CreateFrameBuffer() -> void {
        if (m_UseDynamicRendering) {
            return;
        }

        FramebufferDescription description{};
        description
            .WithWidth( static_cast<Int32>( m_Extent2D.width ) )
            .WithHeight( static_cast<Int32>( m_Extent2D.height ) )
            .AddAttachment( m_ColorImage )
            .AddDepthAttachment( m_DepthImage )
            .WithSpecInfo( VkFramebufferCreateInfo{
                .sType{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO },
                .renderPass{ m_ImGuiRenderPass }
            } );

        m_DrawFrameBuffer = m_GpuDevice->CreateFrameBuffer( description );
        m_DrawFrameBuffer->SetDebugName( "ImGui Framebuffer" );
    }

    auto ImGuiVulkanBackend::RecordRenderPassCommands(CommandListHandle cmdList) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Begin ImGui-specific render pass
        VkRenderPassBeginInfo renderPassInfo{ VulkanHelpers::Initializers::RenderPassBeginInfo() };
        renderPassInfo.renderPass = m_ImGuiRenderPass;// Use the render pass for ImGui
        renderPassInfo.framebuffer = m_DrawFrameBuffer->GetNativeHandle( ObjectType::Vk_Framebuffer );
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_Extent2D;

        std::array<VkClearValue, 2> clearValues{};                                                // Only one clear value for the color attachment
        clearValues[0].color = { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a };// Clear color for ImGui
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<UInt32>( clearValues.size() );
        renderPassInfo.pClearValues = clearValues.data();
        renderPassInfo.clearValueCount = static_cast<UInt32>( clearValues.size() );
        renderPassInfo.pClearValues = clearValues.data();

        SetupViewportAndScissors( cmdList );

        const auto nativeCmdListHandle{ cmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        vkCmdBeginRenderPass( nativeCmdListHandle, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

        ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), nativeCmdListHandle );

        // End ImGui-specific render pass
        vkCmdEndRenderPass( nativeCmdListHandle );
    }

    auto ImGuiVulkanBackend::RecordDynamicRenderCommands( CommandListHandle cmdList ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Only one clear value for the color attachment
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a };// Clear color for ImGui
        clearValues[1].depthStencil = { 1.0f, 0 };

        // Setup rendering color attachment
        VkRenderingAttachmentInfo colorAttachment{ VulkanHelpers::Initializers::RenderingAttachmentInfo() };

        colorAttachment.imageView = m_ColorImage->GetNativeHandle( ObjectType::Vk_ImageView );
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearValues[0];

        // Setup rendering depth attachment
        VkRenderingAttachmentInfo depthAttachment{ VulkanHelpers::Initializers::RenderingAttachmentInfo() };

        depthAttachment.imageView = m_DepthImage->GetNativeHandle( ObjectType::Vk_ImageView );
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue = clearValues[1];

        VkRenderingInfo renderInfo { VulkanHelpers::Initializers::RenderingInfo() };

        renderInfo.renderArea = VkRect2D { VkOffset2D { 0, 0 }, m_Extent2D };
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachment;
        renderInfo.pDepthAttachment = &depthAttachment;
        renderInfo.pStencilAttachment = nullptr;

        const auto nativeCmdListHandle{ cmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        vkCmdBeginRendering(nativeCmdListHandle, std::addressof( renderInfo ));

        SetupViewportAndScissors( cmdList );

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), nativeCmdListHandle);

        vkCmdEndRendering( nativeCmdListHandle );
    }

    auto ImGuiVulkanBackend::SetupViewportAndScissors( CommandListHandle cmdList ) -> void {
        const auto nativeCmdListHandle{ cmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        SwapChainHandle vulkanSwapChain{ VulkanContext::Get()->GetSwapchain() };

        // Set Viewport and Scissor
        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>( vulkanSwapChain->GetExtent().width ),
            .height = static_cast<float>( vulkanSwapChain->GetExtent().height ),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor{
            .offset{ 0, 0 },
            .extent{ vulkanSwapChain->GetExtent() },
        };

        vkCmdSetViewport( nativeCmdListHandle, 0, 1, std::addressof( viewport ) );
        vkCmdSetScissor( nativeCmdListHandle, 0, 1, std::addressof( scissor ) );
    }

    auto ImGuiVulkanBackend::RecordCommands( CommandListHandle cmdList ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        VulkanTexture* finalImage{ dynamic_cast<VulkanTexture *>(m_ColorImage.GetRaw()) };
        finalImage->SubmitLayoutTransition( VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) );

        if (m_UseDynamicRendering) {
            RecordDynamicRenderCommands( cmdList );
        } else {
            RecordRenderPassCommands( cmdList );
        }
    }
}