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

    auto FrameGraphBuilder::RegisterPass( FramePass *pass ) -> void {
        m_Nodes[pass] = NodeData{};
    }

    auto FrameGraphBuilder::WriteTexture( FramePass *node, std::string_view name ) -> void {
        m_Nodes[node].WriteTextures.emplace_back( name );
    }

    auto FrameGraphBuilder::WriteBuffer( FramePass *node, std::string_view name ) -> void {
        m_Nodes[node].WriteBuffers.emplace_back( name );
    }

    auto FrameGraphBuilder::ReadTexture( FramePass *node, std::string_view name ) -> void {
        m_Nodes[node].ReadTextures.emplace_back( name );
    }

    auto FrameGraphBuilder::ReadBuffer( FramePass *node, std::string_view name ) -> void {
        m_Nodes[node].ReadBuffers.emplace_back( name );
    }

    auto FrameGraphBuilder::CreateNamedBuffer( std::string_view name, BufferDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::BUFFER;
        m_Resources[std::string{ name }].Description = description;

    }

    auto FrameGraphBuilder::CreateNamedTexture( std::string_view name, TextureDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::TEXTURE;
        m_Resources[std::string{ name }].Description = description;
    }

    auto FrameGraphBuilder::CreateNamedPipeline( std::string_view name, PipelineDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::PIPELINE;
        m_Resources[std::string{ name }].Description = description;
    }

    auto FrameGraphBuilder::CreateNamedRenderTarget( std::string_view name, TextureDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::RENDER_TARGET;
        m_Resources[std::string{ name }].Description = description;
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
        m_Blackboard = CreateScope<FrameBlackboard>( m_GraphicsContex->GetDevice() );
    }

    auto FrameGraph::Compile( FrameGraphBuilder &builder ) -> void {
        // Create the actual resources on the GPU

        // TODO: because pipeline need render targets to exists we create render targets first
        for (auto& [resourceName, resourceDescription] : builder.m_Resources) {
            if (resourceDescription.Type != FrameResourceType::PIPELINE) {
                RegisterResource( resourceName, resourceDescription );
            }
        }

        // Register the actual pipelines
        for (auto& [resourceName, resourceDescription] : builder.m_Resources) {
            if (resourceDescription.Type == FrameResourceType::PIPELINE) {
                RegisterResource( resourceName, resourceDescription );
            }
        }

        // Register nodes
        for (auto& [node, nodeData] : builder.m_Nodes) {
            m_Nodes.emplace_back( node );
        }

        // TODO: Sort passes according to dependencies
    }

    auto FrameGraph::Execute() -> void {
        GpuDevice* gpuDevice{ m_GraphicsContex->GetDevice() };

        // Queue type according to command types, we could switch later depending on the type of pass
        CommandListHandle gpuCmdList{ gpuDevice->CreateCommandList( QueueType::GRAPHICS_QUEUE ) };
        gpuCmdList->Begin();

        m_GraphicsContex->BeginFrame(gpuCmdList);

        for ( const FrameNode & pass : m_Nodes) {
            PassCommandList passCommands{ m_GraphicsContex, m_Blackboard.get() };
            pass.Pass->Execute( passCommands );
        }

        m_GraphicsContex->EndFrame();

        gpuCmdList->End();
        gpuDevice->SubmitCommands( gpuCmdList );
    }

    auto FrameGraph::GetBlackboard() const -> FrameBlackboard * {
        return m_Blackboard.get();
    }

    auto FrameGraph::Create( GraphicsContext *context ) -> Unique<FrameGraph> {
        return CreateScope<FrameGraph>( *context );
    }

    auto FrameGraph::RegisterResource(std::string_view name, FrameResource resource ) const -> void {
        switch (resource.Type) {
            case FrameResourceType::RENDER_TARGET:
                if (std::holds_alternative<TextureDescription>( resource.Description )) {
                    m_Blackboard->RegisterTexture( name, std::get<TextureDescription>( resource.Description ) );
                }

                break;
            case FrameResourceType::TEXTURE:
                if (std::holds_alternative<TextureDescription>( resource.Description )) {
                    m_Blackboard->RegisterTexture( name, std::get<TextureDescription>( resource.Description ) );
                }
                break;
            case FrameResourceType::BUFFER:
                if (std::holds_alternative<BufferDescription>( resource.Description )) {
                    m_Blackboard->RegisterBuffer( name, std::get<BufferDescription>( resource.Description ) );
                }
                break;
            case FrameResourceType::PIPELINE:
                if (std::holds_alternative<PipelineDescription>( resource.Description )) {
                    m_Blackboard->RegisterPipeline( name, std::get<PipelineDescription>( resource.Description ) );
                }
                break;
            case FrameResourceType::INVALID:
                MKT_CORE_LOGGER_WARN( "FrameGraph::RegisterResource - Invalid resource type." );
                break;
        }
    }
}// namespace Mikoto