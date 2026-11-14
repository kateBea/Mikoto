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

#include <array>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <EASTL/array.h>

#include <volk.h>

// ImGuizmo requires ImGui's type declarations before its header is parsed.
// clang-format off
#include <ImGuizmo.h>
#include <imgui.h>

// clang-format on


#include <Core/Core.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Assets/Utility.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

#include <ImGui/ImGuiVulkanBackend.hh>

namespace mikoto::imgui {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace mikoto::renderer::vulkan;

    ImGuiVulkanBackend::TexturePool::~TexturePool() {
        Destroy();
    }

    auto ImGuiVulkanBackend::TexturePool::Initialize() -> void {
        auto* device{ core::checked_cast<renderer::vulkan::Device*>( mDevice ) };
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

        VkDescriptorPoolCreateInfo poolCreateInfo{ renderer::vulkan::initializers::DescriptorPoolCreateInfo() };
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = 1000;
        poolCreateInfo.poolSizeCount = core::as<core::u32>( poolSizes.size() );
        poolCreateInfo.pPoolSizes = poolSizes.data();

        MKT_VK_CHECK( vkCreateDescriptorPool( device->GetDevice(), MKT_ADDRESSOF( poolCreateInfo ), nullptr, MKT_ADDRESSOF( mPool ) ) );
    }

    auto ImGuiVulkanBackend::TexturePool::Destroy() -> void {
        if ( mPool ) {
            vkDestroyDescriptorPool( core::checked_cast<renderer::vulkan::Device*>( mDevice )->GetDevice(), mPool, nullptr );
            mPool = VK_NULL_HANDLE;
        }
    }

    ImGuiVulkanBackend::TextureBinding::TextureBinding( renderer::rhi::TextureHandle texture, core::Ref<TexturePool> pool )
        : mTexture{ eastl::move( texture ) }, mPool{ eastl::move( pool ) } {
    }

    ImGuiVulkanBackend::TextureBinding::~TextureBinding() {
        Destroy();
    }

    auto ImGuiVulkanBackend::TextureBinding::Initialize() -> void {
        mSampler = core::checked_cast<renderer::vulkan::Device*>( mDevice )->GetDummySampler();
        const VkImageView view{ mTexture->GetNativeHandle( renderer::rhi::ObjectType::Vk_ImageView ) };

        if ( view == VK_NULL_HANDLE ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot display a Vulkan texture without a valid image view." );
        }

        std::lock_guard lock{ mPool->mMutex };
        mDescriptorSet = ImGui_ImplVulkan_AddTexture( mSampler->GetNativeHandle( renderer::rhi::ObjectType::Vk_Sampler ), view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
    }

    auto ImGuiVulkanBackend::TextureBinding::Destroy() -> void {
        if ( mDescriptorSet ) {
            // Completion collection can run without an active ImGui context.
            // Free through the retained native pool instead of ImGui globals.
            std::lock_guard lock{ mPool->mMutex };
            vkFreeDescriptorSets( core::checked_cast<renderer::vulkan::Device*>( mDevice )->GetDevice(), mPool->mPool, 1, &mDescriptorSet );
            mDescriptorSet = VK_NULL_HANDLE;
        }
    }

    ImGuiVulkanBackend::ImGuiVulkanBackend( const ImGuiBackendCreateInfo& createInfo )
        : ImGuiBackend{ createInfo } {
        MKT_BEGIN_PROFILER();

        auto dimensions{ renderer::rhi::InferDimensions( mResolution ) };

        mDimensions.width = dimensions.first;
        mDimensions.height = dimensions.second;
    }

    auto ImGuiVulkanBackend::Init() -> void {
        MKT_BEGIN_PROFILER();
        CreateImages();

        mCommandList = mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics );
        mCommandList->SetDebugName( "ImGui Command Buffer" );
        mSubmissionFence = mDevice->CreateFence( mSubmissionValue++ );

        InitImGuiForVulkan();
        mIsInitialized = true;
    }

    auto ImGuiVulkanBackend::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        mDevice->WaitIdle();

