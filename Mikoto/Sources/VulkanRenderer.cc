/**
 * VulkanRenderer.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <array>

// Third-Party Libraries
#include <volk.h>

#include <Core/Profiler.hh>

#include <Scene/Component.hh>

#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Vulkan/VulkanPasses.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {

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
        MKT_BEGIN_PROFILER_NAMED();

        InitCoreRenderPasses();

        CreateBindlessDescriptor();

        InitGlobalShaderBuffers();

        m_LightsInfo = CreateScope<LightInfo>();

        m_IsInitialized = true;
    }

    auto VulkanRenderer::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_IsInitialized ) {
            return;
        }

        m_LightsInfo.reset();

        m_Passes.Clear();

        m_BindlessTextures.clear();

        m_IsInitialized = false;
    }

    auto VulkanRenderer::EndRender() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_GraphicsCommandList->End();
    }

    auto VulkanRenderer::BeginRender( const CommandListHandle cmd ) -> void {
        using namespace Mikoto::VulkanPasses;

        MKT_BEGIN_PROFILER_NAMED();

        // Compute workflow first
        RunComputeWorkflow();

        // Prepare resources
        if ( m_UpdateTextureDescriptor ) {


            // We push all the textures we need, when we begin render
            // we make sure they are all visible through the descriptor set
            // do this once and only when is needed if the texture list hasn't changed
            // there's no need to update this descriptor set
            for ( TextureHandle& texture: m_BindlessTextures ) {

                const auto vkTexture{ dynamic_cast<VulkanTexture*>( texture.GetRaw() ) };

                const Int32 textureIndex{ vkTexture->GetTextureIndex() };
                UpdateBindlessTextureDescriptor( textureIndex, vkTexture );

                // TODO: temporary for debugging only. Here we update all textures in material pass
                TextureRenderPass* textureRenderPass{ m_Passes.Get<TextureRenderPass>() };
                textureRenderPass->RegisterTextureForRender( texture );
            }

            m_UpdateTextureDescriptor = false;
        }

        // Graphics commands
        m_GraphicsCommandList = cmd;
        m_GraphicsCommandList->Begin();
    }

    auto VulkanRenderer::DrawScene( Scene* scene ) -> void {
        using namespace Mikoto::VulkanPasses;

        MKT_BEGIN_PROFILER_NAMED();

        // Handle lights here which are also the same for all passes
        auto& registry{ scene->GetRegistry() };
        auto lightsView{ registry.view<TagComponent, TransformComponent, LightComponent>() };

        Int32 pointLightCount{};
        Int32 spotLightCount{};
        Int32 directionalLightCount{};

        for ( auto& lightEntity: lightsView ) {
            LightComponent& lightComp{ registry.get<LightComponent>( lightEntity ) };
            TransformComponent& transformCom{ registry.get<TransformComponent>( lightEntity ) };

            switch ( lightComp.GetActiveType() ) {
                case LightType::POINT_LIGHT_TYPE: {
                    if ( pointLightCount >= MAX_LIGHTS ) break;

                    auto& point{ lightComp.Get<PointLight>() };
                    auto& uboLight{ m_LightsInfo->PointLights[pointLightCount] };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Ambient = Vec4F( point.GetColor() * 0.1f, 1.0f );
                    uboLight.Diffuse = Vec4F( point.GetColor(), 0.0f );
                    uboLight.AttenuationParams = Vec4F( point.GetIntensity(), point.GetRadius(), 0.0f, 0.0f );

                    ++pointLightCount;
                    break;
                }

                case LightType::SPOT_LIGHT_TYPE: {
                    if ( spotLightCount >= MAX_LIGHTS ) break;

                    auto& spot{ lightComp.Get<SpotLight>() };
                    auto& uboLight{ m_LightsInfo->SpotLights[spotLightCount] };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Direction = Vec4F( spot.GetDirection(), 0.0f );
                    uboLight.Ambient = Vec4F( spot.GetColor() * 0.1f, 1.0f );
                    uboLight.Diffuse = Vec4F( spot.GetColor() * spot.GetIntensity(), 1.0f );
                    uboLight.CutOffValues = Vec4F( spot.GetCutOff(), spot.GetOuterCutOff(), spot.GetIntensity(), spot.GetRadius() );

                    ++spotLightCount;
                    break;
                }

                case LightType::DIRECTIONAL_LIGHT_TYPE: {
                    if ( directionalLightCount >= MAX_LIGHTS ) break;

                    auto& dir{ lightComp.Get<DirectionalLight>() };
                    auto& uboLight{ m_LightsInfo->DirectionalLights[directionalLightCount] };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );// optional for shadows
                    uboLight.Ambient = Vec4F( dir.GetColor() * 0.1f, 1.0f );
                    uboLight.Diffuse = Vec4F( dir.GetColor() * dir.GetIntensity(), 1.0f );

                    ++directionalLightCount;
                    break;
                }
            }
        }

        // Update counts in UBO
        m_LightsInfo->PointLightCount = pointLightCount;
        m_LightsInfo->SpotLightCount = spotLightCount;
        m_LightsInfo->DirectionalLightCount = directionalLightCount;

        m_LightsInfo->DisplayMode = static_cast<Int32>(LightInfo::DisplayModes::DISPLAY_COLOR);

        // Copy to GPU buffer
        m_LightsBuffer->CopyFromBlock( m_LightsInfo.get(), sizeof( LightInfo ) );

        TextureRenderPass* textureRenderPass{ m_Passes.Get<TextureRenderPass>() };
        textureRenderPass->Begin( m_GraphicsCommandList );

        textureRenderPass->Render( scene );

        textureRenderPass->End();

        // Shading pass will bind the global renderer descriptor pass
        // this pass is the main pass. These the lights and the texture sets are not changing between passes,
        // shaders must declare them properly to align to this descriptor layout
        // These descriptors go to set 0
        ShadingPass* shadingPass{ m_Passes.Get<ShadingPass>() };
        shadingPass->BindDefaultSets( m_GraphicsCommandList, m_FrameSet, 0 );
        shadingPass->BindDefaultSets( m_GraphicsCommandList, m_TexturesSet, 1 );

        shadingPass->Begin( m_GraphicsCommandList );

        shadingPass->Render( scene );

        shadingPass->End();

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

    auto VulkanRenderer::SetClearColor( float r, float g, float b, float a ) -> void {
        using namespace Mikoto::VulkanPasses;

        MKT_BEGIN_PROFILER_NAMED();

        ShadingPass* shadingPass{ m_Passes.Get<ShadingPass>() };
        shadingPass->SetClearColor( Vec4F{ r, g, b, a } );
    }

    auto VulkanRenderer::GetFinalComposition() const -> TextureHandle {
        using namespace Mikoto::VulkanPasses;

        TextureHandle handle{ TextureHandle::CreateEmpty() };
        if ( const ShadingPass * shadingPass{ m_Passes.Get<ShadingPass>() } ) {
            handle = shadingPass->GetFinalComposition();
        }

        return handle;
    }

    auto VulkanRenderer::GetMaterialPreview() const -> TextureHandle {
        using namespace Mikoto::VulkanPasses;

        TextureHandle handle{ TextureHandle::CreateEmpty() };
        if ( const TextureRenderPass * textureRenderPass{ m_Passes.Get<TextureRenderPass>() } ) {
            handle = textureRenderPass->GetFinalComposition();
        }

        return handle;
    }

    auto VulkanRenderer::SetMaterialPreviewMat(MaterialHandle material) -> void {
        using namespace Mikoto::VulkanPasses;

        if ( TextureRenderPass* textureRenderPass{ m_Passes.Get<TextureRenderPass>() } ) {
            textureRenderPass->SetMaterialPreviewMat(material);
        }
    }

    auto VulkanRenderer::SetMaterialPreviewViewport( float width, float height ) -> void {
        using namespace Mikoto::VulkanPasses;

        if ( TextureRenderPass * textureRenderPass{ m_Passes.Get<TextureRenderPass>() } ) {
            textureRenderPass->SetMaterialPreviewViewport(width, height);
        }
    }

    auto VulkanRenderer::RegisterTextureForRender( TextureHandle texture ) -> void {
        using namespace Mikoto::VulkanPasses;

        MKT_BEGIN_PROFILER_NAMED();

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

        descriptorWriter.WriteBuffer( 0, m_FrameUBOBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), m_FrameUBOBuffer->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
                .WriteBuffer( 1, m_LightsBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), m_LightsBuffer->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
                .UpdateSet( VK_DEVICE( m_GraphicsDevice ), m_FrameSet );
    }

    auto VulkanRenderer::CreateBindlessDescriptor() -> void {
        const UInt32 maxBindlessTextures{ GetMaxBindlessTextureCount() };

        // We will be using the descriptor set layout from the shading pass it is in this pass that we use the bindless textures
        PipelineHandle shadingPassPipeline{ m_Passes.Get<Mikoto::VulkanPasses::ShadingPass>()->GetPipeline() };
        VulkanGraphicsPipeline* vkPipeline{ dynamic_cast<VulkanGraphicsPipeline*>( shadingPassPipeline.GetRaw() ) };

        // ───────────────────────────────────────────────
        // [ Set = 0 ] Frame + Light UBOs
        // ───────────────────────────────────────────────
        const VkDescriptorSetLayout& layoutFrame{ vkPipeline->GetDescriptorSetLayout( 0 ) };
        m_FrameSet = TO_VK_DEVICE( m_GraphicsDevice )->AllocateDescriptorSet( &layoutFrame );

        // ───────────────────────────────────────────────
        // [ Set = 1 ] Bindless textures
        // ───────────────────────────────────────────────
        std::array<UInt32, 1> variableCount{ maxBindlessTextures };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableCountInfo.descriptorSetCount = static_cast<UInt32>( variableCount.size() );
        variableCountInfo.pDescriptorCounts = variableCount.data();

        const VkDescriptorSetLayout& layoutTextures{ vkPipeline->GetDescriptorSetLayout( 1 ) };
        m_TexturesSet = TO_VK_DEVICE( m_GraphicsDevice )->AllocateDescriptorSet( &layoutTextures, std::addressof( variableCountInfo ) );
    }

    auto VulkanRenderer::UpdateBindlessTextureDescriptor( const Int32 index, VulkanTexture* texture ) -> void {
        using namespace Mikoto::VulkanPasses;

        if ( !texture->HasSampler() ) {
            texture->SetSampler( m_GraphicsDevice->CreateSampler( SamplerDescription{} ) );
        }

        VkSampler sampler{ texture->GetSampler()->GetNativeHandle( ObjectType::Vk_Sampler ) };

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = texture->GetNativeHandle( ObjectType::Vk_ImageView );

        // is this the image's layout or the layout I want to be for optimal sampling
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

        // Final composition
        TextureRenderPass* textureRenderPass{ m_Passes.Register<TextureRenderPass>() };
        textureRenderPass->Init( TO_VK_DEVICE( m_GraphicsDevice ) );

        // Compute basic
        ComputeBasic* computeBasic{ m_Passes.Register<ComputeBasic>() };
        computeBasic->Init( TO_VK_DEVICE( m_GraphicsDevice ) );
    }

    auto VulkanRenderer::RunComputeWorkflow() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        CommandListHandle computeCommandList{ m_GraphicsDevice->CreateCommandList( QueueType::COMPUTE_QUEUE ) };
        computeCommandList->Begin();

        for ( const auto& pass: m_Passes | std::ranges::views::values ) {
            if ( const auto computePass{ dynamic_cast<IComputePass*>( pass.get() ) } ) {
                computePass->Begin( computeCommandList );
            }
        }

        // TODO: Sort execution order based on dependencies
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