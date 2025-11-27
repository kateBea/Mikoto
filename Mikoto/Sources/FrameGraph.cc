//
// Created by kate on 11/24/25.
//

#include <memory>

#include <Renderer/Core/FrameGraph.hh>

#include "Renderer/Core/FramePass.hh"
#include "Renderer/Core/GraphicsContext.hh"

namespace Mikoto {

    FrameGraph::FrameGraph( GraphicsContext &context )
        : m_GraphicsContex{ std::addressof( context )}
    {
    }
    auto FrameGraph::RegisterPass( FramePass *pass ) -> FramePass * {
        return nullptr;
    }

    auto FrameGraph::Compile( GraphicsContext &backend ) -> void {
        // We have the list of resources here we now just need to create them

        // Sort passes according to dependencies
    }

    auto FrameGraph::Execute( GraphicsContext &backend ) -> void {
        for (auto& pass : m_Nodes) {
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
}// namespace Mikoto