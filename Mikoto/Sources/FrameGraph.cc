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
#include <Renderer/Core/RenderUtility.hh>

#include "Core/Profiler.hh"

namespace Mikoto {

    auto FramePassNode::IsActive() const -> bool {
        return IsStatus( FramePassNodeStatus::ACTIVE );
    }

    auto FramePassNode::IsSleeping() const -> bool {
        return IsStatus( FramePassNodeStatus::SLEEPING );
    }

    auto FramePassNode::IsStatus(FramePassNodeStatus status) const -> bool {
        return Status == status;
    }

    FramePassBuilder::FramePassBuilder( FramePassNode &node )
        : m_Node{ std::addressof( node ) } {}

    auto FramePassBuilder::Write( std::string_view name, FrameResourceState outState ) -> void {
        m_Node->Writes.emplace_back( StringUtil::From( name ), outState );
    }

    auto FramePassBuilder::Read( std::string_view name, FrameResourceState outState ) -> void {
        m_Node->Writes.emplace_back( StringUtil::From( name ), outState );
    }

    auto FramePassBuilder::UseShader( std::string_view path, ShaderStage stage ) -> void {
        m_PipelineDescription.UseShader( path, stage );
    }

    auto FramePassBuilder::UseSrg( SRGType type ) -> void {
        switch (type) {
            case SRGType::SRG_Textures:
                m_UsesTextures = true;
                break;
            default: ;
        }

    }

    auto FramePassBuilder::UseSrg( SRGType type, std::string_view name, UInt32 bindSlot ) -> void {
        switch (type) {
            case SRGType::SRG_PerPass:
                m_PassShaderResources.SetBuffer( name, bindSlot );
                break;
            default: ;
        }
    }

    auto FramePassBuilder::GetPass() -> FramePassNode * {
        return m_Node;
    }

    auto FramePassBuilder::GetShaderResources() const -> const SRGPerPass & {
        return m_PassShaderResources;
    }

    auto FramePassBuilder::CreateBuffer( std::string_view name, BufferDescription description ) -> void {
        m_Creates[std::string{ name }].Type = FrameResourceType::BUFFER;
        m_Creates[std::string{ name }].Description = description;
    }

    auto FramePassBuilder::CreateBuffer( std::string_view name, BufferUsage usage, Size sizeBytes ) -> void {
        BufferDescription description{};
        description
                .WithUsage( usage )
                .WithSizeBytes( sizeBytes );
        CreateBuffer( name, description );
    }

    auto FramePassBuilder::CreateBuffer( std::string_view name, BufferUsage usage, Size size, Size elementCount ) -> void {
        BufferDescription description{};
        description
                .WithUsage( usage )
                .ForElement( size, elementCount );
        CreateBuffer( name, description );
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, TextureDescription description ) -> void {
        m_Creates[std::string{ name }].Type = FrameResourceType::TEXTURE;
        m_Creates[std::string{ name }].Description = description;
    }

    auto FramePassBuilder::CreatePipeline( std::string_view name, const GraphicsPipelineDescription &description ) -> void {
        m_PipelineDescription.Name = name;
        m_PipelineDescription.Description = description;
    }

    auto FramePassBuilder::CreatePipeline( std::string_view name, const ComputePipelineDescription &description ) -> void {
        m_PipelineDescription.Name = name;
        m_PipelineDescription.Description = description;
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, TextureCubeCreateDescription description ) -> void {
        m_Creates[std::string{ name }].Type = FrameResourceType::TEXTURE;
        m_Creates[std::string{ name }].Description = description;
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, RenderResolution resolution, TextureFormat format, TextureUsage usage ) -> void {
        auto scale{ InferDimensions( resolution ) };
        CreateTexture( name, scale.first, scale.second, format, usage );
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format, TextureUsage usage ) -> void {
        TextureDescription colorDesc{};
        colorDesc.WithWidth( width )
                 .WithHeight( height )
                 .WithChannelCount( 4 )
                 .WithData( nullptr )
                 .WithType( TextureType::TEXTURE_2D )
                 .WithTextureUsage( usage )
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

    FrameGraph::FrameGraph( GraphicsContext *context, GpuDevice *device )
        : m_GraphicsContex{ context }, m_Device{ device } {}

    auto FrameGraph::Compile() -> void {
        SortPassExecution();
    }

    auto FrameGraph::Execute() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!IsCompiled()) {
            return;
        }

        m_GraphicsContex->BeginFrame();

        for (auto & node: m_Passes | std::views::values) {
            if (node.IsActive()) {
                CommandContext commandContext{ m_GraphicsContex, m_Device };
                commandContext.BeginPass(node);

                if (UsesTextureList( node.Name )) {
                    commandContext.UseTextureList();
                }

                node.ExecuteCallback( commandContext, m_GraphBlackboard );

                commandContext.EndPass();
            }
        }

