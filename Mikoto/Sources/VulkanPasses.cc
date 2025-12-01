//
// Created by kate on 10/23/25.
//

// C++ Standard Library
#include <array>
#include <cstdlib>
#include <ranges>

// Third-Party Libraries
#include <volk.h>

#include <Core/Profiler.hh>
#include <Material/PBRMaterial.hh>
#include <Material/ShaderLibrary.hh>
#include <Memory/Allocator.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Scene/Component.hh>

#include "Renderer/Vulkan/VulkanDevice.hh"
#include "Renderer/Vulkan/VulkanHelpers.hh"
#include "Renderer/Vulkan/VulkanPasses.hh"
#include "Renderer/Vulkan/VulkanRenderer.hh"
#include "Renderer/Vulkan/VulkanTexture.hh"

namespace Mikoto::VulkanPasses {

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

    static auto GetMeshTextureIndices( TextureHandle texture ) -> Int32 {
        if ( const auto vkTexture{ dynamic_cast<VulkanTexture*>( texture.GetRaw() ) } ) {
            return vkTexture->GetTextureIndex();
        }

        return -1;
    }

    auto ShadingPass::Init( GpuDevice* device ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Device = device;

        // Color attachment
        TextureDescription colorDesc{};
        colorDesc.WithWidth( 1920 )
                .WithHeight( 1080 )
                .WithChannelCount( 4 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::TEXTURE_USAGE_COLOR )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_ColorTarget.Image = m_Device->CreateTexture( colorDesc );
        m_ColorTarget.Image->SetDebugName( "ShadingPass Color Target" );
        m_ColorTarget.Type = AttachmentType::COLOR;

        // Depth attachment
        TextureDescription depthDesc{};
        depthDesc.WithWidth( 1920 )
                .WithHeight( 1080 )
                .WithChannelCount( 1 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::TEXTURE_USAGE_DEPTH )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_D32_FLOAT )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_DepthTarget.Image = m_Device->CreateTexture( depthDesc );
        m_DepthTarget.Image->SetDebugName( "ShadingPass Depth Target" );
        m_DepthTarget.Type = AttachmentType::DEPTH;

        // Graphics pipeline
        ShaderModuleHandle vertShader{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle fragShader{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT_STAGE ) };

        GraphicsPipelineDescription pipelineDesc{};
        pipelineDesc.ShaderStages = { vertShader, fragShader };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;
        pipelineDesc.DepthTexture = m_DepthTarget.Image;
        pipelineDesc.ColorAttachments = { m_ColorTarget.Image };

        // Input rate
        // Vertices
        AttributesSpec verticesData{
            .DefaultVertexLayout{ DEFAULT_VERTEX_BUFFER_LAYOUT },
            .InputRateSpec{ .BindingIndex{ 0 }, .AttributeRate{ InputRate::PER_VERTEX } }
        };

        // Attributes
        AttributesSpec instancedData{
            .DefaultVertexLayout{
                // Model matrix columns
                { ShaderDataType::FLOAT4_TYPE, "i_Model0" },// mat4 column 0
                { ShaderDataType::FLOAT4_TYPE, "i_Model1" },// mat4 column 1
                { ShaderDataType::FLOAT4_TYPE, "i_Model2" },// mat4 column 2
                { ShaderDataType::FLOAT4_TYPE, "i_Model3" },// mat4 column 3

                // Material properties
                { ShaderDataType::FLOAT4_TYPE, "i_Albedo" },
                { ShaderDataType::FLOAT4_TYPE, "i_Factors" },

                // Texture indices (flat ints)
                { ShaderDataType::INT_TYPE, "i_AlbedoIndex" },
                { ShaderDataType::INT_TYPE, "i_NormalIndex" },
                { ShaderDataType::INT_TYPE, "i_MetallicIndex" },
                { ShaderDataType::INT_TYPE, "i_RoughnessIndex" },
                { ShaderDataType::INT_TYPE, "i_AoIndex" } },
            .InputRateSpec{ .BindingIndex{ 1 }, .AttributeRate{ InputRate::PER_INSTANCE } }
        };

        pipelineDesc.VertexAttributesSpec = { verticesData, instancedData };

