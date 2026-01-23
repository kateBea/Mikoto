//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <queue>
#include <variant>

#include <Common/String.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/GraphicsContext.hh>

namespace Mikoto {

    FramePassBuilder::FramePassBuilder( FramePassNode &node )
        : m_Node{ std::addressof( node ) }
    {}

    auto FramePassBuilder::Write( std::string_view name, FrameResourceState inState, FrameResourceState outState ) -> void {
        m_Node->Writes.emplace_back( StringUtil::From( name ), inState, outState );
    }

    auto FramePassBuilder::Read( std::string_view name, FrameResourceState inState, FrameResourceState outState ) -> void {
        m_Node->Writes.emplace_back( StringUtil::From( name ), inState, outState );
    }

    auto FramePassBuilder::CreateBuffer( std::string_view name, BufferDescription description ) -> void {
        m_Creates[std::string{ name }].Type = FrameResourceType::BUFFER;
        m_Creates[std::string{ name }].Description = description;
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, TextureDescription description ) -> void {
        m_Creates[std::string{ name }].Type = FrameResourceType::TEXTURE;
        m_Creates[std::string{ name }].Description = description;
    }

    auto FramePassBuilder::CreatePipeline( std::string_view name, PipelineDescription description ) -> void {
        m_Creates[std::string{ name }].Type = FrameResourceType::PIPELINE;
        m_Creates[std::string{ name }].Description = description;
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, TextureCubeCreateDescription description ) -> void {
        m_Creates[std::string{ name }].Type = FrameResourceType::RENDER_TARGET;
        m_Creates[std::string{ name }].Description = description;
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format ) -> void {
        TextureDescription colorDesc{};
        colorDesc.WithWidth( width )
            .WithHeight( height )
            .WithChannelCount( 4 )
            .WithData( nullptr )
            .WithType( TextureType::TEXTURE_2D )
            .WithTextureUsage( TextureUsage::TEXTURE_USAGE_COLOR )
            .WithFormat( format )
            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        CreateTexture( name, colorDesc );
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, UInt32 dimensions, TextureFormat format, UInt32 mipLevels ) -> void {
        TextureCubeCreateDescription depthDesc{};
        depthDesc.WithUsageType( TextureUsage::TEXTURE_USAGE_RENDER_TARGET )
            .WithMipLevels( mipLevels )
            .WithTextureFormat( format )
            .WithDimensions( dimensions )
            .WithResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

        CreateTexture( name, depthDesc );
    }

    FrameGraph::FrameGraph( GraphicsContext* context, GpuDevice* device)
        : m_GraphicsContex{ context }, m_Device{ device }
    {}

    auto FrameGraph::Compile( ) -> void {


        SortPassExecution();
    }

    auto FrameGraph::Execute() -> void {
        // m_GraphicsContex->BeginFrame();
        //
        // for ( auto& [name, node] : m_Passes) {
        //     CommandContext commandContext{ m_GraphicsContex, m_Device };
        //     SetShaderResourceGroups(node);
        //
        //     commandContext.Begin(node);
        //
        //     BindShaderResourceGroups(node);
        //
        //     node.ExecuteCallback( commandContext, m_GraphBlackboard );
        //
        //     commandContext.End();
        // }
        //
        // m_GraphicsContex->EndFrame();
    }

    auto FrameGraph::GetTexture( std::string_view name ) const -> TextureHandle {
        const auto it{ m_TexturesByNames.find( StringUtil::From( name ) ) };
        if (it != m_TexturesByNames.end()) {
            return it->second;
        }

        return TextureHandle::CreateEmpty();
    }

    auto FrameGraph::GetBuffer( std::string_view name ) const -> BufferHandle {
        const auto it{ m_BuffersByNames.find( StringUtil::From( name ) ) };
        if (it != m_BuffersByNames.end()) {
            return it->second;
        }
        return BufferHandle::CreateEmpty();
    }

    auto FrameGraph::Create( GraphicsContext* context, GpuDevice* device ) -> Unique<FrameGraph> {
        return CreateScope<FrameGraph>( context, device );
    }

    auto FrameGraph::IsFramePassPresent( const std::string_view name ) const -> bool {
        return m_Passes.contains( std::string{ name } );
    }

    auto FrameGraph::CreatePassNode( std::string_view name ) -> FramePassNode & {
        MKT_ASSERT( !IsFramePassPresent( name ), StringUtil::Format("Cannot register pass {} more than once", name) );
        return m_Passes.emplace( StringUtil::From( name ), FramePassNode{} ).first->second;
    }

    auto FrameGraph::SetShaderResourceGroups( FramePassNode &node ) -> void {

    }

    auto FrameGraph::CreateCommitedResources( FramePassBuilder &builder ) -> void {
        // Create the actual resources on the GPU
        // for (auto& [resourceName, resourceDescription] : builder.m_Resources) {
        //     RegisterResource( resourceName, resourceDescription );
        // }

        // Register the actual pipelines
        // RegisterResource( resourceName, builder.m_Pipeline );
    }