        // Handles need to be disabled as the destruction of the graphics context
        // is deferred to the ImGuiService destruction where we might not have a context ready
        // Services in Mikoto do not do their cleanup in the destructor they do it on the Shutdown method
        mColorImage.Reset();
        mDepthImage.Reset();

        // WaitIdle above completed every submission that used these descriptors.
        mCommandList.Reset();
        mImGuiSets.clear();
        mRetiredTextureBindings.clear();
        mSubmissionFence.Reset();
        {
            std::lock_guard lock{ mTexturePool->mMutex };
            ImGui_ImplVulkan_Shutdown();
        }

        ImGui_ImplSDL3_Shutdown();
        mTexturePool.Reset();

        mIsInitialized = false;
    }

    auto ImGuiVulkanBackend::BeginFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const core::u64 completed{ mSubmissionFence->GetCompletionValue() };
        eastl::erase_if( mRetiredTextureBindings, [completed]( const ImGuiTextIDInfo& binding ) {
            return binding.mLastUse <= completed;
        } );

        {
            std::lock_guard lock{ mTexturePool->mMutex };
            ImGui_ImplVulkan_NewFrame();
        }

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiVulkanBackend::InitImGuiForVulkan() -> void {
        MKT_BEGIN_PROFILER();

        renderer::vulkan::Device* device{ core::as<renderer::vulkan::Device*>( mDevice ) };
        renderer::vulkan::Context* context{ core::as<renderer::vulkan::Context*>( renderer::RenderSystem::Get()->GetContext() ) };

        mTexturePool = core::Ref<TexturePool>::New();
        mTexturePool->Initialize( mDevice );

#if __linux__
        ImGui_ImplVulkan_LoadFunctions( Context::Get()->GetApiVersion(),
            []( const char* functionName, void* vulkanInstance ) {
            return vkGetInstanceProcAddr( *core::as<VkInstance*>( vulkanInstance ), functionName );
        },
            std::addressof( Context::Get()->GetInstance() ) );
#else
        ImGui_ImplVulkan_LoadFunctions( context->GetApiVersion(),
            []( const char* functionName, void* vulkanInstance ) {
            return vkGetInstanceProcAddr( *static_cast<VkInstance*>( vulkanInstance ), functionName );
        }, std::addressof( context->GetInstance() ) );
#endif

        renderer::vulkan::Queue* graphicsQueue{ core::checked_cast<renderer::vulkan::Queue*>( device->GetQueue( renderer::rhi::QueueType::eGraphics ) ) };

        MKT_ASSERT( graphicsQueue, "A graphics queue is needed" );

        eastl::array<VkFormat, 1> colorFormats{ renderer::vulkan::GetFormat( mColorImage->GetFormat() ) };

        ImGui_ImplVulkan_InitInfo initInfo{
            .ApiVersion = context->GetApiVersion(),
            .Instance = context->GetInstance().mInstance,
            .PhysicalDevice = device->GetPhysicalDevice()->mPhysicalDevice,
            .Device = device->GetDevice(),
            .Queue = *graphicsQueue,
            .DescriptorPool = mTexturePool->mPool,
            .MinImageCount = 3,
            .ImageCount = 3,
            .PipelineInfoMain{
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .PipelineRenderingCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .colorAttachmentCount = core::as<core::u32>( colorFormats.size() ),
                    .pColorAttachmentFormats = colorFormats.data(),
                    .depthAttachmentFormat = renderer::vulkan::GetFormat( mDepthImage->GetFormat() ),
                },
            },
            // Mikoto defaults to vulkan 1.3 where dynamic rendering is core
            .UseDynamicRendering = true,
        };

        if ( !ImGui_ImplSDL3_InitForVulkan( GetSDLWindow() ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to initialize ImGui SDL for Vulkan." );
        }

        if ( !ImGui_ImplVulkan_Init( MKT_ADDRESSOF( initInfo ) ) ) {
            ImGui_ImplSDL3_Shutdown();
            MKT_THROW_RUNTIME_ERROR( "ImGuiVulkanBackend - Failed to initialize Vulkan for ImGui" );
        }
    }

    auto ImGuiVulkanBackend::CreateImages() -> void {
        MKT_BEGIN_PROFILER();
        // Color Device attachment. Since we will manage these textures here we can set
        // the tracking state to manual so we will handle the transitions and barriers ourselves
        auto colorDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( mDimensions.width ) )
            .SetHeight( core::as<core::i32>( mDimensions.height ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::CopySource | renderer::rhi::TextureUsageFlagsBits::ShaderResource )// I will copy from this guy to swapchain image
            .SetFormat( renderer::rhi::Format::eBGRA8_UNORM ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "ImGui Color image" );

        // Depth attachment
        auto depthDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( mDimensions.width ) )
            .SetHeight( core::as<core::i32>( mDimensions.height ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget )
            .SetFormat( renderer::rhi::Format::eD32 ) };

        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "ImGui Depth image" );
    }

    auto ImGuiVulkanBackend::EndFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        renderer::vulkan::Context* context{ core::checked_cast<renderer::vulkan::Context*>( renderer::RenderSystem::Get()->GetContext() ) };

        // If the swap chain has been resized, we need to recreate the framebuffers and images
        renderer::vulkan::VulkanSwapChainHandle swapChain{ context->GetSwapChain() };

        if ( swapChain->GetWidth() != mDimensions.width || swapChain->GetHeight() != mDimensions.height ) {
            mDimensions.width = swapChain->GetWidth();
            mDimensions.height = swapChain->GetHeight();

            // Earlier recordings retain these attachments through transitions.
            CreateImages();
        }

