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

#include <ranges>

#include <EASTL/array.h>

#include <volk.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <ImGui/ImGuiVulkanBackend.hh>

#include <Assets/ImageProcessor.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace mikoto::gui {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::vulkan;

    ImGuiVulkanBackend::ImGuiVulkanBackend( const ImGuiBackendCreateInfo& createInfo )
        : ImGuiBackend{ createInfo } {

        auto dimensions{ rhi::InferDimensions( mResolution ) };

        mDimensions.width = dimensions.first;
        mDimensions.height = dimensions.second;
    }

    auto ImGuiVulkanBackend::Init() -> void {
        CreateImages();
        InitImGuiForVulkan();

        mCommandList = mGpuDevice->CreateCommandList( QueueType::eGraphics );

        mIsInitialized = true;
    }

    auto ImGuiVulkanBackend::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        // Handles need to be disabled as the destruction of the graphics context
        // is deferred to the ImGuiService destruction where we might not have a context ready
        // Services in Mikoto do not do their cleanup in the destructor they do it on the Shutdown method
        mColorImage.Reset();
        mDepthImage.Reset();

        // Clear texture IDs
        for (auto& [descriptorSet] : m_ImGuiSets | std::ranges::views::values ) {
            ImGui_ImplVulkan_RemoveTexture( descriptorSet );
        }

        m_ImGuiSets.clear();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        vkDestroyDescriptorPool( as<Device*>( mGpuDevice )->GetDevice(), mImGuiDescriptorPool, nullptr );

        mIsInitialized = false;
    }

    auto ImGuiVulkanBackend::BeginFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiVulkanBackend::InitImGuiForVulkan() -> void {
        const auto window{ eastl::any_cast<GLFWwindow*>( m_Window->GetNativeWindow() ) };

        Device* device{ as<Device*>( mGpuDevice ) };
        Context* context{ as<Context*>( RenderSystem::Get()->GetContext() ) };

        eastl::array<VkDescriptorPoolSize, 11> poolSizes{
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

        VkDescriptorPoolCreateInfo poolCreateInfo{ initializers::DescriptorPoolCreateInfo() };
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = 1000;
        poolCreateInfo.poolSizeCount = as<u32>( poolSizes.size() );
        poolCreateInfo.pPoolSizes = poolSizes.data();

        MKT_VK_CHECK( vkCreateDescriptorPool( device->GetDevice(), MKT_ADDRESSOF( poolCreateInfo ),nullptr, MKT_ADDRESSOF( mImGuiDescriptorPool ) ) );

#if __linux__
        ImGui_ImplVulkan_LoadFunctions(Context::Get()->GetApiVersion(),
                []( const char* functionName, void* vulkanInstance ) {
                    return vkGetInstanceProcAddr( *as<VkInstance*>( vulkanInstance ), functionName );
                },
                std::addressof( Context::Get()->GetInstance() ) );
#else
        ImGui_ImplVulkan_LoadFunctions( context->GetApiVersion(),
            []( const char* functionName, void* vulkanInstance ) {
            return vkGetInstanceProcAddr( *static_cast<VkInstance*>( vulkanInstance ), functionName );
        }, std::addressof( context->GetInstance() ) );
#endif

        ImGui_ImplGlfw_InitForVulkan( window, true );

        Queue* graphicsQueue{ device->GetQueue( QueueType::eGraphics ) };

        MKT_ASSERT( graphicsQueue, "A graphics queue is needed" );

        eastl::array<VkFormat, 1> colorFormats{ GetFormat(mColorImage->GetFormat() ) };

        ImGui_ImplVulkan_InitInfo initInfo{
            .ApiVersion = context->GetApiVersion(),
            .Instance = context->GetInstance().mInstance,
            .PhysicalDevice = device->GetPhysicalDevice()->mPhysicalDevice,
            .Device = device->GetDevice(),
            .Queue = *graphicsQueue,
            .DescriptorPool = mImGuiDescriptorPool,
            .MinImageCount = 3 ,
            .ImageCount = 3,
            .PipelineInfoMain{
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .PipelineRenderingCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .colorAttachmentCount = as<u32>( colorFormats.size() ),
                    .pColorAttachmentFormats = colorFormats.data(),
                    .depthAttachmentFormat = GetFormat(mDepthImage->GetFormat() ),
                },
            },

            // Mikoto defaults to vulkan 1.3 where dynamic rendering is core
            .UseDynamicRendering = true,
        };

        if ( !ImGui_ImplVulkan_Init( MKT_ADDRESSOF( initInfo ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "ImGuiVulkanBackend - Failed to initialize Vulkan for ImGui" );
        }
    }

    auto ImGuiVulkanBackend::CreateImages() -> void {

        // Color Device attachment. Since we will manage these textures here we can set
        // the tracking state to manual so we will handle the transitions and barriers ourselves
        auto colorDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( mDimensions.width ) )
            .SetHeight( as<i32>( mDimensions.height ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget )
            .SetFormat( Format::eBGRA8_UNORM ) };

        mColorImage = mGpuDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "ImGui Color image" );

        // Depth attachment
        auto depthDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( mDimensions.width ) )
            .SetHeight( as<i32>( mDimensions.height ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

        mDepthImage = mGpuDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "ImGui Depth image" );
    }

    auto ImGuiVulkanBackend::EndFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        Device* device{ as<Device*>( mGpuDevice ) };
        Context* context{ as<Context*>( RenderSystem::Get()->GetContext() ) };

        // If the swap chain has been resized, we need to recreate the framebuffers and images
        SwapChainHandle swapChain{ context->GetSwapchain() };
        if ( swapChain->GetWidth() != mDimensions.width || swapChain->GetHeight() != mDimensions.height ) {
            mDimensions.width = swapChain->GetWidth();
            mDimensions.height = swapChain->GetHeight();

            CreateImages();
        }

        ImGui::Render();

        mCommandList->Begin();

        mCommandList->SetResourceState( mColorImage.GetRaw(), ResourceStates::eRenderTarget );

        RecordCommands( mCommandList );

        mCommandList->End();
        mGpuDevice->SubmitCommands( mCommandList );

        if ( const ImGuiIO & io{ ImGui::GetIO() }; io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        // [ DEBUG ]
        RenderSystem::Get()->SetPresentTarget( mColorImage );
    }

    auto ImGuiVulkanBackend::GetFinalComposition() -> TextureHandle {
        return mColorImage;
    }

    auto ImGuiVulkanBackend::ConstructImGuiTextureID( const ITexture* texture ) -> ImTextureID {
        if (texture == nullptr) {
            return 0;
        }

        ImTextureID result{};

         if ( texture->GetResourceState() != ResourceStates::eShaderResource ) {
            return result;
        }

        auto itFind{ m_ImGuiSets.find( texture ) };
        if ( itFind != m_ImGuiSets.end() ) {
            result = reinterpret_cast<ImTextureID>(itFind->second.descriptorSet);
        } else {
            ISampler* sampler{ checked_cast<vulkan::Device*>( mGpuDevice )->GetDummySampler() };

            const auto color{ as<const Texture*>( texture ) };

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

    auto ImGuiVulkanBackend::RecordDynamicRenderCommands( CommandListHandle cmdList ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Only one clear value for the color attachment
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a };// Clear color for ImGui
        clearValues[1].depthStencil = { 1.0f, 0 };

        // Setup rendering color attachment
        VkRenderingAttachmentInfo colorAttachment{ initializers::RenderingAttachmentInfo() };

        colorAttachment.imageView = mColorImage->GetNativeHandle( ObjectType::Vk_ImageView );
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearValues[0];

        // Setup rendering depth attachment
        VkRenderingAttachmentInfo depthAttachment{ initializers::RenderingAttachmentInfo() };

        depthAttachment.imageView = mDepthImage->GetNativeHandle( ObjectType::Vk_ImageView );
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue = clearValues[1];

        VkRenderingInfo renderInfo { initializers::RenderingInfo() };

        renderInfo.renderArea = VkRect2D{
            VkOffset2D { 0, 0 },
            VkExtent2D{ mDimensions.width, mDimensions.height }
        };
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

        SwapChainHandle vulkanSwapChain{ Context::Get()->GetSwapchain() };

        // Set Viewport and Scissor
        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = as<float>( vulkanSwapChain->GetWidth() ),
            .height = as<float>( vulkanSwapChain->GetHeight() ),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor{
            .offset{ 0, 0 },
            .extent{ vulkanSwapChain->GetWidth(), vulkanSwapChain->GetHeight() },
        };

        vkCmdSetViewport( nativeCmdListHandle, 0, 1, std::addressof( viewport ) );
        vkCmdSetScissor( nativeCmdListHandle, 0, 1, std::addressof( scissor ) );
    }

    auto ImGuiVulkanBackend::RecordCommands( CommandListHandle cmdList ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RecordDynamicRenderCommands( cmdList );
    }
}