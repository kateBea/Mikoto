/**
 * VulkanRenderer.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <array>

// Third-Party Libraries
#include <volk.h>

#include <Renderer/Light.hh>
#include <Renderer/Vulkan/VulkanPasses.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Scene/Component.hh>

// TODO: temporary matches pbr_instance shaders layout
#define MAX_LIGHTS 50

namespace Mikoto {

    struct FrameUBO {
        glm::mat4 View{};
        glm::mat4 Projection{};
        Vec4F CameraPosition{};
    };

    struct SpotLightShader {
        Vec4F Position;
        Vec4F Direction;
        Vec4F Ambient;
        Vec4F Diffuse;
        Vec4F Specular;
        // x=cutOff, y=outerCutOff, z=intensity, w=radius
        Vec4F CutOffValues;
    };

    struct PointLightShader {
        Vec4F Position;
        Vec4F ambient;
        Vec4F Diffuse;
        Vec4F specular;
        Vec4F AttenuationParams;
    };

    struct DirectionalLightShader {
        Vec4F Position;
        Vec4F Ambient;
        Vec4F Diffuse;
        Vec4F Specular;
    };


    struct LightInfo {
        SpotLight SpotLights[MAX_LIGHTS];
        PointLightShader PointLights[MAX_LIGHTS];
        DirectionalLightShader DirectionalLights[MAX_LIGHTS];
        Int32 DirectionalLightCount{};
        Int32 PointLightCount{};
        Int32 SpotLightCount{};
        Int32 DisplayMode{};
        Int32 Wireframe{};
    };

    template<typename UniformBufferT>
    static auto CreateUniformBuffer( GpuDevice* device ) -> BufferHandle {
        BufferHandle buffer{ BufferHandle::CreateEmpty() };

        constexpr Size elementCount{ 1 };
        constexpr Size elementSize{ sizeof( UniformBufferT ) };

        // UniformBuffer size padded. Vertex shader
        const VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE( device )->GetUniformBufferMinOffsetAlignment() };
        const VkDeviceSize paddedSize{ VulkanHelpers::GetUniformBufferPadding( elementSize, minOffsetAlignment ) };

        const Size totalSize{ elementCount * paddedSize };

        BufferDescription bufferDesc{};
        bufferDesc.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithSizeBytes( totalSize )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STREAM );
        buffer = device->CreateBuffer( bufferDesc );

        return buffer;
    }

    VulkanRenderer::VulkanRenderer( GpuDevice* device, const std::string_view name )
        : RendererBackend{ { name, device } } {}

    auto VulkanRenderer::Init() -> void {

        CreateBindlessDescriptor();

        InitGlobalShaderBuffers();

        m_Materials.Init( 10 );

        InitCoreRenderPasses();

        m_IsInitialized = true;
    }

    auto VulkanRenderer::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        m_Passes.Clear();

        m_Materials.Shutdown();

        m_BindlessTextures.clear();

        m_IsInitialized = false;
    }

    auto VulkanRenderer::EndRender() -> void {
        for ( const auto& pass: m_Passes | std::ranges::views::values ) {

            // This is only for the render commands which share the same command buffer
            if ( const auto graphicsPass{ dynamic_cast<IRenderPass*>( pass.get() ) } ) {
                graphicsPass->End( );
            }
        }

        m_GraphicsCommandList->End();
    }

    auto VulkanRenderer::BeginRender( const CommandListHandle cmd ) -> void {
        using namespace Mikoto::VulkanPasses;

        // Compute workflow first
        RunComputeWorkflow();

        // Graphics commands
        m_GraphicsCommandList = cmd;
        m_GraphicsCommandList->Begin();

        // Prepare resources
        if (m_UpdateTextureDescriptor) {
            // We push all the textures we need, when we begin render
            // we make sure they are all visible through the descriptor set
            // do this once and only when is needed if the texture list hasn't changed
            // there's no need to update this descriptor set
            for ( TextureHandle& texture: m_BindlessTextures ) {

                const auto vkTexture{ dynamic_cast<VulkanTexture*>( texture.GetRaw() ) };

                const Int32 textureIndex{ vkTexture->GetTextureIndex() };
                UpdateBindlessTextureDescriptor( textureIndex, vkTexture );
            }

            m_UpdateTextureDescriptor = false;
        }

        for ( const auto& pass: m_Passes | std::ranges::views::values ) {
            if ( const auto graphicsPass{ dynamic_cast<IRenderPass*>( pass.get() ) } ) {
                graphicsPass->Begin( m_GraphicsCommandList );
            }
        }

        // Shading pass will bind the global renderer descriptor pass
        // this pass is the main pass. These the lights and the texture sets are not changing between passes,
        // shaders must declare them properly to align to this descriptor layout
        // These descriptors go to set 0
        ShadingPass* shadingPass{ m_Passes.Get<ShadingPass>() };
        shadingPass->BindDefaultSets( m_FrameSet, 0 );
        shadingPass->BindDefaultSets( m_TexturesSet, 1 );
    }

    auto VulkanRenderer::DrawScene( Scene* scene ) -> void {
        // Handle lights here which are also the same for all passes
        auto& registry{ scene->GetRegistry() };
        auto lightsView{ registry.view<TagComponent, TransformComponent, LightComponent>() };
        for ( auto& lightEntity: lightsView ) {
            TagComponent& tagComp{ registry.get<TagComponent>( lightEntity ) };
            LightComponent& lightComp{ registry.get<LightComponent>( lightEntity ) };
            TransformComponent& transformCom{ registry.get<TransformComponent>( lightEntity ) };
        }

        // For all passes with a graphics pipeline
        for ( auto& pass: m_Passes | std::ranges::views::values ) {
            if ( const auto graphicsPass{ dynamic_cast<IRenderPass*>( pass.get() ) } ) {
                graphicsPass->Render( scene );
            }
        }
    }

    auto VulkanRenderer::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto VulkanRenderer::SetCamera( const Camera* camera ) -> void {
        FrameUBO frameUBO{};
        frameUBO.View = camera->GetViewMatrix();
        frameUBO.Projection = camera->GetProjection();

        m_FrameUBOBuffer->CopyFromBlock( std::addressof( frameUBO ), sizeof( FrameUBO ) );
    }

    auto VulkanRenderer::SetViewport( const float x, const float y, const float width, const float height ) -> void {
        VkCommandBuffer cmd{ m_GraphicsCommandList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

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
        MaterialHandle material{ m_Materials.Allocate(  ) };
        if (material.IsEmpty()) {
            MKT_CORE_LOGGER_ERROR( "VulkanRenderer::CreateMaterial - Failed to create material" );
        } else {
            PBRMaterial* pbrMat{ dynamic_cast<PBRMaterial*>(material.GetRaw()) };

            pbrMat->SetTextureType( MapType::ALBEDO_TEXTURE, m_GraphicsDevice->GetDummyTexture() );
            pbrMat->SetTextureType( MapType::NORMAL_TEXTURE, m_GraphicsDevice->GetDummyTexture() );
            pbrMat->SetTextureType( MapType::EMISSIVE_TEXTURE, m_GraphicsDevice->GetDummyTexture() );
            pbrMat->SetTextureType( MapType::METALLIC_TEXTURE, m_GraphicsDevice->GetDummyTexture() );
            pbrMat->SetTextureType( MapType::ROUGHNESS_TEXTURE, m_GraphicsDevice->GetDummyTexture() );
            pbrMat->SetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE, m_GraphicsDevice->GetDummyTexture() );
        }

        return material;
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
        const auto vkTexture{ dynamic_cast<VulkanTexture*>( texture.GetRaw() ) };

        const Int32 textureIndex{ static_cast<Int32>( m_BindlessTextures.size() ) };
        vkTexture->SetTextureIndex( textureIndex );

        m_BindlessTextures.push_back( texture );

        m_UpdateTextureDescriptor = true;
    }

    auto VulkanRenderer::GetMaxBindlessTextureCount() -> UInt32 {
        return 4096;
    }

    auto VulkanRenderer::InitGlobalShaderBuffers() -> void {
        m_FrameUBOBuffer = CreateUniformBuffer<FrameUBO>( m_GraphicsDevice );
        m_LightsBuffer = CreateUniformBuffer<LightInfo>( m_GraphicsDevice );


        DescriptorWriter descriptorWriter{};

        descriptorWriter.WriteBuffer( 0, m_FrameUBOBuffer->GetNativeHandle(ObjectType::Vk_Buffer), m_FrameUBOBuffer->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .WriteBuffer( 1, m_LightsBuffer->GetNativeHandle(ObjectType::Vk_Buffer), m_LightsBuffer->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .UpdateSet( VK_DEVICE(m_GraphicsDevice), m_FrameSet );
    }

    auto VulkanRenderer::CreateBindlessDescriptor() -> void {
        const UInt32 maxBindlessTextures{ GetMaxBindlessTextureCount() };

        // ───────────────────────────────────────────────
        // [ Set = 0 ] Frame + Light UBOs
        // ───────────────────────────────────────────────
        std::array<VkDescriptorSetLayoutBinding, 2> frameBindings{};
        frameBindings[0].binding = 0;
        frameBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        frameBindings[0].descriptorCount = 1;
        frameBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        frameBindings[1].binding = 1;
        frameBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        frameBindings[1].descriptorCount = 1;
        frameBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo frameLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        frameLayoutInfo.bindingCount = static_cast<uint32_t>( frameBindings.size() );
        frameLayoutInfo.pBindings = frameBindings.data();

        m_FrameLayout = TO_VK_DEVICE( m_GraphicsDevice )->AllocateDescriptorSetLayout( frameLayoutInfo );

        // Set = 0 → Frame + Light
        m_FrameSet = TO_VK_DEVICE( m_GraphicsDevice )->AllocateDescriptorSet( m_FrameLayout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ) );

        // ───────────────────────────────────────────────
        // [ Set = 1 ] Bindless textures
        // ───────────────────────────────────────────────
        VkDescriptorSetLayoutBinding textureBinding{};
        textureBinding.binding = 0;// matches layout(set=1, binding=0)
        textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureBinding.descriptorCount = maxBindlessTextures;
        textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorBindingFlags textureFlags =
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo textureFlagsInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
        textureFlagsInfo.bindingCount = 1;
        textureFlagsInfo.pBindingFlags = &textureFlags;

        VkDescriptorSetLayoutCreateInfo textureLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        textureLayoutInfo.pNext = &textureFlagsInfo;
        textureLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        textureLayoutInfo.bindingCount = 1;
        textureLayoutInfo.pBindings = &textureBinding;

        m_TextureLayout = TO_VK_DEVICE( m_GraphicsDevice )->AllocateDescriptorSetLayout( textureLayoutInfo );

        // Set = 1 → Bindless textures
        const UInt32 variableCount[] = { maxBindlessTextures };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableCountInfo.descriptorSetCount = 1;
        variableCountInfo.pDescriptorCounts = variableCount;

        m_TexturesSet = TO_VK_DEVICE( m_GraphicsDevice )->AllocateDescriptorSet( m_TextureLayout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ), std::addressof( variableCountInfo ) );
    }

    auto VulkanRenderer::UpdateBindlessTextureDescriptor( const Int32 index, VulkanTexture* texture ) -> void {
        if ( !texture->HasSampler() ) {
            texture->SetSampler( m_GraphicsDevice->CreateSampler( SamplerDescription{} ) );
        }

        VkSampler sampler{ texture->GetSampler()->GetNativeHandle( ObjectType::Vk_Sampler ) };
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = texture->GetNativeHandle( ObjectType::Vk_ImageView );

        // is this the image's layout or the layout i want to to be for optimal sampling
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_TexturesSet;
        write.dstBinding = 0;
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

    auto VulkanRenderer::RunComputeWorkflow()-> void{
        CommandListHandle computeCommandList{ m_GraphicsDevice->CreateCommandList( QueueType::COMPUTE_QUEUE ) };
        computeCommandList->Begin();

        for ( const auto& pass: m_Passes | std::ranges::views::values ) {
            if ( const auto computePass{ dynamic_cast<IComputePass*>( pass.get() ) } ) {
                computePass->Begin(computeCommandList);
            }
        }

        for ( const auto& pass: m_Passes | std::ranges::views::values ) {
            if ( const auto computePass{ dynamic_cast<IComputePass*>( pass.get() ) } ) {
                computePass->Execute();
            }
        }

        for ( const auto& pass: m_Passes | std::ranges::views::values ) {
            if ( const auto computePass{ dynamic_cast<IComputePass*>( pass.get() ) } ) {
                computePass->End();
            }
        }

        computeCommandList->End();
        m_GraphicsDevice->SubmitCommands( computeCommandList );
    }

}// namespace Mikoto