#if false
        ImGui::Begin("Performance");

        const float fps = ImGui::GetIO().Framerate;
        const float frameTime = 1000.0f / fps;

        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame time: %.2f ms", frameTime);

        ImGui::End();
#endif

        ImGui::Render();

        mCommandList->Open( { .mScopeName = "ImGui Render" } );

        // The batched signal covers main and secondary viewport reads on this queue.
        for ( auto it{ mImGuiSets.begin() }; it != mImGuiSets.end(); ) {
            if ( it->second.mUsedThisFrame ) {
                it->second.mLastUse = mSubmissionValue;
                it->second.mUsedThisFrame = false;
                ++it;
            }
            else {
                mRetiredTextureBindings.push_back( {
                    .mBinding = eastl::move( it->second.mBinding ),
                    .mLastUse = it->second.mLastUse
                } );
                it = mImGuiSets.erase( it );
            }
        }

        mCommandList->SetTransition( renderer::rhi::TransitionDescription{}
            .AddTexture( mColorImage.GetPtr(), renderer::rhi::ResourceStates::eRenderTarget )
            .AddTexture( mDepthImage.GetPtr(), renderer::rhi::ResourceStates::eDepthWrite ) );

        RecordRenderCommands();

        mCommandList->Close();

        auto submitInfo{ renderer::rhi::SubmitInfo{}
            .AddCommandList( mCommandList )
            .AddSignal( mSubmissionFence, mSubmissionValue++ ) };
        renderer::RenderSystem::Get()->BatchSubmission( eastl::move( submitInfo ), renderer::rhi::QueueType::eGraphics );

#if false
        renderer::RenderSystem::Get()->SetPresentTarget( ImGuiService::Get()->GetFinalComposition() );
