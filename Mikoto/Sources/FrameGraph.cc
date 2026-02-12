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

    auto FramePassNode::MarkDirty() -> void {
        IsDirty = true;
        Status = FramePassNodeStatus::ACTIVE;
    }

    auto FramePassNode::IsActive() const -> bool {
        return IsStatus( FramePassNodeStatus::ACTIVE );
    }

    auto FramePassNode::IsSleeping() const -> bool {
        return IsStatus( FramePassNodeStatus::SLEEPING );
    }

    auto FramePassNode::IsStatus(FramePassNodeStatus status) const -> bool {
        return Status == status;
    }

    auto FramePassNode::IsExecutionPolicy( FramePassExecutionPolicy status ) const -> bool {
        return ExecutionPolicy == status;
    }

    auto FramePassNode::ShouldRun() const -> bool {
        if (!IsStatus(FramePassNodeStatus::ACTIVE)) {
            return false;
        }

        switch (ExecutionPolicy) {
            case FramePassExecutionPolicy::PER_FRAME:
                return true;

            case FramePassExecutionPolicy::ONCE:
                return !HasExecuted;

            case FramePassExecutionPolicy::ON_CHANGE:
                return IsDirty;
        }

        return false;
    }

    auto FramePassNode::HasResources() const -> bool {
        return !this->PerPassShaderResources.IsEmpty();
    }

    auto BufferBuilder::ForElement( Size size, Size count ) -> BufferBuilder & {
        this->ElementSize = size;
        this->ElementCount = count;
        return *this;
    }

    auto BufferBuilder::WithSizeBytes( Size size ) -> BufferBuilder & {
        this->SizeBytes = size;
        return *this;
    }

    auto BufferBuilder::WithUsage( BufferUsage usage ) -> BufferBuilder & {
        this->Usage = usage;
        return *this;
    }

    auto BufferBuilder::IsDynamic( bool value ) -> BufferBuilder & {
        this->UsageType = value ? ResourceUsageType::RESOURCE_USAGE_DYNAMIC
            : ResourceUsageType::RESOURCE_USAGE_STATIC;
        return *this;
    }

    auto BufferBuilder::Build( std::string_view name ) -> void {
        this->Name = name;
    }

    FramePassBuilder::FramePassBuilder( FramePassNode &node )
        : m_Node{ std::addressof( node ) } {}

    auto FramePassBuilder::Write( std::string_view name, FrameResourceState state ) -> FramePassBuilder& {
        m_Node->Writes.emplace_back( StringUtil::From( name ), state );
        return *this;
    }

    auto FramePassBuilder::Read( std::string_view name, FrameResourceState state ) -> FramePassBuilder& {
        m_Node->Reads.emplace_back( StringUtil::From( name ), state );
        return *this;
    }

    auto FramePassBuilder::CreateBuffer( const BufferBuilder &description ) -> void {
        MKT_ASSERT( description.IsBuilt, "Forgot to call build" );

        BufferDescription desc{};
        desc.WithUsage( description.Usage )
            .WithSizeBytes( description.SizeBytes )
            .ForElement( description.ElementSize, description.ElementCount )
            .WithResourceUsageType( description.UsageType );

        CreateBuffer( description.Name, desc );
    }

    auto FramePassBuilder::UseShader( std::string_view path, ShaderStage stage ) -> FramePassBuilder& {
        m_PipelineDescription.UseShader( path, stage );
        return *this;
    }

    auto FramePassBuilder::Use( SRGType type ) -> FramePassBuilder& {
        switch (type) {
            case SRGType::SRG_Textures:
                m_UsesTextures = true;
                break;
            default: ;
        }

        return *this;
    }

    auto FramePassBuilder::Use( SRGType type, std::string_view name, UInt32 bindSlot ) -> FramePassBuilder& {
        switch (type) {
            case SRGType::SRG_PerPass:
                m_Node->PerPassShaderResources.SetBuffer( name, bindSlot );
                break;
            default: ;
        }

        return *this;
    }

    auto FramePassBuilder::GetPass() -> FramePassNode * {
        return m_Node;
    }

    auto FramePassBuilder::CreateBuffer( std::string_view name, BufferDescription description ) -> void {
        // SSBOs and Uniforms are often updated better to mark them as such
        if (description.Usage == BufferUsage::SHADER_STORAGE || description.Usage == BufferUsage::UNIFORM) {
            description.UsageType = ResourceUsageType::RESOURCE_USAGE_DYNAMIC;
        }

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

        m_HasActivePipeline = true;
    }

    auto FramePassBuilder::CreatePipeline( std::string_view name, const ComputePipelineDescription &description ) -> void {
        m_PipelineDescription.Name = name;
        m_PipelineDescription.Description = description;

        m_HasActivePipeline = true;
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

    auto FramePassBuilder::CreateTexture( std::string_view name, RenderResolution resolution, TextureFormat format, Multisampling msaa, TextureUsage usage ) -> void {
        auto scale{ InferDimensions( resolution ) };
        CreateTexture( name, scale.first, scale.second, format, msaa, usage );
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format, Multisampling msaa, TextureUsage usage ) -> void {
        TextureDescription colorDesc{};
        colorDesc.WithWidth( width )
                 .WithHeight( height )
                 .WithChannelCount( 4 )
                 .WithData( nullptr )
                 .WithType( TextureType::TEXTURE_2D )
                 .WithTextureUsage( usage )
                 .WithFormat( format )
                 .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        colorDesc.MSAA = msaa;

        CreateTexture( name, colorDesc );
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format, void *ptr, Size sizeBytes ) -> void {
        TextureDescription colorDesc{};
        colorDesc.WithWidth( width )
                 .WithHeight( height )
                 .WithChannelCount( 4 )
                 .WithData( static_cast<Byte*>(ptr) )
                 .WithSize( sizeBytes )
                 .WithType( TextureType::TEXTURE_2D )
                 .WithTextureUsage( TextureUsage::COLOR )
                 .WithFormat( format )
                 .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        CreateTexture( name, colorDesc );
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, UInt32 dimensions, TextureFormat format, UInt32 mipLevels ) -> void {
        TextureCubeCreateDescription depthDesc{};
        depthDesc.WithUsageType( TextureUsage::RENDER_TARGET )
                 .WithMipLevels( mipLevels )
                 .WithTextureFormat( format )
                 .WithDimensions( dimensions )
                 .WithResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

        CreateTexture( name, depthDesc );
    }

    FrameGraph::FrameGraph( GraphicsContext *context, GpuDevice *device )
        : m_GraphicsContex{ context }, m_Device{ device } {}

    auto FrameGraph::Compile() -> void {
        for (auto& node : m_Passes | std::views::values) {
            m_GraphicsContex->UpdateResourceBindings( node.Name, node.PerPassShaderResources );
        }

        SortPassExecution();
    }

    auto FrameGraph::Execute() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!IsCompiled()) {
            return;
        }

        m_GraphicsContex->BeginFrame();

        for (auto & node: m_Passes | std::views::values) {
            if (node.ShouldRun()) {
                CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE, false ) };
                cmd->Begin();

                CommandContext commandContext{ m_GraphicsContex, cmd };
                commandContext.BeginPass(node);

                // Bindless textures bound once and active
                // for the entire recording of the command buffer
                if (UsesTextureList( node.Name )) {
                    commandContext.BindGlobalTextures();
                }
                m_GraphicsContex->BindShaderResources( node.Name, cmd );

                // Make sure the resources this pass consumes are in proper state
                InsertResourceBarriers(node, cmd);

                node.ExecuteCallback( commandContext, m_GraphBlackboard );

                commandContext.EndPass();

                cmd->End();
                m_Device->SubmitCommands( cmd );
            }
        }

        m_GraphicsContex->EndFrame();
    }

    auto FrameGraph::SetNodeExecutionPolicy( std::string_view name, FramePassExecutionPolicy policy ) -> void {
        MKT_ASSERT( IsFramePassPresent( name ), StringUtil::Format( "Node {} does not exist.", name ) );
        m_Passes[std::string{ name }].ExecutionPolicy = policy;
    }

    auto FrameGraph::IsCompiled() const -> bool {
        return m_Compiled;
    }

    auto FrameGraph::GetPassList() const -> const PassList& {
        return m_Passes;
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

    auto FrameGraph::TransitionWrites( FramePassNode &node, CommandListHandle cmd ) -> void {
        for (const auto &resourceNode : node.Writes) {
            const auto it{ m_ResourcesByNames.find( resourceNode.Name ) };
            if (it != m_ResourcesByNames.end()) {
                // This is temporary because these texture might used somewhere else for sampling from shaders (like ImGui viewports)
                // But the passes should either ask this explicitly in the Write method from the builder where
                // they would specify the state they leave the resource as
                if (it->second.IsResource( FrameResourceType::TEXTURE )) {
                    TextureHandle textureHandle{ it->second.Handle.As<Texture>() };

                    // if its cube
                    if (textureHandle.IsEmpty()) {
                        textureHandle = it->second.Handle.As<TextureCube>();
                    }

                    if (m_GraphicsContex->InsertResourceBarrier( textureHandle,
                        it->second.CurrentState, resourceNode.OutState, cmd )) {
                        it->second.CurrentState = resourceNode.OutState;
                    }
                }
            }
        }
    }

    auto FrameGraph::InsertBarrier( FramePassResource& resource, FrameResourceState newState, CommandListHandle cmd ) -> void {
        bool success{ false };
        if (resource.IsResource( FrameResourceType::BUFFER )) {
            success = m_GraphicsContex->InsertResourceBarrier( resource.Handle.As<Buffer>(), resource.CurrentState, newState, cmd );
        }

        if (resource.IsResource( FrameResourceType::TEXTURE )) {
            success = m_GraphicsContex->InsertResourceBarrier( resource.Handle.As<Texture>(), resource.CurrentState, newState, cmd );
        }

        if (success) {
            resource.CurrentState = newState;
        }
    }

    auto FrameGraph::InsertResourceBarriers( FramePassNode& node, CommandListHandle cmd ) -> void {
        for (const auto &resourceNode : node.Reads) {
            const auto it{ m_ResourcesByNames.find( resourceNode.Name ) };
            if (it != m_ResourcesByNames.end()) {
                InsertBarrier( it->second, resourceNode.OutState, cmd );
            }
        }

        for (const auto &resourceNode : node.Writes) {
            const auto it{ m_ResourcesByNames.find( resourceNode.Name ) };
            if (it != m_ResourcesByNames.end()) {
                InsertBarrier( it->second, resourceNode.OutState, cmd );
            }
        }
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

        if (builder.m_HasActivePipeline) {
            m_GraphicsContex->PrepareResourceBindings(builder.GetPass()->Name, builder.m_PipelineDescription);
        }
    }

    auto FrameGraph::SortPassExecution() -> void {
        // TODO: check also that every resource has t least one writer
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

    auto FrameGraph::CreateResource( std::string_view name, FramePassResourceDescription resource ) -> void {
        BufferHandle buffer{};
        TextureHandle texture{};

        switch (resource.Type) {
            case FrameResourceType::TEXTURE:
                if (std::holds_alternative<TextureDescription>( resource.Description )) {
                    texture = m_GraphicsContex->CreateTexture( name, std::get<TextureDescription>( resource.Description ) );
                } else if (std::holds_alternative<TextureCubeCreateDescription>( resource.Description )) {
                    texture = m_GraphicsContex->CreateTexture( name, std::get<TextureCubeCreateDescription>( resource.Description ) );
                }
                break;
            case FrameResourceType::BUFFER:
                if (std::holds_alternative<BufferDescription>( resource.Description )) {
                    buffer = m_GraphicsContex->CreateBuffer( name, std::get<BufferDescription>( resource.Description ) );
                }
                break;
            default: ;
        }

        // The resource can either be a texture or a buffer
        if (!texture.IsEmpty()) {
            m_ResourcesByNames.emplace( name, FramePassResource{
                .Handle{ texture },
                .Type{ FrameResourceType::TEXTURE },
                .CurrentState{ FrameResourceState::Undefined }
            } );
        }

        if (!buffer.IsEmpty()) {
            m_ResourcesByNames.emplace( name,  FramePassResource{
                .Handle{ buffer },
                .Type{ FrameResourceType::BUFFER },
                .CurrentState{ FrameResourceState::Undefined }
            } );
        }
    }

    auto FrameGraph::DebugDumpResources() -> void {

    }
}// namespace Mikoto