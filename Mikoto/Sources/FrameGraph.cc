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
#include <Renderer/Core/Barrier.hh>

#include "Core/Profiler.hh"
#include "Core/Timer.hh"

namespace Mikoto {

    auto FramePassNode::MarkDirty() -> void {
        IsDirty = true;
        Status = FramePassNodeStatus::ACTIVE;

        // Need to rerun the pass so we clear this flag too
        HasExecuted = false;
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
        this->IsBuilt = true;
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

        m_Creates[std::string{ description.Name }].Type = FrameResourceType::BUFFER;
        m_Creates[std::string{ description.Name }].Description = desc;
    }

    auto FramePassBuilder::UseShader( std::string_view path, ShaderStage stage ) -> FramePassBuilder& {
        m_PipelineDescription.UseShader( path, stage );
        return *this;
    }

    auto FramePassBuilder::GetPass() -> FramePassNode * {
        return m_Node;
    }

    auto FramePassBuilder::CreateBuffer( std::string_view name, BufferDescription description) -> void {
        m_Creates[std::string{ name }].Type = FrameResourceType::BUFFER;
        m_Creates[std::string{ name }].Description = description;
    }

    auto FramePassBuilder::CreateBuffer( std::string_view name, BufferUsage usage, Size sizeBytes, ResourceUsageType usageType ) -> void {
        BufferDescription description{};
        description
            .WithUsage( usage )
            .WithSizeBytes( sizeBytes )
            .WithResourceUsageType( usageType );

        CreateBuffer( name, description );
    }

