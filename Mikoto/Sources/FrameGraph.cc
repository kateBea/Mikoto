//
// Created by kate on 11/24/25.
//

#include <memory>
#include <variant>

#include <Renderer/Core/FrameGraph.hh>

#include "Material/ShaderLibrary.hh"
#include "Renderer/Core/FramePass.hh"
#include "Renderer/Core/GraphicsContext.hh"

namespace Mikoto {

    auto FrameGraphBuilder::RegisterInput( FramePass *node, std::string_view name ) -> void {
        m_Nodes[node].Inputs.emplace_back( name );
    }

    auto FrameGraphBuilder::WriteTexture( FramePass *node, std::string_view name ) -> void {
        m_Nodes[node].Outputs.emplace_back( name );
    }

    auto FrameGraphBuilder::WriteBuffer( FramePass *node, std::string_view name ) -> void {

    }

    auto FrameGraphBuilder::ReadTexture( FramePass *node, std::string_view name ) -> void {

    }

    auto FrameGraphBuilder::ReadBuffer( FramePass *node, std::string_view name ) -> void {

    }

    auto FrameGraphBuilder::CreateNamedBuffer( std::string_view name, BufferDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::BUFFER;
        m_Resources[std::string{ name }].ResourceDesc = description;

    }

    auto FrameGraphBuilder::CreateNamedTexture( std::string_view name, TextureDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::TEXTURE;
        m_Resources[std::string{ name }].ResourceDesc = description;
    }

    auto FrameGraphBuilder::CreateNamedPipeline( FramePass *node, std::string_view name, PipelineDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::PIPELINE;
        m_Resources[std::string{ name }].ResourceDesc = description;

    }

    auto FrameGraphBuilder::CreateNamedRenderTarget( std::string_view name, TextureDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::RENDER_TARGET;
        m_Resources[std::string{ name }].ResourceDesc = description;
    }

    auto FrameGraphBuilder::RegisterShaderResource( FramePass* pass, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType type, ShaderResourceVisibility visibility ) -> void {
        m_Nodes[pass].ShaderResources[groupIndex] = ShaderResourceInfo{
            .Name{ name },
            .GroupBinding{ groupBinding },
            .ResourceType{ type },
            .Visibility{ visibility }
        };
    }

    FrameGraph::FrameGraph( GraphicsContext &context )
        : m_GraphicsContex{ std::addressof( context )}
    {
    }

    auto FrameGraph::Compile( FrameGraphBuilder &builder ) -> void {
        // Create the actual resources on the GPU

        // TODO: because pipeline need render targets to exists we create render targets first
        for (auto& [resourceName, resourceDescription] : builder.m_Resources) {
            if (resourceDescription.Type == FrameResourceType::RENDER_TARGET) {
                RegisterResource( resourceName, resourceDescription );
            }
        }

        // Register for each pass the shader resources
        for (auto& [resourceName, resourceDescription] : builder.m_Resources) {
            if (resourceDescription.Type == FrameResourceType::PIPELINE) {
                // Pass can only have one pipeline
                RegisterResource( resourceName, resourceDescription );
            }
        }

        for (auto& [framePass, nodeData] : builder.m_Nodes) {
            PipelineHandle passPipeline{};
            for (auto& resourceName : builder.m_Nodes) {
                if (!passPipeline.IsEmpty()) {
                    // Pass can only have one pipeline
                    break;
                }
            }

            MKT_ASSERT( !passPipeline.IsEmpty(), "There must be a valid pipeline" );

            // This registers the pass and creates its resources which includes the descriptor sets
            m_GraphicsContex->RegisterPass(framePass, passPipeline);

            // Now we iterate the resources it needs resolve them from the frame graph and update the corresponding binding in the set
            for (auto& [groupIndex, shaderResource] : nodeData.ShaderResources) {
                // Find the resource
                switch (shaderResource.ResourceType) {
                    case ShaderResourceType::SHADER_STORAGE_BUFFER:
                    case ShaderResourceType::SHADER_RESOURCE_UNIFORM_BUFFER:
                        m_GraphicsContex->PushBuffer(framePass, groupIndex, shaderResource.GroupBinding, shaderResource.ResourceType, shaderResource.Visibility, GetNamedBuffer( shaderResource.Name ));
                        break;
                    case ShaderResourceType::SHADER_RESOURCE_COMBINED_IMAGE_SAMPLER:
                        m_GraphicsContex->PushImage(framePass, groupIndex, shaderResource.GroupBinding, shaderResource.ResourceType, shaderResource.Visibility, GetNamedTexture( shaderResource.Name ));
                        break;
                    case ShaderResourceType::SHADER_RESOURCE_UNDEFINED:
                        break;
                }
            }
        }

        // TODO: Sort passes according to dependencies
        // Prepare input and outputs
        for (auto& [node, data] : builder.m_Nodes) {
            // Register outputs only because those are the only ones that can be created, inputs are consumed
            m_Nodes.emplace_back( FrameNode{ node, std::move(data.Inputs), std::move(data.Outputs) } );
        }
    }

