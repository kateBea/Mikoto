//
// Created by kate on 10/23/25.
//

// C++ Standard Library
#include <array>
#include <cstdlib>
// Third-Party Libraries
#include <volk.h>

#include <Renderer/Core/RenderUtility.hh>
#include <Core/Profiler.hh>
#include <Material/PBRMaterial.hh>
#include <Material/ShaderLibrary.hh>
#include <Scene/Component.hh>
#include <Memory/Allocator.hh>

#include "Renderer/Vulkan/VulkanDevice.hh"
#include "Renderer/Vulkan/VulkanHelpers.hh"
#include "Renderer/Vulkan/VulkanPasses.hh"
#include "Renderer/Vulkan/VulkanTexture.hh"

namespace Mikoto::VulkanPasses {

    static auto GetMeshTextureIndices(TextureHandle texture) -> Int32 {
        if ( const auto vkTexture{ dynamic_cast<VulkanTexture*>( texture.GetRaw() ) } ) {
            return vkTexture->GetTextureIndex();
        }

        return -1;
    }

    auto ShadingPass::Init( GpuDevice* device ) -> void{
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
        ShaderModuleHandle vertShader{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle fragShader{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT_STAGE ) };

        GraphicsPipelineDescription pipelineDesc{};
        pipelineDesc.ShaderStages = { vertShader, fragShader };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;
        pipelineDesc.DepthTexture = m_DepthTarget.Image;
        pipelineDesc.ColorAttachments = { m_ColorTarget.Image };

        m_Pipeline = m_Device->CreatePipeline( pipelineDesc );

        InitInstanceData();
        CreateMeshesStorageDescriptorSet();
    }

    auto ShadingPass::Shutdown() -> void {

        //MKT_NOTHROW_PLACEMENT_DELETE( m_InstanceDataPtr );

        m_BatchOffsetMap.clear();
        m_MeshBatches.clear();
    }

    auto ShadingPass::Begin( CommandListHandle cmd ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_DrawCalls = 0;

        m_CmdList = cmd;
        VkCommandBuffer vkCmd { cmd->GetNativeHandle(ObjectType::Vk_CmdBuffer) };

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

        const auto vkCmd{ m_CmdList->GetNativeHandle(ObjectType::Vk_CmdBuffer) };
        vkCmdEndRendering( vkCmd );

        // Transition color target to shader read
        const auto tex{ dynamic_cast<VulkanTexture*>( m_ColorTarget.Image.GetRaw() ) };
        tex->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkCmd );

        //MKT_CORE_LOGGER_DEBUG( "ShadingPass::End - Draw call count {} ", m_DrawCalls );
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