#endif

        if ( const ImGuiIO & io{ ImGui::GetIO() }; io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            ImGui::UpdatePlatformWindows();

            // Secondary viewports submit to this same graphics queue now;
            // the batched main submission follows them and covers their reads.
            std::lock_guard lock{ mTexturePool->mMutex };
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    auto ImGuiVulkanBackend::GetFinalComposition() -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return mColorImage;
    }

    auto ImGuiVulkanBackend::ConstructImGuiTextureID( const renderer::rhi::ITexture* texture ) -> ImTextureID {
        MKT_BEGIN_PROFILER();

        if ( texture == nullptr ) {
            return 0;
        }

        ImTextureID result{};

        if ( texture->GetResourceState() != renderer::rhi::ResourceStates::eShaderResource ) {
            return result;
        }

        const auto found{ mImGuiSets.find( texture ) };

        if ( found != mImGuiSets.end() ) {
            found->second.mUsedThisFrame = true;
            return reinterpret_cast<ImTextureID>( found->second.mBinding->mDescriptorSet );
        }

        auto binding{ core::Ref<TextureBinding>::New( renderer::rhi::TextureHandle{ const_cast<renderer::rhi::ITexture*>( texture ) }, mTexturePool ) };
        binding->Initialize( mDevice );
        result = reinterpret_cast<ImTextureID>( binding->mDescriptorSet );
        mImGuiSets.try_emplace( texture, ImGuiTextIDInfo{ eastl::move( binding ), true } );

        return result;
    }

    auto ImGuiVulkanBackend::ConstructImGuiTextureID( renderer::rhi::TextureHandle texture ) -> ImTextureID {
        MKT_BEGIN_PROFILER();
        return ConstructImGuiTextureID( texture.GetPtr() );
    }

    auto ImGuiVulkanBackend::RecordRenderCommands() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Only one clear value for the color attachment
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a };// Clear color for ImGui
        clearValues[1].depthStencil = { 1.0f, 0 };

        // Setup rendering color attachment
        VkRenderingAttachmentInfo colorAttachment{ renderer::vulkan::initializers::RenderingAttachmentInfo() };

        colorAttachment.imageView = mColorImage->GetNativeHandle( renderer::rhi::ObjectType::Vk_ImageView );
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearValues[0];

        // Setup rendering depth attachment
        VkRenderingAttachmentInfo depthAttachment{ renderer::vulkan::initializers::RenderingAttachmentInfo() };

        depthAttachment.imageView = mDepthImage->GetNativeHandle( renderer::rhi::ObjectType::Vk_ImageView );
        depthAttachment.imageLayout = renderer::vulkan::GetImageLayout( renderer::rhi::ResourceStates::eDepthWrite );
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue = clearValues[1];

        VkRenderingInfo renderInfo{ renderer::vulkan::initializers::RenderingInfo() };

        renderInfo.renderArea = VkRect2D{
            VkOffset2D{ 0, 0 },
            VkExtent2D{ mDimensions.width, mDimensions.height }
        };
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachment;
        renderInfo.pDepthAttachment = &depthAttachment;
        renderInfo.pStencilAttachment = nullptr;

        const auto nativeCmdListHandle{ mCommandList->GetNativeHandle( renderer::rhi::ObjectType::Vk_CmdBuffer ) };
        vkCmdBeginRendering( nativeCmdListHandle, std::addressof( renderInfo ) );

        RecordViewportState();

        {
            std::lock_guard lock{ mTexturePool->mMutex };
            ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), nativeCmdListHandle );
        }

        vkCmdEndRendering( nativeCmdListHandle );
    }

    auto ImGuiVulkanBackend::RecordViewportState() -> void {
        MKT_BEGIN_PROFILER();
        const auto nativeCmdListHandle{ mCommandList->GetNativeHandle( renderer::rhi::ObjectType::Vk_CmdBuffer ) };

        renderer::vulkan::Context* context{ core::checked_cast<renderer::vulkan::Context*>( renderer::RenderSystem::Get()->GetContext() ) };
        renderer::vulkan::VulkanSwapChainHandle vulkanSwapChain{ context->GetSwapChain() };

        // Set Viewport and Scissor
        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = core::as<float>( vulkanSwapChain->GetWidth() ),
            .height = core::as<float>( vulkanSwapChain->GetHeight() ),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        VkRect2D scissor{
            .offset{ 0, 0 },
                .extent{ vulkanSwapChain->GetWidth(), vulkanSwapChain->GetHeight() }
        };

        vkCmdSetViewport( nativeCmdListHandle, 0, 1, std::addressof( viewport ) );
        vkCmdSetScissor( nativeCmdListHandle, 0, 1, std::addressof( scissor ) );
    }
}
