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

        m_IsInitialized = true;
    }

    auto VulkanRenderer::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_IsInitialized ) {
            return;
        }

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

        // Shading pass will bind the global renderer descriptor pass
        // this pass is the main pass. These the lights and the texture sets are not changing between passes,
        // shaders must declare them properly to align to this descriptor layout
        // These descriptors go to set 0
        ShadingPass* shadingPass{ m_Passes.Get<ShadingPass>() };

        // Bind once bindless textures (uses pipeline layout at set index 0 as shading pass)
        const VkCommandBuffer vkCmd{ m_GraphicsCommandList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        const VkPipelineLayout pipeline{ shadingPass->GetPipeline()->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

        vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, 0, 1, std::addressof( m_TexturesSet ), 0, nullptr );

        shadingPass->Begin( m_GraphicsCommandList );
        shadingPass->Render( scene );
        shadingPass->End();

        TextureRenderPass* textureRenderPass{ m_Passes.Get<TextureRenderPass>() };
        textureRenderPass->Begin( m_GraphicsCommandList );
        textureRenderPass->Render( scene );
        textureRenderPass->End();

        FontRenderPass* fontRenderPass{ m_Passes.Get<FontRenderPass>() };
        fontRenderPass->Begin( m_GraphicsCommandList );
        fontRenderPass->Render( scene );
        fontRenderPass->End();

    }

    auto VulkanRenderer::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto VulkanRenderer::SetCamera( const Camera* camera ) -> void {
        using namespace Mikoto::VulkanPasses;

        MKT_BEGIN_PROFILER_NAMED();

        // Shading pass will bind the global renderer descriptor pass
        // this pass is the main pass. These the lights and the texture sets are not changing between passes,
        // shaders must declare them properly to align to this descriptor layout
        // These descriptors go to set 0
        ShadingPass* shadingPass{ m_Passes.Get<ShadingPass>() };
        shadingPass->SetCamera( camera );
    }

    auto VulkanRenderer::SetViewport( const float x, const float y, const float width, const float height ) -> void {
        VkCommandBuffer cmd{ m_GraphicsCommandList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        m_Viewport.x = x;
        m_Viewport.y = height;
        m_Viewport.width = width;
        m_Viewport.height = -height;
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

    auto VulkanRenderer::GetFontPassComposition() -> TextureHandle {
        using namespace Mikoto::VulkanPasses;

        FontRenderPass * fontRenderPass{ m_Passes.Get<FontRenderPass>() };
        return fontRenderPass->GetFinalComposition();
    }

    auto VulkanRenderer::CreateBindlessDescriptor() -> void {
        const UInt32 maxBindlessTextures{ GetMaxBindlessTextureCount() };

        // We will be using the descriptor set layout from the shading pass it is in this pass that we use the bindless textures
        PipelineHandle shadingPassPipeline{ m_Passes.Get<Mikoto::VulkanPasses::ShadingPass>()->GetPipeline() };
        VulkanGraphicsPipeline* vkPipeline{ dynamic_cast<VulkanGraphicsPipeline*>( shadingPassPipeline.GetRaw() ) };

        // ───────────────────────────────────────────────
        // [ Set = 0 ] Bindless textures
        // ───────────────────────────────────────────────
        std::array<UInt32, 1> variableCount{ maxBindlessTextures };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableCountInfo.descriptorSetCount = static_cast<UInt32>( variableCount.size() );
        variableCountInfo.pDescriptorCounts = variableCount.data();

        const VkDescriptorSetLayout& layoutTextures{ vkPipeline->GetDescriptorSetLayout( 0 ) };
        m_TexturesSet = TO_VK_DEVICE( m_GraphicsDevice )->AllocateDescriptorSet( &layoutTextures, std::addressof( variableCountInfo ) );
    }

    auto VulkanRenderer::UpdateBindlessTextureDescriptor( const Int32 index, VulkanTexture* texture ) const -> void {
        using namespace Mikoto::VulkanPasses;

        if ( !texture->HasSampler() ) {
            texture->SetSampler( m_GraphicsDevice->CreateSampler( SamplerDescription{} ) );
        }

        VkSampler sampler{ texture->GetSampler()->GetNativeHandle( ObjectType::Vk_Sampler ) };
        VkImageView image{ texture->GetNativeHandle( ObjectType::Vk_ImageView ) };

        DescriptorWriter writer{};
        writer.WriteImage( 0, image, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, index )
            .UpdateSet( VK_DEVICE( m_GraphicsDevice ), m_TexturesSet );
    }

    auto VulkanRenderer::InitCoreRenderPasses() -> void {
        using namespace Mikoto::VulkanPasses;

        // Final composition
        ShadingPass* shadingPass{ m_Passes.Register<ShadingPass>() };
        shadingPass->Init( TO_VK_DEVICE( m_GraphicsDevice ) );

        // Texture composition
        TextureRenderPass* textureRenderPass{ m_Passes.Register<TextureRenderPass>() };
        textureRenderPass->Init( TO_VK_DEVICE( m_GraphicsDevice ) );

        // Font composition
        FontRenderPass* fontRenderPass{ m_Passes.Register<FontRenderPass>() };
        fontRenderPass->Init( TO_VK_DEVICE( m_GraphicsDevice ) );

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