        m_GraphicsContex->EndFrame();
    }

    auto FrameGraph::IsCompiled() const -> bool {
        return m_Compiled;
    }

    auto FrameGraph::GetTexture( std::string_view name ) const -> TextureHandle {
        return m_GraphicsContex->GetTexture( name );
    }

    auto FrameGraph::GetBuffer( std::string_view name ) const -> BufferHandle {
        return m_GraphicsContex->GetBuffer( name );
    }

    auto FrameGraph::Create( GraphicsContext *context, GpuDevice *device ) -> Unique<FrameGraph> {
        return CreateScope<FrameGraph>( context, device );
    }

    auto FrameGraph::UsesTextureList(std::string_view nodeName) const -> bool {
        return m_TexturePasses.contains( StringUtil::From( nodeName ) );
    }

    auto FrameGraph::IsFramePassPresent( const std::string_view name ) const -> bool {
        return m_Passes.contains( std::string{ name } );
    }

    auto FrameGraph::CreatePassNode( std::string_view name ) -> FramePassNode & {
        MKT_ASSERT( !IsFramePassPresent( name ), StringUtil::Format("Cannot register pass {} more than once", name) );
        return m_Passes.emplace( StringUtil::From( name ), FramePassNode{ .Name{ name }} ).first->second;
    }

    auto FrameGraph::CreateCommitedResources( FramePassBuilder &builder ) -> void {
        for (auto &[resourceName, resourceDescription]: builder.m_Creates) {
            CreateResource( resourceName, resourceDescription );
        }

        if (builder.m_UsesTextures) {
            m_TexturePasses.emplace( builder.GetPass()->Name );
        }

        m_GraphicsContex->CreateShaderResources(builder.GetPass()->Name, builder.m_PipelineDescription);
        m_GraphicsContex->CommitShaderResources( builder.GetPass()->Name, builder.m_PassShaderResources );
    }

    auto FrameGraph::SortPassExecution() -> void {
        ankerl::unordered_dense::map<std::string, std::string> resourceWriters;

        for (auto &[passName, node]: m_Passes) {
            for (const auto &w: node.Writes) {
                resourceWriters[w.Name] = passName;
            }
        }

        struct SortNode {
            FramePassNode *Node{};
            UInt32 Incoming{};               // number of prerequisites
            std::vector<SortNode *> Outgoing{};// passes that depend on this pass
        };

        // Create SortNode objects
        ankerl::unordered_dense::map<std::string, SortNode> sortNodes{};

        for (auto &[passName, node]: m_Passes) {
            sortNodes[passName].Node = &node;
        }

        // Build edges
        for (auto &[passName, node]: m_Passes) {
            SortNode &reader{ sortNodes[passName] };

            auto addDependency = [&]( const ResourceNode &res ) {
                auto it{ resourceWriters.find( res.Name ) };
                if (it == resourceWriters.end()) {
                    return;
                }

                const std::string &writerName{ it->second };

                // ignore self-dependency
                if (writerName == passName) {
                    return;
                }

                SortNode &writer{ sortNodes[writerName] };

                writer.Outgoing.push_back( &reader );
                reader.Incoming++;
            };

            // Add dependencies for reads only
            for (const auto &r: node.Reads) {
                addDependency( r );
            }
        }

        std::vector<FramePassNode> sorted{};
        sorted.reserve( m_Passes.size() );

        std::queue<SortNode *> ready{};

        // Push all passes with no incoming edges
        for (auto &[name, sn]: sortNodes) {
            if (sn.Incoming == 0) {
                ready.push( &sn );
            }
        }

        while (!ready.empty()) {
            SortNode *sn{ ready.front() };
            ready.pop();

            sorted.push_back( *sn->Node );

            for (SortNode *dep: sn->Outgoing) {
                if (--dep->Incoming == 0) {
                    ready.push( dep );
                }
            }
        }

        if (sorted.size() != m_Passes.size()) {
            MKT_CORE_LOGGER_ERROR( "FrameGraph cycle detected in pass dependencies!" );
            return;
        }

        ankerl::unordered_dense::map<std::string, FramePassNode> newOrder{};

        for (auto &pass: sorted) {
            newOrder[pass.Name] = pass;
        }

        m_Passes = std::move( newOrder );
        m_Compiled = true;
    }

    auto FrameGraph::CreateResource( std::string_view name, FrameResource resource ) -> void {
        switch (resource.Type) {
            case FrameResourceType::TEXTURE:
                if (std::holds_alternative<TextureDescription>( resource.Description )) {
                    m_GraphicsContex->CreateTexture( name, std::get<TextureDescription>( resource.Description ) );
                } else if (std::holds_alternative<TextureCubeCreateDescription>( resource.Description )) {
                    m_GraphicsContex->CreateTexture( name, std::get<TextureCubeCreateDescription>( resource.Description ) );
                }
                break;
            case FrameResourceType::BUFFER:
                if (std::holds_alternative<BufferDescription>( resource.Description )) {
                    m_GraphicsContex->CreateBuffer( name, std::get<BufferDescription>( resource.Description ) );
                }
                break;
        }
    }
}// namespace Mikoto