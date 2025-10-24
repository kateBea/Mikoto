/**
 * VulkanRenderer.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <array>

// Third-Party Libraries
#include <volk.h>

#include <Scene/Component.hh>
#include <Renderer/Vulkan/VulkanPasses.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {

    struct FrameUBO {
        glm::mat4 View{};
        glm::mat4 Projection{};
    };

    VulkanRenderer::VulkanRenderer( GpuDevice *device, std::string_view name )
        : RendererBackend{ { name, device } }
    {}

    auto VulkanRenderer::Init() -> void {
        // Create data that is needed per frame
        // need to use GetUniformBufferMinOffsetAlignment()
        // somewhere here check in legacy how we created them, proper alignment is
        // required gpu packs them in at least that amount of bytes blocks
        BufferDescription bufferDesc{};
        bufferDesc.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithSizeBytes( sizeof(FrameUBO) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STREAM );
        m_FrameUBOBuffer = m_GraphicsDevice->CreateBuffer( bufferDesc );

        CreateBindlessDescriptor();

        m_Materials.Init(10);

        InitCoreRenderPasses();

        m_IsInitialized = true;
    }

    auto VulkanRenderer::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        m_Passes.Clear();

        m_Materials.Shutdown();

        m_BindlessTextures.clear();

        vkFreeDescriptorSets( VK_DEVICE(m_GraphicsDevice) , m_BindlessPool, 1, std::addressof( m_BindlessDescriptorSet ) );

        vkDestroyDescriptorPool(VK_DEVICE(m_GraphicsDevice), m_BindlessPool, nullptr);
        vkDestroyDescriptorSetLayout(VK_DEVICE(m_GraphicsDevice), m_BindlessDescriptorSetLayout, nullptr);

        m_IsInitialized = false;
    }

    auto VulkanRenderer::EndRender() -> void {
        for ( const auto& pass : m_Passes | std::ranges::views::values ) {
            pass->End();
        }
    }

    auto VulkanRenderer::BeginRender( CommandListHandle cmdList ) -> void {
        using namespace Mikoto::VulkanPasses;

        m_GraphicsCommandList = cmdList;

        // Prepare resources
        for ( TextureHandle& texture : m_BindlessTextures ) {

            // Once the texture is ready, update the descriptor containing the texture arrays
            const auto vkTexture{ dynamic_cast<VulkanTexture*>(texture.GetRaw()) };

            const Int32 textureIndex{ vkTexture->GetTextureIndex() };
            UpdateBindlessTextureDescriptor(textureIndex, vkTexture);
        }

        // Shading pass will bind the global renderer descriptor pass
        // this pass is the main pass. These the lights and the texture sets are not changing between passes,
        // shaders must declare them properly to align to this descriptor layout
        ShadingPass* shadingPass{ m_Passes.Get<ShadingPass>() };
        shadingPass->BindDefaultSets(m_BindlessDescriptorSet);

        for ( const auto& pass : m_Passes | std::ranges::views::values) {
            pass->Begin( cmdList );
        }
    }

    auto VulkanRenderer::SetPipeline( const PipelineHandle pipeline ) -> void {
        m_Pipeline = pipeline;
    }

    auto VulkanRenderer::DrawScene( Scene *scene ) -> void {
        // Handle lights here which are also the same for all passses
        auto& registry{ scene->GetRegistry() };
        auto lightsView{ registry.view<TagComponent, TransformComponent, LightComponent>() };
        for (auto& lightEntity : lightsView) {
            TagComponent& tagComp{ registry.get<TagComponent>( lightEntity ) };
            LightComponent& lightComp{ registry.get<LightComponent>( lightEntity ) };
            TransformComponent& transformCom{ registry.get<TransformComponent>( lightEntity ) };
        }

        for ( const auto& pass : m_Passes | std::ranges::views::values ) {
            pass->Execute();
        }

        // For all passes with a graphics pipeline
        for (auto& pass : m_Passes | std::ranges::views::values ) {
            if ( const auto graphicsPass{ dynamic_cast<IRenderPass*>(pass.get()) } ) {
                graphicsPass->Render( scene );
            }
        }
    }

    auto VulkanRenderer::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto VulkanRenderer::SetCamera( const Camera *camera ) -> void {
        FrameUBO frameUBO{};
        frameUBO.View = camera->GetViewMatrix();
        frameUBO.Projection = camera->GetProjection();

        m_FrameUBOBuffer->CopyFromBlock(std::addressof( frameUBO ), sizeof(FrameUBO));
    }

    auto VulkanRenderer::SetViewport( const float x, const float y, const float width, const float height ) -> void {
        VkCommandBuffer cmd{ *m_GraphicsCommandList->GetNativeHandle<VulkanCmdList>() };

        m_Viewport.x = x;
        m_Viewport.y = y;
        m_Viewport.width = width;
        m_Viewport.height = height;
        m_Viewport.minDepth = 0.0f;
        m_Viewport.maxDepth = 1.0f;

        m_Scissor.offset = { static_cast<Int32>( x ), static_cast<Int32>( y ) };
        m_Scissor.extent = { static_cast<UInt32>( width ), static_cast<UInt32>( height ) };

        vkCmdSetViewport( cmd, 0, 1, std::addressof( m_Viewport ) );
        vkCmdSetScissor( cmd, 0, 1, std::addressof( m_Scissor ) );
    }

    auto VulkanRenderer::CreateMaterial() -> MaterialHandle {
        return MaterialHandle::CreateEmpty();
    }

    auto VulkanRenderer::GetFinalComposition() const -> TextureHandle {
        using namespace Mikoto::VulkanPasses;

        TextureHandle handle{ TextureHandle::CreateEmpty() };
        if ( const ShadingPass * shadingPass{ m_Passes.Get<ShadingPass>() } ) {
            handle = shadingPass->GetFinalComposition();
        }

        return handle;
    }

    auto VulkanRenderer::RegisterTextureForRender( TextureHandle texture ) -> void {
        const auto vkTexture{ dynamic_cast<VulkanTexture*>(texture.GetRaw()) };

        const Int32 textureIndex{ static_cast<Int32>(m_BindlessTextures.size()) };
        vkTexture->SetTextureIndex(textureIndex);

        m_BindlessTextures.push_back(texture);
    }

    auto VulkanRenderer::GetMaxBindlessTextureCount() -> UInt32 {
        return 4096;
    }

    auto VulkanRenderer::CreateBindlessDescriptor() -> void {
        const UInt32 maxBindlessTextures{ GetMaxBindlessTextureCount() };

        // Descriptor pool ------------------------
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = maxBindlessTextures;

        // Flags required for bindless descriptor sets
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags =
            VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // needed for descriptor indexing
        poolInfo.maxSets = 1; // You’ll typically have one global bindless descriptor set
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        if (vkCreateDescriptorPool(VK_DEVICE(m_GraphicsDevice), &poolInfo, nullptr, &m_BindlessPool) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create bindless descriptor pool!");
        }

        // Descriptor layout ------------------------
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 1;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = maxBindlessTextures; // e.g. 4096
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorBindingFlags bindingFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = 1;
        bindingFlagsInfo.pBindingFlags = &bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        vkCreateDescriptorSetLayout(VK_DEVICE(m_GraphicsDevice), &layoutInfo, nullptr, &m_BindlessDescriptorSetLayout);

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{};
        variableCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        const UInt32 counts[]{ maxBindlessTextures };
        variableCountInfo.descriptorSetCount = 1;
        variableCountInfo.pDescriptorCounts = counts;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_BindlessPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_BindlessDescriptorSetLayout;
        allocInfo.pNext = &variableCountInfo;

        if (vkAllocateDescriptorSets(VK_DEVICE(m_GraphicsDevice), &allocInfo, &m_BindlessDescriptorSet) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to allocate bindless descriptor set!");
        }

    }

    auto VulkanRenderer::UpdateBindlessTextureDescriptor( const Int32 index, VulkanTexture *texture ) -> void {
        if ( !texture->HasSampler() ) {
            texture->SetSampler( m_GraphicsDevice->CreateSampler( SamplerDescription{} ) );
        }

        VkSampler sampler{ *texture->GetSampler()->GetNativeHandle<VulkanSampler>() };
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = *texture->GetView();

        // is this the image's layout or the layout i want to to be for optimal sampling
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_BindlessDescriptorSet;
        write.dstBinding = 1;// textures[] binding
        write.dstArrayElement = index;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets( VK_DEVICE( m_GraphicsDevice ), 1, &write, 0, nullptr );
    }

    auto VulkanRenderer::InitCoreRenderPasses() -> void {
        using namespace Mikoto::VulkanPasses;

        // Final composition
        ShadingPass* shadingPass{ m_Passes.Register<ShadingPass>() };
        shadingPass->Init( TO_VK_DEVICE( m_GraphicsDevice ) );

        // Compute basic
        ComputeBasic* computeBasic{ m_Passes.Register<ComputeBasic>() };
        computeBasic->Init( TO_VK_DEVICE( m_GraphicsDevice ) );
    }

}// namespace Mikoto