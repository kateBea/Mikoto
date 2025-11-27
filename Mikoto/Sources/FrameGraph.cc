//
// Created by kate on 11/24/25.
//

#include <memory>

#include <Renderer/Core/FrameGraph.hh>

#include "Renderer/Core/FramePass.hh"
#include "Renderer/Core/GraphicsContext.hh"

namespace Mikoto {

    auto FrameGraphBuilder::RegisterInput( FramePass *node, std::string_view name ) -> void {
    }
    auto FrameGraphBuilder::RegisterOutput( FramePass *node, std::string_view name ) -> void {
    }

    auto FrameGraphBuilder::CreateNamedBuffer( std::string_view name, BufferDescription description ) -> void {
    }
    auto FrameGraphBuilder::CreateNamedTexture( std::string_view name, TextureDescription description ) -> void {
    }
    auto FrameGraphBuilder::CreateNamedPipeline( std::string_view name, PipelineDescription description, PipelineType type ) -> void {
    }
    auto FrameGraphBuilder::CreateNamedRenderTarget( std::string_view name, TextureDescription description, RenderTargetType ) -> void {
    }
    FrameGraph::FrameGraph( GraphicsContext &context )
        : m_GraphicsContex{ std::addressof( context )}
    {
    }
    auto FrameGraph::RegisterPass( FramePass *pass ) -> FramePass * {
        return nullptr;
    }

    auto FrameGraph::Compile( FrameGraphBuilder &builder ) -> void {
        // We have the list of resources here we now just need to create them
        for ( const FrameNode & pass : m_Nodes) {
            // auto& outputs{ pass->GetOutputResources() };
            //
            // // I do not register input resources because they are consumed
            // // I will only register outputs because those are produced by passes
            // for (auto& output : outputs) {
            //     RegisterResource( output );
            // }
        }

        // Sort passes according to dependencies
    }

    auto FrameGraph::Execute( GraphicsContext &backend ) -> void {
        for ( const FrameNode & pass : m_Nodes) {
            PassCommandList* cmdList{ m_GraphicsContex->CreateCommandList() };
            cmdList->Begin();

            pass.Pass->Execute( *cmdList );

            cmdList->End();
            m_GraphicsContex->SubmitCommandList(cmdList);
        }
    }

    auto FrameGraph::Create( GraphicsContext *context ) -> Unique<FrameGraph> {
        return CreateScope<FrameGraph>( *context );
    }

    auto FrameGraph::RegisterResource( FrameResource resource ) -> void {
        if (!m_Resources.contains( resource.Name )) {

        }
    }
}// namespace Mikoto