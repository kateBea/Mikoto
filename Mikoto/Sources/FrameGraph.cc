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

#include <ranges>

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <taskflow/taskflow.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>
#include <Filesystem/FileService.hh>

#include <Material/ShaderLibrary.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

namespace mikoto::renderer {

// Must match shaders
#define MKT_DEFAULT_REGISTER_SPACE 0

#define MKT_CONSTANT_BUFFER_BINDING 0
#define MKT_STRUCTURED_BUFFER_BINDING 1

#define MKT_RW_CONSTANT_BUFFER_BINDING 2
#define MKT_RW_STRUCTURED_BUFFER_BINDING 3

#define MKT_SAMPLER_BINDING 4
#define MKT_TEXTURE_2D_BINDING 5
#define MKT_TEXTURE_CUBE_BINDING 6

#define MKT_ACCELERATION_STRUCTURE_BINDING 7

    using namespace mikoto::core;
    using namespace mikoto::material;

    MKT_NODISCARD constexpr auto GetShaderFlagsFromStage( FrameGraphStageType type ) -> ShaderType {
        switch (type) {
            case FrameGraphStageType::eVertex: return ShaderType::eVertex;
            case FrameGraphStageType::eFragment: return ShaderType::eFragment;
            case FrameGraphStageType::eCompute: return ShaderType::eCompute;
            default:;
        }

        return ShaderType::eInvalid;
    }

    auto FrameGraphPipelineDescription::SetName( eastl::string_view name ) -> FrameGraphPipelineDescription & {
        mName = name;
        return *this;
    }

    auto FrameGraphPipelineDescription::SetPipelineType( PipelineType type ) -> FrameGraphPipelineDescription & {
        mPipelineType = type;
        return *this;
    }

    auto FrameGraphPipelineDescription::PushShader( const Path &path, FrameGraphStageType stage ) -> FrameGraphPipelineDescription & {
        mShaders[stage] = path;
        return *this;
    }

    auto FrameGraphBufferDescription::SetName( eastl::string_view name ) -> FrameGraphBufferDescription & {
        mName = name;
        return *this;
    }

    auto FrameGraphBufferDescription::SetUsage( BufferUsageFlags flags ) -> FrameGraphBufferDescription & {
        mBufferUsageFlags = flags;
        return *this;
    }

    auto FrameGraphBufferDescription::SetSizeBytes( size_t byteSize ) -> FrameGraphBufferDescription & {
        mElementSizeBytes = byteSize;
        return *this;
    }

    auto FrameGraphBufferDescription::SetElementsSize( u32 elementCount, size_t elementSizeBytes ) -> FrameGraphBufferDescription & {
        mElementCount = elementCount;
        mElementSizeBytes = elementSizeBytes;
        return *this;
    }

    auto FrameGraphBufferDescription::SetCpuAccess( HeapType heap ) -> FrameGraphBufferDescription & {
        mHeapType = heap;
        return *this;
    }

    auto FrameGraphBufferDescription::SetResourceType( ResourceType resource ) -> FrameGraphBufferDescription & {
        mResourceType = resource;
        return *this;
    }

    auto FrameGraphTextureDescription::SetName( eastl::string_view name ) -> FrameGraphTextureDescription & {
        mName = name;
        return *this;
    }

    auto FrameGraphTextureDescription::SetWidth(u32 width) -> FrameGraphTextureDescription& {
        mWidth = width;
        return *this;
    }

    auto FrameGraphTextureDescription::SetHeight(u32 height) -> FrameGraphTextureDescription& {
        mHeight = height;
        return *this;
    }

    auto FrameGraphTextureDescription::SetMipCount(u32 count) -> FrameGraphTextureDescription& {
        mMipCount = count;
        return *this;
    }

    auto FrameGraphTextureDescription::SetHeapType(HeapType heapType) -> FrameGraphTextureDescription& {
        mHeapType = heapType;
        return *this;
    }

    auto FrameGraphTextureDescription::SetMultisampling(Multisampling sampleCount) -> FrameGraphTextureDescription& {
        mMSAA = sampleCount;
        return *this;
    }

    auto FrameGraphTextureDescription::SetDimensions(TextureDimension dimensions) -> FrameGraphTextureDescription& {
        mDimension = dimensions;
        return *this;
    }

    auto FrameGraphTextureDescription::SetFormat(Format format) -> FrameGraphTextureDescription& {
        mFormat = format;
        return *this;
    }