    auto FrameGraph::Execute() -> void {
        GpuDevice* gpuDevice{ m_GraphicsContex->GetDevice() };

        // Queue type according to command types, we could switch later depending on the type of pass
        CommandListHandle gpuCmdList{ gpuDevice->CreateCommandList( QueueType::GRAPHICS_QUEUE ) };
        gpuCmdList->Begin();

        m_GraphicsContex->BeginFrame(gpuCmdList);

        for ( const FrameNode & pass : m_Nodes) {
            PassCommandList passCommands{ m_GraphicsContex, this };
            pass.Pass->Execute( passCommands );
        }

        m_GraphicsContex->EndFrame();

        gpuCmdList->End();
        gpuDevice->SubmitCommands( gpuCmdList );
    }

    auto FrameGraph::GetNamedTexture( std::string_view name ) -> TextureHandle {
        const auto it{ m_TexturesByNames.find( std::string{ name } ) };
        if (it != m_TexturesByNames.end()) {
            return it->second;
        }

        return TextureHandle::CreateEmpty();
    }

    auto FrameGraph::GetNamedPipeline( std::string_view name ) -> PipelineHandle {
        const auto it{ m_PipelinesByNames.find( std::string{ name } ) };
        if (it != m_PipelinesByNames.end()) {
            return it->second;
        }

        return PipelineHandle::CreateEmpty();
    }

    auto FrameGraph::Create( GraphicsContext *context ) -> Unique<FrameGraph> {
        return CreateScope<FrameGraph>( *context );
    }

    auto FrameGraph::RegisterTexture( std::string_view name, TextureDescription description ) -> void {

    }

    auto FrameGraph::RegisterPipeline( std::string_view name, PipelineDescription description ) -> void {
        if (m_PipelinesByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN("FrameGraph::RegisterPipeline - Named pipeline [{}] already exists.", name);
            return;
        }

        GpuDevice* gpuDevice{ m_GraphicsContex->GetDevice() };
        PipelineHandle pipeline{ PipelineHandle::CreateEmpty() };

        if (std::holds_alternative<GraphicsPipelineDescription>( description.Description )) {
            // Graphics pipeline ====================================
            GraphicsPipelineDescription& desc{ std::get<GraphicsPipelineDescription>( description.Description ) };

            for (auto& [stage, shaderPath] : description.Shaders) {
                desc.ShaderStages.emplace_back( ShaderLibrary::Get()->LoadShader(shaderPath, stage) );
            }

            // TODO: find render target formats
            desc.DepthTexture = GetNamedTexture( description.DepthRenderTargets);
            for (const auto& renderTargetName : description.ColorRenderTargets) {
                desc.ColorAttachments.emplace_back( GetNamedTexture( renderTargetName) );
            }

            pipeline = gpuDevice->CreatePipeline( desc );
        } else if (std::holds_alternative<ComputePipelineDescription>( description.Description )) {
            // Compute pipeline ====================================
            ComputePipelineDescription& desc{ std::get<ComputePipelineDescription>( description.Description ) };

            for (auto& [stage, shaderPath] : description.Shaders) {
                desc.Stage = ShaderLibrary::Get()->LoadShader(shaderPath, stage);
            }

            pipeline = gpuDevice->CreatePipeline( desc );
        }

        if (!pipeline.IsEmpty()) {
            m_PipelinesByNames.emplace( std::string{ name }, pipeline );
        }
    }

    auto FrameGraph::RegisterRenderTarget( std::string_view name, TextureDescription description ) -> void {
        if (m_TexturesByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN("FrameGraph::RegisterPipeline - Named render target [{}] already exists.", name);
            return;
        }

        GpuDevice* gpuDevice{ m_GraphicsContex->GetDevice() };
        TextureHandle texture{ gpuDevice->CreateTexture( description ) };

        if (!texture.IsEmpty()) {
            m_TexturesByNames.emplace( std::string{ name }, texture );
        }
    }

    auto FrameGraph::RegisterBuffer( std::string_view name, BufferDescription description ) -> void {}

    auto FrameGraph::RegisterResource(std::string_view name, ResourceDescription resource ) -> void {
        switch (resource.Type) {
            case FrameResourceType::RENDER_TARGET:
                if (std::holds_alternative<TextureDescription>( resource.ResourceDesc )) {
                    RegisterRenderTarget( name, std::get<TextureDescription>( resource.ResourceDesc ) );
                }

                break;
            case FrameResourceType::TEXTURE:
                if (std::holds_alternative<TextureDescription>( resource.ResourceDesc )) {
                    RegisterTexture( name, std::get<TextureDescription>( resource.ResourceDesc ) );
                }
                break;
            case FrameResourceType::BUFFER:
                if (std::holds_alternative<BufferDescription>( resource.ResourceDesc )) {
                    RegisterBuffer( name, std::get<BufferDescription>( resource.ResourceDesc ) );
                }
                break;
            case FrameResourceType::PIPELINE:
                if (std::holds_alternative<PipelineDescription>( resource.ResourceDesc )) {
                    RegisterPipeline( name, std::get<PipelineDescription>( resource.ResourceDesc ) );
                }
                break;
            case FrameResourceType::INVALID:
                MKT_CORE_LOGGER_WARN( "FrameGraph::RegisterResource - Invalid resource type." );
                break;
        }
    }

    auto FrameGraph::GetNamedBuffer( std::string_view name ) -> BufferHandle {
        const auto it{ m_BuffersByNames.find( std::string{ name } ) };
        if (it != m_BuffersByNames.end()) {
            return it->second;
        }

        return BufferHandle::CreateEmpty();
    }
}// namespace Mikoto