    auto ShadingPass::Render( Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Clear previous batches
        m_MeshBatches.clear();

        // Build batches per mesh
        auto& registry{ scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComp { registry.get<MeshComponent>( entity ) };
            auto& materialComp { registry.get<MaterialComponent>( entity ) };

            MeshNode* meshNode{ meshComp.GetMesh() };
            PBRMaterial* pbrMat{ dynamic_cast<PBRMaterial*>(materialComp.GetMaterial().GetRaw()) };

            if ( !meshNode || !pbrMat ) {
                continue;
            }

            ShadingPassMeshBufferUBO ubo{};
            ubo.Transform = transform.GetTransform();
            ubo.Albedo = pbrMat->GetColor();
            ubo.Factors.x = pbrMat->GetMetallicFactor();
            ubo.Factors.y = pbrMat->GetRoughnessFactor();

            ubo.AlbedoIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::ALBEDO_TEXTURE ) );
            ubo.NormalIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::NORMAL_TEXTURE ) );
            ubo.MetallicIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::METALLIC_TEXTURE ) );
            ubo.RoughnessIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
            ubo.AoIndex = GetMeshTextureIndices( pbrMat->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );

            auto& [Mesh, Instances]{ m_MeshBatches[meshNode] };

            Mesh = meshNode;
            Instances.push_back( ubo );
        }

        // Upload instances into GPU buffer
        UploadInstanceData();

        // Draw each mesh with instancing
        for ( auto& batch: m_MeshBatches | std::views::values ) {
            if ( !batch.Instances.empty() ) {
                DrawMeshBatch( batch );
            }
        }
    }

    auto ShadingPass::InitInstanceData() -> void {

        constexpr Size elementCount{ 1024 };
        constexpr Size elementSize{ sizeof( ShadingPassMeshBufferUBO ) };

        // UniformBuffer size padded. Vertex shader
        const VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE( m_Device )->GetStorageBufferMinOffsetAlignment() };
        const VkDeviceSize paddedSize{ VulkanHelpers::GetUniformBufferPadding(elementSize, minOffsetAlignment) };

        const Size totalSize{ elementCount * paddedSize };

        BufferDescription desc{};
        desc.WithSizeBytes( totalSize )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

        m_InstanceSSBO = m_Device->CreateBuffer( desc );
        m_InstanceSSBO->SetDebugName( "ShadingPass Instance SSBO" );

        //m_InstanceDataPtr = MKT_NOTHROW_PLACEMENT_NEW( totalSize );
    }

    auto ShadingPass::UploadInstanceData() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        std::vector<ShadingPassMeshBufferUBO> allInstances{};

        for ( auto& [meshNode, batch]: m_MeshBatches ) {
            allInstances.insert( allInstances.end(), batch.Instances.begin(), batch.Instances.end() );

            m_BatchOffsetMap[meshNode] = allInstances.size() - batch.Instances.size();
        }

        const VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE( m_Device )->GetStorageBufferMinOffsetAlignment() };
        const VkDeviceSize paddedSize{ VulkanHelpers::GetUniformBufferPadding(sizeof(ShadingPassMeshBufferUBO), minOffsetAlignment) };

        for (Size meshInstanceIndex{}; meshInstanceIndex < allInstances.size(); ++meshInstanceIndex) {
            const VkDeviceSize dstOffset{ meshInstanceIndex * paddedSize };
            m_InstanceSSBO->CopyFromBlock(&allInstances[meshInstanceIndex], sizeof(ShadingPassMeshBufferUBO), dstOffset);
        }

        auto vkCmd { m_CmdList->GetNativeHandle(ObjectType::Vk_CmdBuffer) };
        vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 m_Pipeline->GetNativeHandle(ObjectType::Vk_PipelineLayout),
                                 2, 1, &m_MeshDataSet, 0, nullptr );
    }

    auto ShadingPass::CreateMeshesStorageDescriptorSet() -> void {

        VulkanGraphicsPipeline* vkPipeline{ dynamic_cast<VulkanGraphicsPipeline*>( m_Pipeline.GetRaw() ) };
        const VkDescriptorSetLayout& layout{ vkPipeline->GetDescriptorSetLayout(2) };

        m_MeshDataSet = TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( &layout );

        // Write buffer info
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_InstanceSSBO->GetNativeHandle(ObjectType::Vk_Buffer);
        bufferInfo.offset = 0;
        bufferInfo.range = m_InstanceSSBO->GetSizeBytes(); // WHOLE_SIZE if dynamic (but count will be the total)

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_MeshDataSet;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = std::addressof( bufferInfo );

        vkUpdateDescriptorSets(VK_DEVICE(m_Device), 1, &write, 0, nullptr);
    }

    auto ShadingPass::DrawMeshBatch( const MeshBatch& batch ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!batch.Mesh || batch.Instances.empty()) return;
        if (batch.Mesh->GetVertexBuffer().IsEmpty() || batch.Mesh->GetIndexBuffer().IsEmpty()) return;

        const VkCommandBuffer vkCmd { m_CmdList->GetNativeHandle(ObjectType::Vk_CmdBuffer) };

        BufferHandle vertexBuffer{ batch.Mesh->GetVertexBuffer() };
        BufferHandle indexBuffer{ batch.Mesh->GetIndexBuffer() };

        const std::array<VkDeviceSize, 1> offsets{};
        const std::array<VkBuffer, 1> vertexBuffers{ vertexBuffer->GetNativeHandle(ObjectType::Vk_Buffer) };

        vkCmdBindVertexBuffers(vkCmd, 0, 1, vertexBuffers.data(), offsets.data());
        vkCmdBindIndexBuffer(vkCmd, indexBuffer->GetNativeHandle(ObjectType::Vk_Buffer), 0, VK_INDEX_TYPE_UINT32);

        // find the base/firstInstance for this mesh
        const Size baseInstance{ m_BatchOffsetMap.at( batch.Mesh ) };
        const UInt32 firstInstance{ static_cast<UInt32>( baseInstance ) };

        ++m_DrawCalls;
        vkCmdDrawIndexed(
            vkCmd,
            indexBuffer->GetCount(),                            // indexCount
            static_cast<UInt32>(batch.Instances.size()),       // instanceCount
            0,                                                 // firstIndex
            0,                                                 // vertexOffset
            firstInstance                                      // firstInstance
        );
    }

    auto ShadingPass::OnResize( UInt32 width, UInt32 height ) -> void {
        // TODO: resize color/depth targets
    }

    auto ShadingPass::BindDefaultSets( VkDescriptorSet& set, const UInt32 setIndex ) -> void {
        const VkCommandBuffer vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        const VkPipelineLayout pipeline{ m_Pipeline->GetNativeHandle(ObjectType::Vk_PipelineLayout) };

        vkCmdBindDescriptorSets(vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, setIndex, 1, std::addressof( set ), 0, nullptr);
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
        ShaderModuleHandle compModule{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE_STAGE ) };
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
        descriptorWriter.WriteBuffer( 0, m_StorageBuffer->GetNativeHandle(ObjectType::Vk_Buffer), m_StorageBuffer->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .UpdateSet( VK_DEVICE(m_Device), m_DescriptorSet );
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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

        // Dispatch enough workgroups for 64 threads total
        constexpr UInt32 localSize{ 64 }; // matches shader's local_size_x
        const UInt32 groupCount{ (m_Limit + localSize - 1) / localSize };
        vkCmdDispatch(cmd, groupCount, 1, 1);
    }

    auto TextPass::Init( GpuDevice* device ) -> void {

    }

    auto TextPass::Shutdown() -> void {
    }

    auto TextPass::Begin( CommandListHandle cmd ) -> void {
    }

    auto TextPass::End() -> void {
    }

    auto TextPass::Render( Scene* scene ) -> void {
    }

    auto TextPass::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto ShadowPass::Init( GpuDevice* device ) -> void {
    }

    auto ShadowPass::Shutdown() -> void {
    }

    auto ShadowPass::Begin( CommandListHandle cmd ) -> void {
    }

    auto ShadowPass::End() -> void {
    }

    auto ShadowPass::Render( Scene* scene ) -> void {
    }

    auto ShadowPass::OnResize( UInt32 width, UInt32 height ) -> void {
    }

}// namespace Mikoto::VulkanPasses