    auto FrameGraphTextureDescription::SetUsage(TextureUsageFlags usage) -> FrameGraphTextureDescription& {
        mUsage = usage;
        return *this;
    }

    auto FrameGraphTextureDescription::SetResourceType(ResourceType type) -> FrameGraphTextureDescription& {
        mResourceType = type;
        return *this;
    }

    auto FrameGraphResource::HasResource() const -> bool {
        return !mHandle.IsEmpty();
    }

    FrameGraphResourceManager::FrameGraphResourceManager( size_t initialIdsCount ) {
        constexpr ResourceID kValidIdStart{ 1 };
        for (ResourceID count{ kValidIdStart }; count < initialIdsCount; count++) {
            mFreeIds.emplace( count );
            mResources[count] = eastl::make_unique<FrameGraphResource>();
        }
    }
    auto FrameGraphResourceManager::Get( ResourceID id ) -> FrameGraphResource & {
        MKT_ASSERT( mResources.contains( id ), "Resource with ID does not exist" );
        return *mResources.at( id );
    }

    auto FrameGraphResourceManager::Get( ResourceID id ) const -> const FrameGraphResource & {
        MKT_ASSERT( mResources.contains( id ), "Resource with ID does not exist" );
        return *mResources.at( id );
    }

    auto FrameGraphResourceManager::Allocate() -> FrameGraphResource& {
        if (mFreeIds.empty()) {
            // Resize
        }

        // Return the first available
        FrameGraphResource& result{ *mResources.at( *mFreeIds.begin() ) };
        mFreeIds.erase( mFreeIds.begin() );

        return result;
    }

    auto FrameGraphResourceManager::Free( ResourceID id ) -> bool {
        return false;
    }

    FrameGraphNodeBuilder::FrameGraphNodeBuilder( ShaderLibrary* shaderLibrary, GpuDevice* device, FrameGraphNode* node, FrameGraphResourceManager* resourceManager )
        : mDevice{ device }, mGraphNode{ node }, mResourceManager{ resourceManager }, mShaderLibrary{ shaderLibrary }
    {}

    auto FrameGraphNodeBuilder::Create( const FrameGraphPipelineDescription &desc ) -> ResourceID {
        FrameGraphResource& result{ mResourceManager->Allocate() };

        // All pipelines are bindless
        auto layoutDesc{ BindlessLayoutDescription{}
            .SetVisibility(ShaderFlagsBits::kAll)
            .SetRegisterSpace(MKT_DEFAULT_REGISTER_SPACE)
            .AddBindlessItem(BindlessLayoutItem::Samplers(MKT_SAMPLER_BINDING, 1024))
            .AddBindlessItem(BindlessLayoutItem::Texture2D_SRV(MKT_TEXTURE_2D_BINDING, 4096))
            .AddBindlessItem(BindlessLayoutItem::ConstantBuffer_SRV(MKT_CONSTANT_BUFFER_BINDING, 8192))
            .AddBindlessItem(BindlessLayoutItem::StructuredBuffer_SRV(MKT_STRUCTURED_BUFFER_BINDING, 16384))
            .AddBindlessItem(BindlessLayoutItem::ConstantBuffer_UAV(MKT_RW_CONSTANT_BUFFER_BINDING, 8192))
            .AddBindlessItem(BindlessLayoutItem::StructuredBuffer_UAV(MKT_RW_STRUCTURED_BUFFER_BINDING, 16384))
            .AddBindlessItem(BindlessLayoutItem::TextureCube_SRV(MKT_TEXTURE_CUBE_BINDING, 4096))};
            //.AddBindlessItem(BindlessLayoutItem::AccelerationStructures(MKT_ACCELERATION_STRUCTURE_BINDING, 1024)) };

        rhi::BindingLayoutHandle bl{ mDevice->CreateBindlessLayout( layoutDesc ) };
        rhi::PipelineLayoutHandle pl = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{}
            .AddBindingLayout( bl ));

