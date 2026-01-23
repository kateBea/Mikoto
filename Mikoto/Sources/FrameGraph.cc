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

#include <Material/ShaderLibrary.hh>

#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/GraphicsContext.hh>

namespace Mikoto {

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
        m_Pipelines[std::string{ name }].Type = FrameResourceType::PIPELINE;
        m_Pipelines[std::string{ name }].Description = description;
    }

    auto FrameGraphBuilder::CreateNamedRenderTarget( std::string_view name, TextureDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::RENDER_TARGET;
        m_Resources[std::string{ name }].Description = description;
    }

    auto FrameGraphBuilder::CreateNamedRenderTarget( std::string_view name, TextureCubeCreateDescription description ) -> void {
        m_Resources[std::string{ name }].Type = FrameResourceType::RENDER_TARGET;
        m_Resources[std::string{ name }].Description = description;
    }

    auto FrameGraphBuilder::CreateColorRenderTarget( std::string_view name, UInt32 width, UInt32 height, TextureFormat format ) -> void {
        TextureDescription colorDesc{};
        colorDesc.WithWidth( width )
            .WithHeight( height )
            .WithChannelCount( 4 )
            .WithData( nullptr )
            .WithType( TextureType::TEXTURE_2D )
            .WithTextureUsage( TextureUsage::TEXTURE_USAGE_COLOR )
            .WithFormat( format )
            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        CreateNamedRenderTarget( name, colorDesc );
    }

    auto FrameGraphBuilder::CreateDepthRenderTarget( std::string_view name, UInt32 width, UInt32 height, TextureFormat format ) -> void {
        TextureDescription depthDesc{};
        depthDesc.WithWidth( width )
            .WithHeight( height )
            .WithChannelCount( 1 )
            .WithData( nullptr )
            .WithType( TextureType::TEXTURE_2D )
            .WithTextureUsage( TextureUsage::TEXTURE_USAGE_DEPTH )
            .WithFormat( format )
            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        CreateNamedRenderTarget( name, depthDesc );
    }

    auto FrameGraphBuilder::CreateCubeRenderTarget( std::string_view name, UInt32 dimensions, TextureFormat format, UInt32 mipLevels ) -> void {
        TextureCubeCreateDescription depthDesc{};
        depthDesc.WithUsageType( TextureUsage::TEXTURE_USAGE_RENDER_TARGET )
            .WithMipLevels( mipLevels )
            .WithTextureFormat( format )
            .WithDimensions( dimensions )
            .WithResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

        CreateNamedRenderTarget( name, depthDesc );
    }

    auto FrameGraphBuilder::Clear() -> void {
        m_Nodes.clear();
        m_Pipelines.clear();
        m_Resources.clear();
    }

    FrameGraph::FrameGraph( GraphicsContext* context, GpuDevice* device)
        : m_GraphicsContex{ context }, m_Device{ device }
    {
        m_Blackboard = CreateScope<FrameBlackboard>( device );
    }

    auto FrameGraph::RegisterPass( FramePass *pass ) -> void {
        m_Nodes.emplace_back( pass );
    }

    auto FrameGraph::Compile( ) -> void {
        RunPassSetups();

        RunPassDependencyCallbacks();

        // Create the actual resources on the GPU
        for (auto& [resourceName, resourceDescription] : m_Builder.m_Resources) {
            RegisterResource( resourceName, resourceDescription );
        }

        // Register the actual pipelines
        for (auto& [resourceName, resourceDescription] : m_Builder.m_Pipelines) {
            RegisterResource( resourceName, resourceDescription );
        }

        SortPassExecution();
    }

    auto FrameGraph::Execute() -> void {
        m_GraphicsContex->BeginFrame( GetBlackboard() );

        for ( const auto& [pass, input, outputs] : m_Nodes) {

            if (pass->ShouldRun()) {
                CommandContext context{ m_GraphicsContex, GetBlackboard(), m_Device };
                pass->Execute( context );

                pass->PostExecute();
            }
        }

        m_GraphicsContex->EndFrame();
    }

    auto FrameGraph::GetBlackboard() const -> FrameBlackboard * {
        return m_Blackboard.get();
    }

    auto FrameGraph::Create( GraphicsContext* context, GpuDevice* device ) -> Unique<FrameGraph> {
        return CreateScope<FrameGraph>( context, device );
    }

    auto FrameGraph::RunPassSetups() -> void {
        for (const auto &pass: m_Nodes) {
            pass.Pass->Setup( m_Builder );
        }
    }

    auto FrameGraph::RunPassDependencyCallbacks() -> void {
        for (const auto &pass: m_Nodes) {
            pass.Pass->SetDependencies( m_Builder );
        }
    }

    auto FrameGraph::SortPassExecution() -> void {
        ankerl::unordered_dense::map<std::string, FramePass*> resourceWriters{};

        // Assumption: One writer per resource per frame

        for (auto& [pass, nodeData] : m_Builder.m_Nodes) {
            for (const auto& buf : nodeData.WriteBuffers) {
                resourceWriters[buf] = pass;
            }

            for (const auto& tex : nodeData.WriteTextures) {
                resourceWriters[tex] = pass;
            }
        }

        struct SortNode {
            FrameNode* Node{};
            UInt32 Incoming{};
            std::vector<SortNode*> Outgoing{};
        };

        ankerl::unordered_dense::map<FramePass*, SortNode> sortNodes{};

        for (FrameNode& node : m_Nodes) {
            sortNodes[node.Pass].Node = std::addressof( node );
        }

        for (auto& [pass, nodeData] : m_Builder.m_Nodes) {
            SortNode& readerNode{ sortNodes[pass] };

            auto addDependency{ [&](const std::string& resource) {
                auto it{ resourceWriters.find(resource) };
                if (it == resourceWriters.end())
                    return;

                FramePass* writer{ it->second };
                if (writer == pass)
                    return; // same pass, ignore

                SortNode& writerNode{ sortNodes[writer] };
                writerNode.Outgoing.push_back(&readerNode);
                readerNode.Incoming++;
            }};

            for (const auto& buf : nodeData.ReadBuffers)
                addDependency(buf);

            for (const auto& tex : nodeData.ReadTextures)
                addDependency(tex);
        }

        std::vector<FrameNode> sorted{};
        std::queue<SortNode*> ready{};

        for (auto& [_, sn] : sortNodes) {
            if (sn.Incoming == 0)
                ready.push(&sn);
        }

        while (!ready.empty()) {
            SortNode* sn{ ready.front() };
            ready.pop();

            sorted.push_back(*sn->Node);

            for (SortNode* dependent : sn->Outgoing) {
                if (--dependent->Incoming == 0)
                    ready.push(dependent);
            }
        }

        if (sorted.size() != m_Nodes.size()) {
            MKT_CORE_LOGGER_ERROR("FrameGraph cycle detected!");

        }

        m_Nodes = std::move(sorted);
        m_Compiled = true;
    }

    auto FrameGraph::RegisterResource(std::string_view name, FrameResource resource ) const -> void {
        switch (resource.Type) {
            case FrameResourceType::TEXTURE:
            case FrameResourceType::RENDER_TARGET:
                if (std::holds_alternative<TextureDescription>( resource.Description )) {
                    m_Blackboard->RegisterTexture( name, std::get<TextureDescription>( resource.Description ) );
                } else if (std::holds_alternative<TextureCubeCreateDescription>( resource.Description )) {
                    m_Blackboard->RegisterTexture( name, std::get<TextureCubeCreateDescription>( resource.Description ) );
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