    auto FramePassBuilder::CreateBuffer( std::string_view name, BufferUsage usage, Size size, Size elementCount, ResourceUsageType usageType ) -> void {
        BufferDescription description{};
        description
            .WithUsage( usage )
            .WithResourceUsageType( usageType )
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

    auto FramePassBuilder::CreateTexture( std::string_view name, RenderResolution resolution, TextureFormat format, Multisampling msaa, TextureUsage usage, UInt32 mipLevelCount ) -> void {
        const auto [width, height] { InferDimensions( resolution ) };

        TextureDescription colorDesc{};
        colorDesc.WithWidth( width )
                 .WithHeight( height )
                 .WithChannelCount( 4 ) // TODO: infer from texture, depth ones usually have one channel
                 .WithData( nullptr )
                 .WithType( TextureType::TEXTURE_2D )
                 .WithTextureUsage( usage )
                 .WithSampleCount( msaa )
                 .WithMipLevelCount( mipLevelCount )
                 .WithFormat( format )
                 .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        CreateTexture( name, colorDesc );
    }

    auto FramePassBuilder::CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format, Multisampling msaa, TextureUsage usage ) -> void {
        TextureDescription colorDesc{};
        colorDesc.WithWidth( width )
                 .WithHeight( height )
                 .WithChannelCount( 4 )
                 .WithData( nullptr )
                 .WithType( TextureType::TEXTURE_2D )
                 .WithTextureUsage( usage )
                 .WithSampleCount( msaa )
                 .WithFormat( format )
                 .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

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

    auto FramePassBuilder::HasPipeline() const -> bool {
        return m_HasActivePipeline;
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

        for (auto& [name, node]: m_Passes) {
            if (!node.ShouldRun()) {
                continue;
            }

            CommandListHandle cmd{ InitializeCommandList( node.Type ) };
            InsertResourceBarriers( node, cmd );

            CommandContext commandContext{ m_GraphicsContex, node, m_ResourcesByNames };
            commandContext.BeginPass( cmd );

            Timer passTimer{ false };
            node.ExecuteCallback( commandContext, m_GraphBlackboard );

            // Read pass elapsed time
            double currentTime{ TimeService::Get()->GetTime( TimeUnit::MILLISECONDS ) };
            if ( currentTime - m_ElapsedTime > m_ElapsedTimeUpdatedInterval || 
                node.IsExecutionPolicy( FramePassExecutionPolicy::ON_CHANGE ) || 
                node.IsExecutionPolicy( FramePassExecutionPolicy::ONCE ) ) 
            {
                node.LastExecutionTime.Value = passTimer.GetCurrentProgress( node.LastExecutionTime.Unit );
                m_ElapsedTime = currentTime;
            }

            commandContext.EndPass();
        }

        SubmitCommandLists();
        m_GraphicsContex->EndFrame();
    }

    auto FrameGraph::SubmitCommandLists() -> void {
        // End recording commands to the command list and then submit for execution
        if (!m_GraphicsCommandList.IsEmpty()) {
            m_GraphicsCommandList->End();

            m_Device->SubmitCommands( m_GraphicsCommandList );
            m_GraphicsCommandList.Reset();
        }

        if ( !m_ComputeCommandList.IsEmpty() ) {
            m_ComputeCommandList->End();

            m_Device->SubmitCommands( m_ComputeCommandList );
            m_ComputeCommandList.Reset();
        }

        if ( !m_TransferCommandList.IsEmpty() ) {
            m_TransferCommandList->End();

            m_Device->SubmitCommands( m_TransferCommandList );
            m_TransferCommandList.Reset();
        }
    }

    auto FrameGraph::InitializeCommandList( FramePassNodeType type ) -> CommandListHandle {
        CommandListHandle cmd{ CommandListHandle::CreateEmpty() };

        switch (type) {
            case FramePassNodeType::GRAPHICS:
                if ( m_GraphicsCommandList.IsEmpty() ) {
                    m_GraphicsCommandList = m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE, false );
                    m_GraphicsCommandList->Begin();

                }
                   
                cmd = m_GraphicsCommandList;
                break;

            case FramePassNodeType::COMPUTE:
                if ( m_ComputeCommandList.IsEmpty() ) {
                    m_ComputeCommandList = m_Device->CreateCommandList( QueueType::COMPUTE_QUEUE, false );
                    m_ComputeCommandList->Begin();


                }
                cmd = m_ComputeCommandList;
                break;

            case FramePassNodeType::TRANSFER:
                if ( m_TransferCommandList.IsEmpty() ) {
                    m_TransferCommandList = m_Device->CreateCommandList( QueueType::TRANSFER_QUEUE, false );
                    m_TransferCommandList->Begin();
                }

                cmd = m_TransferCommandList;
                break;

            default:
                break;
        }

        return cmd;
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

    auto FrameGraph::InsertResourceBarriers( FramePassNode& node, CommandListHandle cmd ) -> void {
        auto &barrierList{ m_CommandBarriers[cmd.GetRaw()] };

        for (const auto &resourceNode : node.Reads) {
            const auto it{ m_ResourcesByNames.Resources.find( resourceNode.Name ) };
            if (it != m_ResourcesByNames.Resources.end()) {
                ResourceBarrierInfo info{
                    .Name{ resourceNode.Name },
                    .Type{ it->second.Type },
                    .PreState{ it->second.CurrentState },
                    .PostState{ resourceNode.OutState },
                };

                barrierList.emplace_back( info );
                
                it->second.CurrentState = resourceNode.OutState;
            }

        }

        for (const auto &resourceNode : node.Writes) {
            const auto it{ m_ResourcesByNames.Resources.find( resourceNode.Name ) };
            if (it != m_ResourcesByNames.Resources.end()) {
                ResourceBarrierInfo info{
                    .Name{ resourceNode.Name },
                    .Type{ it->second.Type },
                    .PreState{ it->second.CurrentState },
                    .PostState{ resourceNode.OutState },
                };

                barrierList.emplace_back( info );

                it->second.CurrentState = resourceNode.OutState;
            }
        }

        m_GraphicsContex->InsertResourceBarrierBatch( barrierList, cmd );
        barrierList.clear();
    }

    auto FrameGraph::IsFramePassPresent( const std::string_view name ) const -> bool {
        return m_Passes.contains( std::string{ name } );
    }

    auto FrameGraph::CreatePassNode( std::string_view name, FramePassNodeType type ) -> FramePassNode & {
        MKT_ASSERT( !IsFramePassPresent( name ), StringUtil::Format("Cannot register pass {} more than once", name) );
        auto& node{ m_Passes.emplace( StringUtil::From( name ), FramePassNode{ .Name{ name }} ).first->second };

        node.Type = type;

        return node;
    }

    auto FrameGraph::CreateCommitedResources( FramePassBuilder &builder ) -> void {
        for (auto &[resourceName, resourceDescription]: builder.m_Creates) {
            CreateResource( resourceName, resourceDescription );
        }

        if (builder.HasPipeline()) {
            m_GraphicsContex->PrepareResourceBindings(builder.GetPass()->Name, builder.m_PipelineDescription);
        }
    }

    auto FrameGraph::SortPassExecution() -> void {
        // TODO: check also that every resource has t least one writer
        ankerl::unordered_dense::map<std::string, std::string> resourceWriters{};

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
            m_ResourcesByNames.Resources.emplace( name, FramePassResource{
                .Handle{ texture },
                .Type{ FrameResourceType::TEXTURE },
                .CurrentState{ FrameResourceState::Undefined }
            } );
        }

        if (!buffer.IsEmpty()) {
            m_ResourcesByNames.Resources.emplace( name,  FramePassResource{
                .Handle{ buffer },
                .Type{ FrameResourceType::BUFFER },
                .CurrentState{ FrameResourceState::Undefined }
            } );
        }
    }

    auto FrameGraph::DebugDumpResources() -> void {

    }
}// namespace Mikoto