        // Load shaders
        if ( desc.mPipelineType == PipelineType::eGraphics ) {
            MKT_ASSERT( !desc.mShaders.empty(), "Creating graphics pipeline without shaders." );
            auto graphicsPipelineDesc{ GraphicsPipelineDescription{}
                .SetPipelineLayout( pl )
            };
            for ( const auto &shader: desc.mShaders ) {
                graphicsPipelineDesc.AddShader( mShaderLibrary->LoadShader(shader.second, GetShaderFlagsFromStage( shader.first )) );
            }

            result.mHandle = mDevice->CreatePipeline( graphicsPipelineDesc );
        } else if ( desc.mPipelineType == PipelineType::eCompute ) {
            MKT_ASSERT( desc.mShaders.contains( FrameGraphStageType::eCompute ), "Creating compute pipeline without compute shader." );
            auto computePipelineDesc{ ComputePipelineDescription{}
                .SetPipelineLayout( pl )
                .SetComputeStage( mShaderLibrary->LoadShader( desc.mShaders.at( FrameGraphStageType::eCompute ), ShaderType::eCompute ) )
            };

            result.mHandle = mDevice->CreatePipeline( computePipelineDesc );
        }

        return result.mResourceID;
    }

    auto FrameGraphNodeBuilder::Create( const FrameGraphBufferDescription &desc ) -> ResourceID {
        auto& resource{ mResourceManager->Allocate() };
        auto bufferDesc{ BufferCreateDescription{}
            .SetBufferUsage( desc.mBufferUsageFlags )
            .SetResourceType( desc.mResourceType )
            .SetName( desc.mName )
            .ForElement( desc.mElementSizeBytes, desc.mElementCount )
            .SetCpuAccessType( desc.mHeapType == HeapType::eUpload ? CpuAccessType::eWrite : CpuAccessType::eRead )
            .SetByteSize( desc.mElementSizeBytes )
        };

        resource.mHandle = mDevice->CreateBuffer( bufferDesc );
        return resource.mResourceID;
    }

    auto FrameGraphNodeBuilder::Create( const FrameGraphTextureDescription &desc ) -> ResourceID {
        auto& resource{ mResourceManager->Allocate() };
        auto textureDesc{ TextureCreateDescription{}
            .SetName( desc.mName )
            .SetWidth( desc.mWidth )
            .SetHeight( desc.mHeight )
            .SetDimensions( desc.mDimension )
            .SetMultisampling( desc.mMSAA )
            .SetUsage( desc.mUsage )
            .SetFormat( desc.mFormat ) };

        resource.mHandle = mDevice->CreateTexture( textureDesc );
        return resource.mResourceID;
    }

    auto FrameGraphNodeBuilder::Write( ResourceID resource, FrameGraphResourceAccessType accessType, FrameGraphStageType shaderStage ) -> void {

    }

    FrameGraph::FrameGraph( GpuDevice *device )
        : mDevice{ device } {
        const ShaderLibraryDescription description{
            .mDevice = mDevice,
            .mRootPath{ "Resources/Shaders/slang" },
        };

        mShaderLibrary = eastl::make_unique<ShaderLibrary>( description );
        if (mShaderLibrary) {
            mShaderLibrary->Initialize();
        }

        mResourceManager = eastl::make_unique<FrameGraphResourceManager>( 10 );

    }

    auto FrameGraph::Compile() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Build execution graph
        for (auto& node : mNodes | std::views::values ) {
            mExecutionGraph.emplace( [ctx = CommandContext{ mDevice, node.get(), mResourceManager.get() },
                graphNode = node.get(), this]() mutable -> void {
                graphNode->mCallback( ctx, mBlackboard);
            } );
        }

        // Once we have the individual task constructed, we build the dependencies
    }

    auto FrameGraph::Execute() -> void {
        MKT_BEGIN_PROFILER_NAMED();
    }

    auto FrameGraph::Create( GpuDevice *device ) -> eastl::unique_ptr<FrameGraph> {
        MKT_BEGIN_PROFILER_NAMED();
        return eastl::make_unique<FrameGraph>( device );
    }

    FrameGraph::~FrameGraph() {
        mShaderLibrary->Shutdown();
        mShaderLibrary.reset();
    }

    auto FrameGraph::CreateGpuResources( FrameGraphNodeBuilder &builder ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
    }

    auto FrameGraph::PushNode( eastl::string_view passName, FrameGraphNodeType type ) -> FrameGraphNode & {
        MKT_BEGIN_PROFILER_NAMED();
        eastl::string name{ passName };

        MKT_ASSERT( !mNodes.contains( name ), string::Format("Node {} already exists", name ) );
        const auto it{ mNodes.try_emplace( name, eastl::make_unique<FrameGraphNode>( name ) ) };
        return *it.first->second;
    }
}// namespace Mikoto