    // auto FrameGraph::SetShaderResourceGroups(PassNode& node) -> void {
    //     // Check if dirty and then update otherwise dont update
    //     if (!node->IsSRGDirty()) { retur; }
    //
    //     for (auto& bind : node.bufferBindings) {
    //         m_Context.SetBufferBindSlot(bind.srgType, ResolveResource(bind.resource), bind.bindingSlot);
    //     }
    //
    //     for (auto& bind : node.textureBindings) {
    //         m_Context.SetTextureBindSlot(bind.srgType, ResolveResource(bind.resource), bind.samplerName, bind.bindingSlot);
    //     }
    //
    //     m_Context->CommitShaderResources(node);
    // }


    auto FrameGraph::SortPassExecution() -> void {
        // ankerl::unordered_dense::map<std::string, FramePass*> resourceWriters{};
        //
        // // Assumption: One writer per resource per frame
        //
        // for (auto& [pass, nodeData] : m_Builder.m_Nodes) {
        //     for (const auto& buf : nodeData.WriteBuffers) {
        //         resourceWriters[buf] = pass;
        //     }
        //
        //     for (const auto& tex : nodeData.WriteTextures) {
        //         resourceWriters[tex] = pass;
        //     }
        // }
        //
        // struct SortNode {
        //     FrameNode* Node{};
        //     UInt32 Incoming{};
        //     std::vector<SortNode*> Outgoing{};
        // };
        //
        // ankerl::unordered_dense::map<FramePass*, SortNode> sortNodes{};
        //
        // for (FrameNode& node : m_Nodes) {
        //     sortNodes[node.Pass].Node = std::addressof( node );
        // }
        //
        // for (auto& [pass, nodeData] : m_Builder.m_Nodes) {
        //     SortNode& readerNode{ sortNodes[pass] };
        //
        //     auto addDependency{ [&](const std::string& resource) {
        //         auto it{ resourceWriters.find(resource) };
        //         if (it == resourceWriters.end())
        //             return;
        //
        //         FramePass* writer{ it->second };
        //         if (writer == pass)
        //             return; // same pass, ignore
        //
        //         SortNode& writerNode{ sortNodes[writer] };
        //         writerNode.Outgoing.push_back(&readerNode);
        //         readerNode.Incoming++;
        //     }};
        //
        //     for (const auto& buf : nodeData.ReadBuffers)
        //         addDependency(buf);
        //
        //     for (const auto& tex : nodeData.ReadTextures)
        //         addDependency(tex);
        // }
        //
        // std::vector<FrameNode> sorted{};
        // std::queue<SortNode*> ready{};
        //
        // for (auto& [_, sn] : sortNodes) {
        //     if (sn.Incoming == 0)
        //         ready.push(&sn);
        // }
        //
        // while (!ready.empty()) {
        //     SortNode* sn{ ready.front() };
        //     ready.pop();
        //
        //     sorted.push_back(*sn->Node);
        //
        //     for (SortNode* dependent : sn->Outgoing) {
        //         if (--dependent->Incoming == 0)
        //             ready.push(dependent);
        //     }
        // }
        //
        // if (sorted.size() != m_Nodes.size()) {
        //     MKT_CORE_LOGGER_ERROR("FrameGraph cycle detected!");
        //
        // }
        //
        // m_Nodes = std::move(sorted);
        // m_Compiled = true;
    }

    // auto FrameGraph::RegisterResource(std::string_view name, FrameResource resource ) const -> void {
    //     switch (resource.Type) {
    //         case FrameResourceType::TEXTURE:
    //         case FrameResourceType::RENDER_TARGET:
    //             if (std::holds_alternative<TextureDescription>( resource.Description )) {
    //                 m_Blackboard->RegisterTexture( name, std::get<TextureDescription>( resource.Description ) );
    //             } else if (std::holds_alternative<TextureCubeCreateDescription>( resource.Description )) {
    //                 m_Blackboard->RegisterTexture( name, std::get<TextureCubeCreateDescription>( resource.Description ) );
    //             }
    //             break;
    //         case FrameResourceType::BUFFER:
    //             if (std::holds_alternative<BufferDescription>( resource.Description )) {
    //                 m_Blackboard->RegisterBuffer( name, std::get<BufferDescription>( resource.Description ) );
    //             }
    //             break;
    //         case FrameResourceType::PIPELINE:
    //             if (std::holds_alternative<PipelineDescription>( resource.Description )) {
    //                 m_Blackboard->RegisterPipeline( name, std::get<PipelineDescription>( resource.Description ) );
    //             }
    //             break;
    //         case FrameResourceType::INVALID:
    //             MKT_CORE_LOGGER_WARN( "FrameGraph::RegisterResource - Invalid resource type." );
    //             break;
    //     }
    // }
}// namespace Mikoto