        m_Pipeline = m_Device->CreatePipeline( pipelineDesc );
    }

    auto ShadingPass::Shutdown() -> void {
        m_MeshInstanceData.clear();
    }

    auto ShadingPass::Begin( CommandListHandle cmd ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_CmdList = cmd;
        VkCommandBuffer vkCmd{ cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = m_ColorTarget.Image->GetNativeHandle( ObjectType::Vk_ImageView );
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a };

        std::array<VkRenderingAttachmentInfo, 1> colorAttachments{ colorAttachment };

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = m_DepthTarget.Image->GetNativeHandle( ObjectType::Vk_ImageView );
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = { { 0, 0 }, { 1920, 1080 } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<UInt32>( colorAttachments.size() );
        renderingInfo.pColorAttachments = colorAttachments.data();
        renderingInfo.pDepthAttachment = std::addressof( depthAttachment );

        vkCmdBeginRendering( vkCmd, &renderingInfo );

        vkCmdBindPipeline( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetNativeHandle( ObjectType::Vk_Pipeline ) );
    }

    void ShadingPass::End() {
        MKT_BEGIN_PROFILER_NAMED();

        const auto vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        vkCmdEndRendering( vkCmd );

        // Transition color target to shader read
        const auto tex{ dynamic_cast<VulkanTexture*>( m_ColorTarget.Image.GetRaw() ) };
        tex->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkCmd );
    }

    auto ShadingPass::WantStoreOP( const bool enable ) -> void {
        m_WantStoreOP = enable;
    }

    auto ShadingPass::SetClearColor( const glm::vec4& color ) -> void {
        m_ClearColor = color;
    }

    auto ShadingPass::GetFinalComposition() const -> TextureHandle {
        return m_ColorTarget.Image;
    }

    auto ShadingPass::UploadInstanceData() -> void {
        for ( auto& meshInfo: m_MeshInstanceData | std::views::values ) {
            std::vector<ShadingPassMeshBufferUBO> instancesData{};

            for ( auto& instanceData: meshInfo.second | std::views::values ) {
                instancesData.emplace_back( instanceData );
            }

            meshInfo.first->CopyFromBlock( instancesData.data(), instancesData.size() * sizeof( ShadingPassMeshBufferUBO ) );
        }
    }

    auto ShadingPass::UpdateMeshInstanceData() -> void {
        for ( auto& meshInfo: m_MeshInstanceData | std::views::values ) {
            std::vector<ShadingPassMeshBufferUBO> instancesData{};

            for ( auto& instanceData: meshInfo.second | std::views::values ) {
                instancesData.emplace_back( instanceData );
            }

            BufferDescription vertexDesc{};
            vertexDesc.WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
                    .WithData( reinterpret_cast<Byte*>( instancesData.data() ) )
                    .WithSizeBytes( InferSize<ShadingPassMeshBufferUBO>( instancesData.size() ) )
                    .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

            meshInfo.first = m_Device->CreateBuffer( vertexDesc );
        }
    }

    auto ShadingPass::Render( Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Build batches per mesh
        auto& registry{ scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComp{ registry.get<MeshComponent>( entity ) };
            auto& materialComp{ registry.get<MaterialComponent>( entity ) };

            MeshNode* meshNode{ meshComp.GetMesh() };
            PBRMaterial* pbrMat{ dynamic_cast<PBRMaterial*>( materialComp.GetMaterial().GetRaw() ) };

            if ( !meshNode || !pbrMat ) {
                continue;
            }

            // We need to update this vertex buffer if we don't have this mesh
            if ( !m_MeshInstanceData.contains( meshNode ) ) {
                m_UpdateInstanceData = true;
            }

            auto& [Mesh, Instances]{ m_MeshInstanceData[meshNode] };

            // We need to update this vertex buffer if we don't have its contents
            if ( !Instances.contains( tag.GetGUID() ) ) {
                m_UpdateInstanceData = true;
            }

            ShadingPassMeshBufferUBO& ubo{ Instances[tag.GetGUID()] };
            ubo.i_TransformCol0 = transform.GetTransform()[0];
            ubo.i_TransformCol1 = transform.GetTransform()[1];
            ubo.i_TransformCol2 = transform.GetTransform()[2];
            ubo.i_TransformCol3 = transform.GetTransform()[3];

            ubo.Albedo = pbrMat->GetColor();
            ubo.Factors.x = pbrMat->GetMetallicFactor();
            ubo.Factors.y = pbrMat->GetRoughnessFactor();

            ubo.AlbedoIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::ALBEDO_TEXTURE ) );
            ubo.NormalIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::NORMAL_TEXTURE ) );
            ubo.MetallicIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::METALLIC_TEXTURE ) );
            ubo.RoughnessIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
            ubo.AoIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
        }

        if ( m_UpdateInstanceData ) {
            UpdateMeshInstanceData();
            m_UpdateInstanceData = false;
        }

        // Copy contents
        UploadInstanceData();

        // Draw
        const VkCommandBuffer vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        for ( auto& [meshNode, instanceData]: m_MeshInstanceData ) {
            BufferHandle vertexBuffer{ meshNode->GetVertexBuffer() };
            BufferHandle indexBuffer{ meshNode->GetIndexBuffer() };

            const std::array<VkDeviceSize, 1> offsets{};
            const std::array<VkBuffer, 1> vertexBuffers{ vertexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ) };
            const std::array<VkBuffer, 1> instanceBuffers{ instanceData.first->GetNativeHandle( ObjectType::Vk_Buffer ) };

            vkCmdBindVertexBuffers( vkCmd, 0, 1, vertexBuffers.data(), offsets.data() );
            vkCmdBindVertexBuffers( vkCmd, 1, 1, instanceBuffers.data(), offsets.data() );

            vkCmdBindIndexBuffer( vkCmd, indexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), 0, VK_INDEX_TYPE_UINT32 );

            const Size instanceCount{ ( Size )instanceData.second.size() };
            vkCmdDrawIndexed( vkCmd, indexBuffer->GetCount(), instanceCount, 0, 0, 0 );
        }
    }

    auto ShadingPass::OnResize( UInt32 width, UInt32 height ) -> void {
        // TODO: resize color/depth targets
    }

    auto ShadingPass::BindDefaultSets( CommandListHandle cmd, VkDescriptorSet& set, const UInt32 setIndex ) -> void {
        const VkCommandBuffer vkCmd{ cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        const VkPipelineLayout pipeline{ m_Pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

        vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, setIndex, 1, std::addressof( set ), 0, nullptr );
    }

    auto TextureRenderPass::Init( GpuDevice* device ) -> void {
        m_Device = device;

        // Color attachment
        TextureDescription colorDesc{};
        colorDesc.WithWidth( 1920 )
                .WithHeight( 1080 )
                .WithChannelCount( 4 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::TEXTURE_USAGE_COLOR )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_ColorTarget.Image = m_Device->CreateTexture( colorDesc );
        m_ColorTarget.Image->SetDebugName( "TextureRenderPass Color Target" );
        m_ColorTarget.Type = AttachmentType::COLOR;

        // Depth attachment
        TextureDescription depthDesc{};
        depthDesc.WithWidth( 1920 )
                .WithHeight( 1080 )
                .WithChannelCount( 1 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::TEXTURE_USAGE_DEPTH )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_D32_FLOAT )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_DepthTarget.Image = m_Device->CreateTexture( depthDesc );
        m_DepthTarget.Image->SetDebugName( "TextureRenderPass Depth Target" );
        m_DepthTarget.Type = AttachmentType::DEPTH;

        // Graphics pipeline
        ShaderModuleHandle vertShader{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/FullscreenTriangle_Vert.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle fragShader{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/FullscreenTriangle_Frag.sprv", ShaderStage::FRAGMENT_STAGE ) };

        // we providing none as the shader expects none
        BufferLayout layout{};

        GraphicsPipelineDescription pipelineDesc{};
        pipelineDesc.ShaderStages = { vertShader, fragShader };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;
        pipelineDesc.PrimitiveTopology = Topology::TRIANGLE_STRIP;
        pipelineDesc.DepthTexture = m_DepthTarget.Image;
        pipelineDesc.ColorAttachments = { m_ColorTarget.Image };
        pipelineDesc.VertexAttributesSpec = {};

        m_Pipeline = m_Device->CreatePipeline( pipelineDesc );

        // ───────────────────────────────────────────────
        // [ Set = 0 ] Bindless textures
        // ───────────────────────────────────────────────
        const UInt32 maxBindlessTextures{ VulkanRenderer::GetMaxBindlessTextureCount() };
        std::array variableCount{ maxBindlessTextures };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableCountInfo.descriptorSetCount = static_cast<UInt32>( variableCount.size() );
        variableCountInfo.pDescriptorCounts = variableCount.data();

        VulkanGraphicsPipeline* vkPipeline{ dynamic_cast<VulkanGraphicsPipeline*>( m_Pipeline.GetRaw() ) };

        // Bindless set is at set 0, see FullscreenTriangle_Frag.glsl
        const VkDescriptorSetLayout& layoutTextures{ vkPipeline->GetDescriptorSetLayout( 0 ) };
        m_TexturesSet = TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( &layoutTextures, std::addressof( variableCountInfo ) );
    }

    auto TextureRenderPass::UpdateBindlessTextureDescriptor( Int32 index, VulkanTexture* texture ) const -> void {
        if ( !texture->HasSampler() ) {
            texture->SetSampler( m_Device->CreateSampler( SamplerDescription{} ) );
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

        vkUpdateDescriptorSets( VK_DEVICE( m_Device ), 1, &write, 0, nullptr );
    }

    auto TextureRenderPass::Shutdown() -> void {
    }

    auto TextureRenderPass::Begin( CommandListHandle cmd ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Prepare resources
        if ( m_UpdateTextureDescriptor ) {

            // We push all the textures we need, when we begin render
            // we make sure they are all visible through the descriptor set
            // do this once and only when is needed if the texture list hasn't changed
            // there's no need to update this descriptor set
            for ( TextureHandle& texture: m_BindlessTextures | std::ranges::views::values ) {

                const auto vkTexture{ dynamic_cast<VulkanTexture*>( texture.GetRaw() ) };

                const Int32 textureIndex{ vkTexture->GetTextureIndex() };
                UpdateBindlessTextureDescriptor( textureIndex, vkTexture );
            }

            m_UpdateTextureDescriptor = false;
        }

        m_CmdList = cmd;
        VkCommandBuffer vkCmd{ cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = m_ColorTarget.Image->GetNativeHandle( ObjectType::Vk_ImageView );
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a };

        std::array<VkRenderingAttachmentInfo, 1> colorAttachments{ colorAttachment };

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = m_DepthTarget.Image->GetNativeHandle( ObjectType::Vk_ImageView );
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = { { 0, 0 }, { 1920, 1080 } };
        renderingInfo.layerCount = 1;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<UInt32>( colorAttachments.size() );
        renderingInfo.pColorAttachments = colorAttachments.data();
        renderingInfo.pDepthAttachment = std::addressof( depthAttachment );

        const VkPipelineLayout pipeline{ m_Pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

        vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, 0, 1, std::addressof( m_TexturesSet ), 0, nullptr );

        vkCmdBeginRendering( vkCmd, &renderingInfo );

        vkCmdBindPipeline( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetNativeHandle( ObjectType::Vk_Pipeline ) );
    }

    auto TextureRenderPass::End() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const auto vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        vkCmdEndRendering( vkCmd );

        // Transition color target to shader read
        const auto tex{ dynamic_cast<VulkanTexture*>( m_ColorTarget.Image.GetRaw() ) };
        tex->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkCmd );
    }

    auto TextureRenderPass::Render( Scene* scene ) -> void {
        vkCmdDraw( m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
                   4,  // vertexCount (matches gl_VertexIndex range)
                   1,  // instanceCount
                   0,  // firstVertex
                   0 );// firstInstance
    }

    auto TextureRenderPass::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto TextureRenderPass::GetFinalComposition() const -> TextureHandle {
        return m_ColorTarget.Image;
    }

    auto TextureRenderPass::SetMaterialPreviewMat( MaterialHandle ref ) -> void {
        m_Material = ref;
    }

    auto TextureRenderPass::SetMaterialPreviewViewport( float width, float height ) -> void {
        m_Viewport.x = 0;
        m_Viewport.y = 0;
        m_Viewport.width = width;
        m_Viewport.height = height;
        m_Viewport.minDepth = 0.0f;
        m_Viewport.maxDepth = 1.0f;
    }

    auto TextureRenderPass::RegisterTextureForRender( TextureHandle texture ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_BindlessTextures.try_emplace( texture.GetRaw(), texture );
        m_UpdateTextureDescriptor = true;
    }

    auto FontRenderPass::Init( GpuDevice* device ) -> void {
        m_Device = device;

        // Color attachment
        TextureDescription colorDesc{};
        colorDesc.WithWidth( 1920 )
                .WithHeight( 1080 )
                .WithChannelCount( 4 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::TEXTURE_USAGE_COLOR )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_ColorTarget.Image = m_Device->CreateTexture( colorDesc );
        m_ColorTarget.Image->SetDebugName( "FontRenderPass Color Target" );
        m_ColorTarget.Type = AttachmentType::COLOR;

        // Depth attachment
        TextureDescription depthDesc{};
        depthDesc.WithWidth( 1920 )
                .WithHeight( 1080 )
                .WithChannelCount( 1 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::TEXTURE_USAGE_DEPTH )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_D32_FLOAT )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_DepthTarget.Image = m_Device->CreateTexture( depthDesc );
        m_DepthTarget.Image->SetDebugName( "FontRenderPass Depth Target" );
        m_DepthTarget.Type = AttachmentType::DEPTH;

        // Graphics pipeline
        ShaderModuleHandle vertShader{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/Text_Vert.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle fragShader{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/Text_Frag.sprv", ShaderStage::FRAGMENT_STAGE ) };

        GraphicsPipelineDescription pipelineDesc{};

        pipelineDesc.ShaderStages = { vertShader, fragShader };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;
        pipelineDesc.PrimitiveTopology = Topology::TRIANGLE_STRIP;
        pipelineDesc.DepthTexture = m_DepthTarget.Image;
        pipelineDesc.ColorAttachments = { m_ColorTarget.Image };

        // we're providing none as the shader expects none
        const BufferLayout layout{
                { ShaderDataType::FLOAT2_TYPE, "a_Position" },
                { ShaderDataType::FLOAT2_TYPE, "a_TexCoord" },
                { ShaderDataType::UINT_TYPE, "a_TexCoordIndex" },
            };
        pipelineDesc.VertexAttributesSpec = { AttributesSpec{
            .DefaultVertexLayout { layout },
            .InputRateSpec{ .BindingIndex = { 0 }, .AttributeRate { InputRate::PER_VERTEX} }
        } };

        m_Pipeline = m_Device->CreatePipeline( pipelineDesc );

        InitBuffersAndSets();

        CreateAttributeBuffers();
    }

    auto FontRenderPass::UpdateBindlessTextureDescriptor( Int32 index, VulkanTexture* texture ) const -> void {
        if ( !texture->HasSampler() ) {
            texture->SetSampler( m_Device->CreateSampler( SamplerDescription{} ) );
        }

        VkSampler sampler{ texture->GetSampler()->GetNativeHandle( ObjectType::Vk_Sampler ) };

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = texture->GetNativeHandle( ObjectType::Vk_ImageView );

        // is this the image's layout or the layout I want to be for optimal sampling
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_UBOSet;
        write.dstBinding = 1;
        write.dstArrayElement = index;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets( VK_DEVICE( m_Device ), 1, &write, 0, nullptr );
    }

    auto FontRenderPass::DrawText( glm::vec2 position, const std::string& text, double fontSize, Vec4F color ) -> void {
        double xPos = position.x;
        double yPos = position.y;
        double scale = fontSize / m_FontTest->GetSize();

        for ( size_t i = 0; i < text.length(); i++ ) {
            FontGlyph& glyph{ m_FontTest->GetGlyph( static_cast<uint32_t>( text[i] ) ) };

            if ( text[i] != ' ' ) {
                // Quad Coordinates
                double x0 = xPos + glyph.m_PlaneBounds.x * fontSize;
                double y0 = yPos - glyph.m_PlaneBounds.y * fontSize;

                // UV Coordinates
                TextureHandle atlas{ m_FontTest->GetAtlas() };
                double s0 = glyph.m_AtlasBounds.x / atlas->GetWidth();
                double t0 = glyph.m_AtlasBounds.w / atlas->GetHeight();
                double s1 = glyph.m_AtlasBounds.z / atlas->GetWidth();
                double t1 = glyph.m_AtlasBounds.y / atlas->GetHeight();

                FontParams fontParams{};
                fontParams.Pos = { x0, y0 + std::round( ( m_FontTest->GetMaxHeight() * scale ) ) - ( glyph.m_Height * scale ) };
                fontParams.Size = { glyph.m_Width * scale, glyph.m_Height * scale };
                fontParams.Color = color;
                fontParams.TexIndex = dynamic_cast<VulkanTexture*>( atlas.GetRaw() )->GetTextureIndex();
                fontParams.TexCoords[0] = { s0, t0 };// top left
                fontParams.TexCoords[1] = { s1, t0 };// bottom left
                fontParams.TexCoords[2] = { s1, t1 };// bottom right
                fontParams.TexCoords[3] = { s0, t1 };// top right

                m_FontRenderParams.emplace_back( fontParams );
            }

            xPos += glyph.m_AdvanceX * fontSize;
        }
    }

    auto FontRenderPass::Shutdown() -> void {
    }

    auto FontRenderPass::Begin( CommandListHandle cmd ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Prepare resources
        if ( m_UpdateTextureDescriptor ) {

            // We push all the textures we need, when we begin render
            // we make sure they are all visible through the descriptor set
            // do this once and only when is needed if the texture list hasn't changed
            // there's no need to update this descriptor set
            for ( TextureHandle& texture: m_BindlessTextures | std::ranges::views::values ) {

                const auto vkTexture{ dynamic_cast<VulkanTexture*>( texture.GetRaw() ) };

                const Int32 textureIndex{ vkTexture->GetTextureIndex() };
                UpdateBindlessTextureDescriptor( textureIndex, vkTexture );
            }

            DescriptorWriter descriptorWriterUBO{};
            descriptorWriterUBO.WriteBuffer( 0, m_UBO->GetNativeHandle( ObjectType::Vk_Buffer ), m_UBO->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
                    .UpdateSet( VK_DEVICE( m_Device ), m_UBOSet );

            m_UpdateTextureDescriptor = false;
        }

        m_CmdList = cmd;
        VkCommandBuffer vkCmd{ cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = m_ColorTarget.Image->GetNativeHandle( ObjectType::Vk_ImageView );
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a };

        std::array<VkRenderingAttachmentInfo, 1> colorAttachments{ colorAttachment };

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = m_DepthTarget.Image->GetNativeHandle( ObjectType::Vk_ImageView );
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = { { 0, 0 }, { 1920, 1080 } };
        renderingInfo.layerCount = 1;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<UInt32>( colorAttachments.size() );
        renderingInfo.pColorAttachments = colorAttachments.data();
        renderingInfo.pDepthAttachment = std::addressof( depthAttachment );

        vkCmdBeginRendering( vkCmd, &renderingInfo );

        m_FontTest = AssetsService::Get()->LoadAsset<Font>( Path{ "Resources/Fonts/Google_Sans_Code/GoogleSansCode-Italic-VariableFont_wght.ttf" } );
    }

    auto FontRenderPass::End() -> void {

        MKT_BEGIN_PROFILER_NAMED();

        const auto vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        vkCmdEndRendering( vkCmd );

        // Transition color target to shader read
        const auto tex{ dynamic_cast<VulkanTexture*>( m_ColorTarget.Image.GetRaw() ) };
        tex->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkCmd );

        m_FontRenderParams.clear();
    }

    auto FontRenderPass::Render( Scene* scene ) -> void {
        // Prepare data
        DrawText( { 0.0f, 0.0f }, "Hello", 10, { 0.2f, 0.5f, 0.2f, 1.0f } );

        const VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE( m_Device )->GetStorageBufferMinOffsetAlignment() };
        const VkDeviceSize paddedSize{ VulkanHelpers::GetUniformBufferPadding( sizeof( FontParams ), minOffsetAlignment ) };

        for ( Size meshInstanceIndex{}; meshInstanceIndex < m_FontRenderParams.size(); ++meshInstanceIndex ) {
            const VkDeviceSize dstOffset{ meshInstanceIndex * paddedSize };
            m_FontParamsSSBO->CopyFromBlock( &m_FontRenderParams[meshInstanceIndex], sizeof( FontParams ), dstOffset );
        }

        // UBO
        glm::mat4 proj{};
        proj = glm::ortho( 0.0f, static_cast<float>( m_ColorTarget.Image->GetWidth() ), static_cast<float>( m_ColorTarget.Image->GetHeight() ), 0.0f );

        m_UBO->CopyFromBlock( &proj, sizeof( proj ) );

        VkCommandBuffer vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        const VkPipelineLayout pipelineLayout{ m_Pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

        vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, std::addressof( m_UBOSet ), 0, nullptr );
        vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, std::addressof( m_FontParamsBufferSet ), 0, nullptr );
        vkCmdBindPipeline( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetNativeHandle( ObjectType::Vk_Pipeline ) );

        const std::array<VkDeviceSize, 1> offsets{};
        const std::array<VkBuffer, 1> vertexBuffers{ m_VertexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ) };

        vkCmdBindVertexBuffers( vkCmd, 0, 1, vertexBuffers.data(), offsets.data() );
        vkCmdBindIndexBuffer( vkCmd, m_IndexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), 0, VK_INDEX_TYPE_UINT32 );

        vkCmdDrawIndexed( vkCmd, static_cast<UInt32>( m_Indices.size() ), static_cast<UInt32>( m_FontRenderParams.size() ), 0, 0, 0 );
    }

    auto FontRenderPass::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto FontRenderPass::GetFinalComposition() const -> TextureHandle {
        return m_ColorTarget.Image;
    }

    auto FontRenderPass::RegisterTextureForRender( TextureHandle texture ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_BindlessTextures.try_emplace( texture.GetRaw(), texture );
        m_UpdateTextureDescriptor = true;
    }

    auto FontRenderPass::CreateAttributeBuffers() -> void {
        BufferDescription vertexDesc{};
        vertexDesc.WithData( reinterpret_cast<Byte*>( m_Vertices.data() ) )
                .WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
                .WithBufferDataType( BufferDataType::BUFFER_DATA_FLOAT32 )
                .WithSizeBytes( InferSize<FontVertex>( m_Vertices.size() ) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        m_VertexBuffer = m_Device->CreateBuffer( vertexDesc );

        BufferDescription indexDesc{};
        indexDesc.WithData( reinterpret_cast<Byte*>( m_Indices.data() ) )
                .WithUsage( BufferUsage::BUFFER_USAGE_INDEX )
                .WithSizeBytes( InferSize<UInt32>( m_Indices.size() ) )
                .WithBufferDataType( BufferDataType::BUFFER_DATA_UINT32 )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_IndexBuffer = m_Device->CreateBuffer( indexDesc );
    }

    auto FontRenderPass::InitBuffersAndSets() -> void {
        VulkanGraphicsPipeline* vkPipeline{ dynamic_cast<VulkanGraphicsPipeline*>( m_Pipeline.GetRaw() ) };

        // ───────────────────────────────────────────────
        // Set 0, textures and UBO
        // ───────────────────────────────────────────────
        m_UBO = CreateUniformBuffer<UBO>( m_Device );

        const VkDescriptorSetLayout& firstSetLayout{ vkPipeline->GetDescriptorSetLayout( 0 ) };
        m_UBOSet = TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( &firstSetLayout );
        DescriptorWriter descriptorWriterUBO{};
        descriptorWriterUBO.WriteBuffer( 0, m_UBO->GetNativeHandle( ObjectType::Vk_Buffer ), m_UBO->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
                .UpdateSet( VK_DEVICE( m_Device ), m_UBOSet );

        // Textures bindless
        const UInt32 maxBindlessTextures{ VulkanRenderer::GetMaxBindlessTextureCount() };
        std::array variableCount{ maxBindlessTextures };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableCountInfo.descriptorSetCount = static_cast<UInt32>( variableCount.size() );
        variableCountInfo.pDescriptorCounts = variableCount.data();

        m_UBOSet = TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( &firstSetLayout, std::addressof( variableCountInfo ) );

        // ───────────────────────────────────────────────
        // Set 1, font parameters
        // ───────────────────────────────────────────────
        constexpr Size elementCount{ 1024 };
        constexpr Size elementSize{ sizeof( FontParams ) };

        // UniformBuffer size padded. Vertex shader
        const VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE( m_Device )->GetStorageBufferMinOffsetAlignment() };
        const VkDeviceSize paddedSize{ VulkanHelpers::GetUniformBufferPadding( elementSize, minOffsetAlignment ) };

        const Size totalSize{ elementCount * paddedSize };

        BufferDescription desc{};
        desc.WithSizeBytes( totalSize )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

        m_FontParamsSSBO = m_Device->CreateBuffer( desc );
        m_FontParamsSSBO->SetDebugName( "FontRenderPass Instance SSBO" );

        const VkDescriptorSetLayout& secondSetLayout{ vkPipeline->GetDescriptorSetLayout( 1 ) };
        m_FontParamsBufferSet = TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( &secondSetLayout );
        DescriptorWriter descriptorWriter{};
        descriptorWriter.WriteBuffer( 0, m_FontParamsSSBO->GetNativeHandle( ObjectType::Vk_Buffer ), m_FontParamsSSBO->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
                .UpdateSet( VK_DEVICE( m_Device ), m_FontParamsBufferSet );
    }

    auto ComputeBasic::Init( GpuDevice* device ) -> void {

        m_Device = device;

        // Create small storage buffer
        const Size totalSize{ sizeof( UInt32 ) * m_Limit };
        BufferDescription desc{};
        desc.WithSizeBytes( totalSize )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

        m_StorageBuffer = m_Device->CreateBuffer( desc );
        m_StorageBuffer->SetDebugName( "ComputeBasic SSBO" );

        // Pipeline setup
        ShaderModuleHandle compModule{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE_STAGE ) };
        ComputePipelineDescription description{
            .Stage{ compModule }
        };

        m_Pipeline = m_Device->CreatePipeline( description );

        // Descriptor set setup
        VulkanComputePipeline* vkPipeline{ dynamic_cast<VulkanComputePipeline*>( m_Pipeline.GetRaw() ) };
        const VkDescriptorSetLayout& layout{ vkPipeline->GetDescriptorSetLayout( 0 ) };

        m_DescriptorSet = TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( &layout );

        // Descriptor update
        DescriptorWriter descriptorWriter{};
        descriptorWriter.WriteBuffer( 0, m_StorageBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), m_StorageBuffer->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
                .UpdateSet( VK_DEVICE( m_Device ), m_DescriptorSet );
    }

    auto ComputeBasic::Shutdown() -> void {
    }

    auto ComputeBasic::Begin( const CommandListHandle cmd ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_CmdList = cmd;

        std::vector<UInt32> data{};
        data.resize( m_Limit );

        m_StorageBuffer->CopyToBlock( data.data(), data.size() * sizeof( UInt32 ) );
    }

    auto ComputeBasic::End() -> void {
        MKT_BEGIN_PROFILER_NAMED();
    }

    auto ComputeBasic::Execute() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        VkCommandBuffer cmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        VkPipeline pipeline{ m_Pipeline->GetNativeHandle( ObjectType::Vk_Pipeline ) };
        VkPipelineLayout pipelineLayout{ m_Pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr );

        // Dispatch enough workgroups for 64 threads total
        constexpr UInt32 localSize{ 64 };// matches shader's local_size_x
        const UInt32 groupCount{ ( m_Limit + localSize - 1 ) / localSize };
        vkCmdDispatch( cmd, groupCount, 1, 1 );
    }
}// namespace Mikoto::VulkanPasses