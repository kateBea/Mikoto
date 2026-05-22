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

#include <sstream>
#include <ranges>

#include <EASTL/queue.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <taskflow/taskflow.hpp>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Assets/AssetsService.hh>
#include <Assets/ImageProcessor.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Material/ShaderLibrary.hh>

#include <Memory/Allocator.hh>
#include <Memory/MemoryArena.hh>
#include <Memory/FreeListAllocator.hh>

#include <Threading/TaskService.hh>
#include <Threading/ThreadUtility.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::material;


    MKT_NODISCARD constexpr auto GetResourceState(FGResourceState state) -> ResourceStates {
        switch (state) {
            case FGResourceState::eUnknown:                return ResourceStates::eUnknown;

            // Buffers
            case FGResourceState::eConstantBuffer:         return ResourceStates::eConstantBuffer;
            case FGResourceState::eVertexBuffer:           return ResourceStates::eVertexBuffer;
            case FGResourceState::eIndexBuffer:            return ResourceStates::eIndexBuffer;
            case FGResourceState::eIndirectArgument:       return ResourceStates::eIndirectArgument;
            case FGResourceState::eShaderResource:         return ResourceStates::eShaderResource;
            case FGResourceState::eUnorderedAccess:        return ResourceStates::eUnorderedAccess;

            // Images
            case FGResourceState::eRenderTarget:           return ResourceStates::eRenderTarget;
            case FGResourceState::eDepthWrite:             return ResourceStates::eDepthWrite;
            case FGResourceState::eDepthRead:              return ResourceStates::eDepthRead;

            // Transfer
            case FGResourceState::eCopyDest:               return ResourceStates::eCopyDest;
            case FGResourceState::eCopySource:             return ResourceStates::eCopySource;
            case FGResourceState::eResolveDest:            return ResourceStates::eResolveDest;
            case FGResourceState::eResolveSource:          return ResourceStates::eResolveSource;

            // Present
            case FGResourceState::ePresent:                return ResourceStates::ePresent;

            // Raytracing
            case FGResourceState::eAccelStructRead:        return ResourceStates::eAccelStructRead;
            case FGResourceState::eAccelStructWrite:       return ResourceStates::eAccelStructWrite;
            case FGResourceState::eAccelStructBuildInput:  return ResourceStates::eAccelStructBuildInput;
            case FGResourceState::eAccelStructBuildBlas:   return ResourceStates::eAccelStructBuildBlas;
        }

        // Fallback (should never happen if enum is exhaustive)
        return ResourceStates::eUnknown;
    }

    MKT_NODISCARD constexpr auto GetShaderFlagsFromStage( FGStageType type ) -> ShaderType {
        switch (type) {
            case FGStageType::eVertex: return ShaderType::eVertex;
            case FGStageType::eFragment: return ShaderType::ePixel;
            case FGStageType::eCompute: return ShaderType::eCompute;
            default:;
        }

        return ShaderType::eInvalid;
    }

    auto FGSamplerDescription::SetMipLevels( float mipLevels ) -> FGSamplerDescription & {
        mMipLevels = mipLevels;
        return *this;
    }

    auto FGSamplerDescription::SetBorderColor( const Color &color ) -> FGSamplerDescription & {
        mBorderColor = color;
        return *this;
    }

    auto FGSamplerDescription::SetName( eastl::string_view name ) -> FGSamplerDescription& {
        mName = name;
        return *this;
    }

    auto FGSamplerDescription::SetFilter( SamplerFilter filter ) -> FGSamplerDescription & {
        mMinFilter = filter;
        mMagFilter = filter;
        return *this;
    }

    auto FGSamplerDescription::SetMinFilter( SamplerFilter filter ) -> FGSamplerDescription & {
        mMinFilter = filter;
        return *this;
    }

    auto FGSamplerDescription::SetMagFilter( SamplerFilter filter ) -> FGSamplerDescription & {
        mMagFilter = filter;
        return *this;
    }

    auto FGSamplerDescription::SetWrap( SamplerWrapMode wrap ) -> FGSamplerDescription & {
        mWrapU = wrap;
        mWrapV = wrap;
        mWrapW = wrap;
        return *this;
    }

    auto FGSamplerDescription::SetWrapU( SamplerWrapMode wrap ) -> FGSamplerDescription & {
        mWrapU = wrap;
        return *this;
    }

    auto FGSamplerDescription::SetWrapV( SamplerWrapMode wrap ) -> FGSamplerDescription & {
        mWrapV = wrap;
        return *this;
    }

    auto FGSamplerDescription::SetWrapW( SamplerWrapMode wrap ) -> FGSamplerDescription & {
        mWrapW = wrap;
        return *this;
    }

    auto FGPipelineDescription::SetCullMode( CullMode mode ) -> FGPipelineDescription& {
        mCullMode = mode;
        return *this;
    }

    auto FGPipelineDescription::SetPolygonMode(PolygonMode mode) -> FGPipelineDescription& {
        mPolygonMode = mode;
        return *this;
    }

    auto FGPipelineDescription::SetDepthTest( bool value ) -> FGPipelineDescription& {
        mEnableDepthTest = value;
        return *this;
    }

    auto FGPipelineDescription::SetDepthWrite( bool value ) -> FGPipelineDescription& {
        mEnableDepthWrite = value;
        return *this;
    }

    auto FGPipelineDescription::SetDepthFormat( Format format ) -> FGPipelineDescription & {
        mDepthFormat = format;
        return *this;
    }

    auto FGPipelineDescription::SetBlendEnable( bool value ) -> FGPipelineDescription& {
        mEnableBlend = value;
        return *this;
    }

    auto FGPipelineDescription::AddColorFormat( Format format ) -> FGPipelineDescription & {
        mColorFormats.push_back( format );
        return *this;
    }

    auto FGPipelineDescription::SetName( eastl::string_view name ) -> FGPipelineDescription & {
        mName = name;
        return *this;
    }

    auto FGPipelineDescription::SetPipelineType( PipelineType type ) -> FGPipelineDescription & {
        mPipelineType = type;
        return *this;
    }

    auto FGPipelineDescription::PushShader( const Path &path, FGStageType stage ) -> FGPipelineDescription & {
        mShaders[stage] = path;
        return *this;
    }

    auto FGPipelineDescription::SetTopology( PrimitiveTopology topology ) -> FGPipelineDescription & {
        mTopology = topology;
        return *this;
    }

    auto FGBufferDescription::SetName( eastl::string_view name ) -> FGBufferDescription & {
        mName = name;
        return *this;
    }

    auto FGBufferDescription::SetInitialData( BufferSpanHandle data ) -> FGBufferDescription& {
        mSpanHandle = data;
        mElementSizeBytes = data->GetSize();

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetUsage( BufferUsageFlagsBits::kCopyDst );

        return *this;
    }

    auto FGBufferDescription::SetUsage( BufferUsageFlags flags ) -> FGBufferDescription & {
        mBufferUsageFlags = flags;
        return *this;
    }

    auto FGBufferDescription::SetSizeBytes( size_t byteSize ) -> FGBufferDescription & {
        mElementSizeBytes = byteSize;
        return *this;
    }

    auto FGBufferDescription::SetElementsSize( u32 elementCount, size_t elementSizeBytes ) -> FGBufferDescription & {
        mElementCount = elementCount;
        mElementSizeBytes = elementSizeBytes;
        return *this;
    }

    auto FGBufferDescription::SetHeapType( HeapType heap ) -> FGBufferDescription & {
        mHeapType = heap;
        return *this;
    }

    auto FGTextureDescription::SetName( eastl::string_view name ) -> FGTextureDescription & {
        mName = name;
        return *this;
    }

    auto FGTextureDescription::SetWidth(u32 width) -> FGTextureDescription& {
        mWidth = width;
        return *this;
    }

    auto FGTextureDescription::SetHeight(u32 height) -> FGTextureDescription& {
        mHeight = height;
        return *this;
    }

    auto FGTextureDescription::SetMipCount(u32 count) -> FGTextureDescription& {
        mMipCount = count;
        return *this;
    }

    auto FGTextureDescription::SetHeapType(HeapType heapType) -> FGTextureDescription& {
        mHeapType = heapType;
        return *this;
    }

    auto FGTextureDescription::SetMultisampling(Multisampling sampleCount) -> FGTextureDescription& {
        mMSAA = sampleCount;
        return *this;
    }

    auto FGTextureDescription::SetDimensions(TextureDimension dimensions) -> FGTextureDescription& {
        mDimension = dimensions;
        return *this;
    }

    auto FGTextureDescription::SetFormat(Format format) -> FGTextureDescription& {
        mFormat = format;
        return *this;
    }

    auto FGTextureDescription::SetUsage(TextureUsageFlags usage) -> FGTextureDescription& {
        mUsage = usage;
        return *this;
    }

    FGResourceManager::FGResourceManager( GpuDevice* device )
        : mDevice{ device }
    {
        // TODO: Should I allow every pass to have the possibility to push one constant buffer?
        // This data is faster to access in shaders that storage buffers
        // The bindless buffers bindings are supposed to be there for larger data
        auto layoutDescCbuffer{ BindingLayoutDescription{}
            .SetRegisterSpace( 1 )
            .SetShaderVisibility(ShaderFlagsBits::kAll)
            .AddItem(BindingLayoutItem::ConstantBuffer(0)) };
        //mBindingLayoutHandle = mDevice->CreateBindingLayout(layoutDescCbuffer);

        // Prepare layouts
        auto layoutDesc{ BindlessLayoutDescription{}
            .SetVisibility(ShaderFlagsBits::kAll)
            .SetRegisterSpace(MKT_DEFAULT_REGISTER_SPACE)
            .AddBindlessItem(BindlessLayoutItem::Samplers(MKT_SAMPLER_BINDING, 4096))
            .AddBindlessItem(BindlessLayoutItem::Texture_SRV(MKT_TEXTURE_SRV_BINDING, 4096))
            .AddBindlessItem(BindlessLayoutItem::Texture_UAV(MKT_TEXTURE_UAV_BINDING, 4096))
            .AddBindlessItem(BindlessLayoutItem::StructuredBuffer_SRV(MKT_STRUCTURED_SRV_BINDING, 4096))
            .AddBindlessItem(BindlessLayoutItem::StructuredBuffer_UAV(MKT_STRUCTURED_UAV_BINDING, 4096)) };
        //.AddBindlessItem(BindlessLayoutItem::AccelerationStructures(MKT_ACCELERATION_STRUCTURE_BINDING, 4096)) };

        mBindlessLayout = mDevice->CreateBindlessLayout( layoutDesc );
        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );

        mPipelineLayout = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindlessLayout ));
    }

    auto FGResourceManager::Get( FGResourceHandle handle ) -> FGResource & {
        MKT_ASSERT( mResources.contains( handle ), "Resource with ID does not exist" );
        return *mResources.at( handle );
    }

    auto FGResourceManager::Get( FGResourceHandle handle ) const -> const FGResource & {
        MKT_ASSERT( mResources.contains( handle ), "Resource with ID does not exist" );
        return *mResources.at( handle );
    }

    auto FGResourceManager::GetBindlessLayout() const -> BindingLayoutHandle {
        return mBindlessLayout;
    }

    auto FGResourceManager::GetPipelineLayout() const -> PipelineLayoutHandle {
        return mPipelineLayout;
    }

    auto FGResourceManager::GetDescriptorTable() const -> DescriptorTableHandle  {
        return mDescriptorTable;
    }

    auto FGResourceManager::Allocate( FGResourceType type, IResource* resource ) -> FGResource& {
        std::lock_guard lock{ mResourceMutex };

        const auto it{ mImportedResources.find( resource ) };
        if (it != mImportedResources.end()) {
            return *mResources[it->second];
        }

        auto id{ ++mResourceCount };
        if (resource) {
            mImportedResources[resource] = id;
        }

        auto& result{ mResources[id] = eastl::make_unique<FGResource>(
            FGResource{
                .mHandle = id,
                .mType = type,
            }
        ) };
        return *result;
    }

    auto FGResourceManager::Free( FGResourceHandle handle ) -> bool {
        std::lock_guard lock{ mResourceMutex };
        return false;
    }

    auto FGResourceManager::PushTexture_SRV( FGResourceHandle handle  ) -> u32 {
        std::lock_guard lock{ mTableWriteMutex };

        // TextureCube and Texture2D are same binding because they are same type of descriptor
        auto& table{ mResourceTable[ MKT_TEXTURE_SRV_BINDING ] };
        if (table.contains( handle )) {
            return table.at( handle );
        }

        u32 newID{ table[handle] = table.size() };
        auto& resource{ Get(handle) };
        ITexture* texture{ checked_cast<ITexture*>( resource.mResource.GetRaw() ) };
        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Texture_SRV( newID, texture, texture->GetFormat(), AllSubResources, texture->GetDimension() ) );

        return newID;
    }

    auto FGResourceManager::PushTexture_UAV( FGResourceHandle handle ) -> u32 {
        std::lock_guard lock{ mTableWriteMutex };

        // TextureCube and Texture2D are same binding because they are same type of descriptor
        auto& table{ mResourceTable[ MKT_TEXTURE_UAV_BINDING ] };
        if (table.contains( handle )) {
            return table.at( handle );
        }

        u32 newID{ table[handle] = table.size() };
        auto& resource{ Get(handle) };
        ITexture* texture{ checked_cast<ITexture*>( resource.mResource.GetRaw() ) };
        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Texture_SRV( newID, texture, texture->GetFormat(), AllSubResources, texture->GetDimension() ) );

        return newID;
    }

    auto FGResourceManager::PushSampler( FGResourceHandle handle ) -> u32 {
        std::lock_guard lock{ mTableWriteMutex };
        auto& table{ mResourceTable[MKT_SAMPLER_BINDING] };
        if (table.contains( handle )) {
            return table.at( handle );
        }

        u32 newID{ table[handle] = table.size() };
        auto& resource{ Get(handle) };
        ISampler* sampler{ checked_cast<ISampler*>( resource.mResource.GetRaw() ) };
        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Sampler( newID, sampler ) );

        return newID;
    }

    auto FGResourceManager::PushBuffer_SRV( FGResourceHandle handle ) -> u32 {
        std::lock_guard lock{ mTableWriteMutex };
        auto& table{ mResourceTable[MKT_STRUCTURED_SRV_BINDING] };
        if (table.contains( handle )) {
            return table.at( handle );
        }

        u32 newID{ table[handle] = table.size() };
        auto& resource{ Get(handle) };
        IBuffer* buffer{ checked_cast<IBuffer*>( resource.mResource.GetRaw() ) };
        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::StructuredBuffer_SRV( newID, buffer ) );

        return newID;
    }

    auto FGResourceManager::PushBuffer_UAV( FGResourceHandle handle ) -> u32 {
        std::lock_guard lock{ mTableWriteMutex };
        auto& table{ mResourceTable[MKT_STRUCTURED_UAV_BINDING] };
        if (table.contains( handle )) {
            return table.at( handle );
        }

        u32 newID{ table[handle] = table.size() };
        auto& resource{ Get(handle) };
        IBuffer* buffer{ checked_cast<IBuffer*>( resource.mResource.GetRaw() ) };
        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::StructuredBuffer_UAV( newID, buffer ) );

        return newID;
    }

    auto FGResourceManager::PushBuffer_Constant( FGResourceHandle ) -> u32 {
        std::lock_guard lock{ mTableWriteMutex };
        // TODO:
        return 0;
    }

    auto FGResourceManager::ImportTexture( TextureHandle handle ) -> FGTextureHandle {
        auto& resource{ Allocate( FGResourceType::eTexture, handle.GetRaw() ) };

        resource.mResource = handle;

        return FGTextureHandle{ .mHandle = resource.mHandle };
    }

    auto FGNodeControl::Clear() -> void {
        mContexts.clear();
        mResources.clear();
        mNodes.clear();
    }

    FGNodeBuilder::FGNodeBuilder( FGNode& node, FGNodeControl& control )
        : mGraphNode{ MKT_ADDRESSOF( node ) }, mNodeControl{ MKT_ADDRESSOF( control ) }
    {}

    auto FGNodeBuilder::Read( FGTextureHandle handle, FGResourceState state ) -> void {
        Read( handle.mHandle );

        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
            .mAccess = FGResourceAccess::eRead
        };
    }

    auto FGNodeBuilder::Write( FGTextureHandle handle, FGResourceState state ) -> void {
        Write( handle.mHandle );

        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
            .mAccess = FGResourceAccess::eWrite
        };
    }

    auto FGNodeBuilder::ReadWrite( FGTextureHandle handle, FGResourceState state ) -> void {
        ReadWrite( handle.mHandle );

        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
            .mAccess = FGResourceAccess::eReadWrite
        };
    }

    auto FGNodeBuilder::ReadWrite( FGBufferHandle handle, FGResourceState state ) -> void {
        ReadWrite( handle.mHandle );

        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
            .mAccess = FGResourceAccess::eReadWrite
        };
    }

    auto FGNodeBuilder::Read( FGBufferHandle handle, FGResourceState state ) -> void {
        Read( handle.mHandle );

        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
            .mAccess = FGResourceAccess::eRead
        };
    }

    auto FGNodeBuilder::Write( FGBufferHandle handle, FGResourceState state ) -> void {
        Write( handle.mHandle );

        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
            .mAccess = FGResourceAccess::eWrite
        };
    }

    auto FGNodeBuilder::Read( FGResourceHandle handle ) -> void {
        MKT_ASSERT( handle != 0, "Invalid resource handle" );
        auto& ver{ mNodeControl->mResources[handle].mVersions.back() };  // current version
        if(ver.HasWriter()) {
            mGraphNode->mDependsOn.emplace(ver.mWriterPass);     // RAW edge
        }

        ver.mReaderPasses.push_back(mGraphNode->mName);          // track who reads this version
        mGraphNode->mReadResources.push_back(handle);    // record for barrier insertion
    }

    auto FGNodeBuilder::Write( FGResourceHandle handle) -> void {
        MKT_ASSERT( handle != 0, "Invalid resource handle" );
        auto& ver{ mNodeControl->mResources[handle].mVersions.back() };  // current version (pre-bump)
        if(ver.HasWriter()) {
            mGraphNode->mDependsOn.emplace(ver.mWriterPass);  // WAW edge: prev writer must finish
        }

        for(const auto& reader : ver.mReaderPasses) {
            mGraphNode->mDependsOn.emplace(reader);  // WAR edge: reader must finish first
        }

        if(ver.HasWriter()) {
            mNodeControl->mResources[handle].mVersions.push_back({}); // bump version if this one already has a writer
            mNodeControl->mResources[handle].mVersions.back().mWriterPass = mGraphNode->mName;  // this pass owns the new version
        } else {
            ver.mWriterPass = mGraphNode->mName;
        }

        mGraphNode->mWriteResources.push_back(handle);  // record for barrier insertion
    }

    auto FGNodeBuilder::ReadWrite( FGResourceHandle handle ) -> void {
        MKT_ASSERT( handle != 0, "Invalid resource handle" );
        auto& ver{ mNodeControl->mResources[handle].mVersions.back() };
        if(ver.HasWriter()) {
            mGraphNode->mDependsOn.emplace(ver.mWriterPass);  // RAW edge
        }
        for(const auto& reader : ver.mReaderPasses) {
            mGraphNode->mDependsOn.emplace(reader);  // WAR edge
        }

        mNodeControl->mResources[handle].mVersions.push_back({});                   // bump version (it's a write)
        mNodeControl->mResources[handle].mVersions.back().mWriterPass = mGraphNode->mName;

        mGraphNode->mReadResources.push_back(handle);       // appears in both lists (for barriers + lifetimes)
        mGraphNode->mWriteResources.push_back(handle);
        mGraphNode->mReadWriteResources.push_back(handle);  // marks this handle as UAV for StateForUsage
    }

    FrameGraph::FrameGraph( GpuDevice *device, ShaderLibrary* library )
        : mDevice{ device }, mShaderLibrary{ library } {
        mNodeControl = eastl::make_unique<FGNodeControl>();
        mResourceManager = eastl::make_unique<FGResourceManager>( mDevice );

        mGraphicsCommands = mDevice->CreateCommandList( { threading::GetThreadConcurrency(), QueueType::eGraphics } );
        mComputeCommands = mDevice->CreateCommandList( { threading::GetThreadConcurrency(), QueueType::eCompute } );
        mTransferCommands = mDevice->CreateCommandList( { threading::GetThreadConcurrency(), QueueType::eTransfer } );

        mGraphicsCommands->SetEnableAutomaticBarriers( false );
        mGraphicsCommands->SetDebugName( "FG GraphicsCommands" );

        mComputeCommands->SetEnableAutomaticBarriers( false );
        mComputeCommands->SetDebugName( "FG ComputeCommands" );

        mTransferCommands->SetEnableAutomaticBarriers( false );
        mTransferCommands->SetDebugName( "FG TransferCommands" );
    }

    auto FrameGraph::Compile() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mExecutionPlan.mBarriers.clear();
        mExecutionPlan.mExecutionGraph.clear();
        mExecutionPlan.mSorted.clear();
        mExecutionPlan.mTaskMap.clear();

        // Build edges
        BuildNodeEdges();

        // Cull nodes
        CullGraphNodes();

        // Barriers
        // I do not think I wil go this way I think the passes will specify the
        // state they need the resource to be in and access type will be inferred
        BuildNodeBarriers();

        // Create the contexts and executions tasks
        BuildExecutionTasks();

#if !defined(NDEBUG)
        std::ostringstream oss{};
        mExecutionPlan.mExecutionGraph.dump(oss);
        mExecutionPlan.mExecutionGraph.name("FrameGraph TaskExecution");

        // to file example
        // std::ofstream file("taskflow.dot");
        // mExecutionPlan.mExecutionGraph.dump(file);
        // dot -Tpng taskflow.dot -o taskflow.png

        MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_CYAN, "FG Dependencies:\n{}", oss.str() );
#endif

    }

    auto FrameGraph::Execute() -> void {
        //MKT_BEGIN_PROFILER_NAMED();

        mGraphicsCommands->Begin( { .mScopeName = "FG Graphics Record" } );
        mComputeCommands->Begin( { .mScopeName = "FG Compute Record" } );
        mTransferCommands->Begin( { .mScopeName = "FG Transfer Record" } );

        // Parallel
        if (false) {
            threading::TaskService::Get()->Submit( mExecutionPlan.mExecutionGraph, true );
        } else {
            // Sequential execution
            for ( auto& passName: mExecutionPlan.mSorted ) {
                if ( !mNodeControl->mNodes[passName].mIsAlive )
                    continue;

                auto& pass = mNodeControl->mNodes[passName];
                auto& ctx = mNodeControl->mContexts[passName];

                CommandListHandle cmd{};

                switch (pass.mType) {
                    case FGPassType::eGraphics:
                        cmd = mGraphicsCommands;
                        break;
                    case FGPassType::eCompute:
                        cmd = mComputeCommands;
                        break;
                    case FGPassType::eTransfer:
                        cmd = mTransferCommands;
                        break;
                    default:;
                }

                // Off load this work to workers threads
                // Vulkan could use secondary command buffers here
                ctx->BeginPass( cmd );

                // Place pass barriers
                auto& barriers = mExecutionPlan.mBarriers[passName];
                ctx->CommitBarriers( barriers );

                pass.mExecuteCallback( *ctx, mBlackboard );
                ctx->EndPass();
            }
        }

        mGraphicsCommands->End();
        mComputeCommands->End();
        mTransferCommands->End();

        mDevice->SubmitCommands( mGraphicsCommands );
        mDevice->SubmitCommands( mComputeCommands );
        mDevice->SubmitCommands( mTransferCommands );
    }

    auto FrameGraph::SetExecutionPolicy( eastl::string_view passName, FGExecutionPolicy policy ) -> void {
        MKT_ASSERT( mNodeControl->mNodes.contains( passName.data() ), string::Format( "Pass '{}' does not exist", passName.data() ) );
        mNodeControl->mNodes[passName.data()].mExecutionPolicy = policy;
    }

    auto FrameGraph::DisablePass( eastl::string_view passName ) -> void {
        MKT_ASSERT( mNodeControl->mNodes.contains( passName.data() ), string::Format( "Pass '{}' does not exist", passName.data() ) );
        mNodeControl->mNodes[passName.data()].mIsAlive = false;

        CullGraphNodes();
    }

    auto FrameGraph::EnablePass( eastl::string_view passName ) -> void {
        MKT_ASSERT( mNodeControl->mNodes.contains( passName.data() ), string::Format( "Pass '{}' does not exist", passName.data() ) );
        mNodeControl->mNodes[passName.data()].mIsAlive = true;

        CullGraphNodes();
    }

    auto FrameGraph::GetNodeControl() const -> const FGNodeControl& {
        return *mNodeControl;
    }

    auto FrameGraph::RecordCommands( CommandListHandle cmd ) -> void {
        cmd->Begin( {} );

        // TODO: Record all commands for specific queue type

        cmd->End();
        mDevice->SubmitCommands( cmd );
    }

    auto FrameGraph::GetTexture( FGTextureHandle handle ) const -> TextureHandle {
        return mResourceManager->Get( handle.mHandle ).mResource;
    }

    auto FrameGraph::GetBuffer( FGBufferHandle handle ) const -> BufferHandle {
        return mResourceManager->Get( handle.mHandle ).mResource;
    }

    auto FrameGraph::Create( GpuDevice* device, ShaderLibrary* shaderLibrary ) -> eastl::unique_ptr<FrameGraph> {
        MKT_BEGIN_PROFILER_NAMED();
        return eastl::make_unique<FrameGraph>( device, shaderLibrary );
    }

    auto FrameGraph::BuildNodeEdges() -> void {
        for (auto& pass : mNodeControl->mNodes | std::views::values ) {
            ankerl::unordered_dense::set<eastl::string> seenPasses{};

            for (auto& dep : pass.mDependsOn) {
                if (seenPasses.insert( dep ).second ) {  // first time seeing this edge?
                    mNodeControl->mNodes[dep].mSuccessors.push_back( pass.mName );
                    mNodeControl->mNodes[pass.mName].mInDegree += 1;
                }
            }
        }

        // TopoSort Kahn's Topological Sort
        eastl::queue<eastl::string> q{};
        ankerl::unordered_dense::map<eastl::string, u32> inDegrees{};
        auto& passesMap{ mNodeControl->mNodes };

        for ( const auto& pass : passesMap | std::views::values ) {
            inDegrees[pass.mName] = passesMap[pass.mName].mInDegree;

            if ( inDegrees[pass.mName] == 0 ) {
                q.push( pass.mName );// no dependencies -> ready immediately
            }
        }

        while ( !q.empty() ) {
            eastl::string cur{ q.front() };
            q.pop();

            mExecutionPlan.mSorted.push_back( cur );
            for ( const auto& succ: passesMap[cur].mSuccessors ) {
                if ( --inDegrees[succ] == 0 ) {// all successors dependencies done?
                    q.push( succ );      // succ is now ready
                }
            }
        }

        // If we didn't visit every pass, the graph has a cycle, invalid.
        MKT_ASSERT( mExecutionPlan.mSorted.size() == passesMap.size(), "Cycle detected!" );
    }

    auto FrameGraph::CullGraphNodes() -> void {
        auto& passesMap{ mNodeControl->mNodes };

        // Cull Passes
        // This step is done if we for instance remove or disable a pass
        // we need to disable the ones depending on it
        if (!mExecutionPlan.mSorted.empty()) {
            for(auto reverseIt{ mExecutionPlan.mSorted.rbegin() }; reverseIt != mExecutionPlan.mSorted.rend(); ++reverseIt) {
                if (!passesMap[*reverseIt].mIsAlive) {
                    continue; // skip dead passes
                }

                // Commenting this works for now because all frame graph resources
                // are persistent across frames so I can disable a node and not
                // have to worry about resource lifetime if needed somewhere else
                // for (const auto& pass: passesMap[*reverseIt].mDependsOn) {
                //     passesMap[pass].mIsAlive = true; // my dependency is needed -> keep it alive
                // }
            }
        }
    }

    auto FrameGraph::BuildNodeBarriers() -> void {
        for ( const auto& passName : mExecutionPlan.mSorted ) {
            const auto& pass{ mNodeControl->mNodes[passName] };
            if ( !pass.mIsAlive ) {
                continue;
            }

            auto recordTransition = [&]( FGResourceHandle h ) -> void {
                // You cannot set a barrier twice for the same resource
                // in the same pass
                if (mExecutionPlan.mBarriers[passName].contains( h )) {
                    return;
                }

                // Improve this, this is better, put barriers only if needed
                // if a resource is always read from no need to sync access
                // insert barrier only for layout transitions or
                // if we write to it
                auto prev = mNodeControl->mResources[h].mCurrentState;
                auto next = mNodeControl->mNodes[passName].mResourceStates[h].mState;

                auto isWrite = [](FGResourceState s) {
                    return s == FGResourceState::eUnorderedAccess ||
                           s == FGResourceState::eRenderTarget ||
                           s == FGResourceState::eDepthWrite ||
                           s == FGResourceState::eCopyDest ||
                           s == FGResourceState::eResolveDest;
                };

                if (prev == FGResourceState::eUnknown) {
                    // First use -> just set state, no barrier
                    // I think I wil probably remove this and transition all resources to general layout so the
                    // i only do the next check for barriers for each pass
                    mNodeControl->mResources[h].mCurrentState = next;
                    mExecutionPlan.mBarriers[passName][h] = FGBarrier{
                        h,
                        prev,
                        next };
                }
                else if (prev != next || isWrite(prev) || isWrite(next)) {
                    mExecutionPlan.mBarriers[passName][h] = FGBarrier{
                        h,
                        prev,
                        next };

                    mNodeControl->mResources[h].mCurrentState = next;
                }
            };

            for ( auto& h: pass.mReadResources ) {
                recordTransition( h );
            }

            for ( auto& h: pass.mWriteResources ) {
                recordTransition( h );
            }
        }
    }

    auto FrameGraph::BuildExecutionTasks() -> void {
        for (auto& [passName, node] : mNodeControl->mNodes ) {
            mNodeControl->mContexts[passName] = Ref<CommandContext>::Spawn( MKT_ADDRESSOF( node ), mResourceManager.get() );
        }

        // create tasks
        for (auto& passName : mExecutionPlan.mSorted) {
            if (!mNodeControl->mNodes[passName].mIsAlive) {
                continue;
            }

            auto& pass{ mNodeControl->mNodes[passName] };

            CommandListHandle cmd{};
            switch (pass.mType) {
                case FGPassType::eGraphics:
                    cmd = mGraphicsCommands;
                    break;
                case FGPassType::eCompute:
                    cmd = mComputeCommands;
                    break;
                case FGPassType::eTransfer:
                    cmd = mTransferCommands;
                    break;
                default:;
            }

            auto task = mExecutionPlan.mExecutionGraph.emplace([this, passName, cmd]() mutable {
                auto& pass{ mNodeControl->mNodes[passName] };
                auto& ctx{ mNodeControl->mContexts[passName] };

                if ( pass.mType != FGPassType::eGeneric ) {
                    cmd->BeginParallel();
                }

                // Off load this work to workers threads
                // Vulkan could use secondary command buffers here
                ctx->BeginPass( cmd );

                // Place pass barriers
                const auto it{ mExecutionPlan.mBarriers.find( passName ) };
                if (it != mExecutionPlan.mBarriers.end()) {
                    ctx->CommitBarriers( it->second );
                }

                pass.mExecuteCallback( *ctx, mBlackboard );
                ctx->EndPass();

                if ( pass.mType != FGPassType::eGeneric ) {
                    cmd->EndParallel();
                }
            });

            task.name( passName.c_str() );

            mExecutionPlan.mTaskMap[MKT_ADDRESSOF( mNodeControl->mNodes[passName] )] = task;
        }

        // connect dependencies
        for (auto& passName : mExecutionPlan.mSorted) {
            auto& pass = mNodeControl->mNodes[passName];

            if (!pass.mIsAlive)
                continue;

            auto& task = mExecutionPlan.mTaskMap[&pass];

            for (auto& succName : pass.mSuccessors) {
                auto& succ = mNodeControl->mNodes[succName];

                if (!succ.mIsAlive)
                    continue;

                mExecutionPlan.mTaskMap[&succ].succeed(task);
            }
        }
    }

    auto FrameGraph::Create( const FGPipelineDescription &desc ) -> FGPipelineHandle {
        FGResource& resource{ mResourceManager->Allocate( FGResourceType::ePipeline ) };

        if ( desc.mPipelineType == PipelineType::eGraphics ) {
            MKT_ASSERT( !desc.mShaders.empty(), "Creating graphics pipeline without shaders." );
            auto graphicsPipelineDesc{ GraphicsPipelineDescription{}
                .SetPipelineLayout( mResourceManager->GetPipelineLayout() )
                .SetTopology( desc.mTopology )
                .SetCullMode( desc.mCullMode )
                .SetPolygonMode( desc.mPolygonMode )
                .SetBlendEnable( desc.mEnableBlend )
                .SetDepthTest( desc.mEnableDepthTest )
                .SetDepthWrite( desc.mEnableDepthWrite )
                .SetDepthFormat( desc.mDepthFormat )
            };

            // Color formats
            for ( const auto &format: desc.mColorFormats ) {
                graphicsPipelineDesc.AddColorFormat( format );
            }

            // Shaders
            for ( const auto &shader: desc.mShaders ) {
                graphicsPipelineDesc.AddShader( mShaderLibrary->LoadShader(shader.second, GetShaderFlagsFromStage( shader.first )) );
            }

            mNodeControl->mResources[resource.mHandle] = FGNodeResource {
                .mName{ desc.mName },
                .mDescription = desc,
                .mVersions{ ResourceVersion{
                    .mWriterPass{}, // No writers
                    .mReaderPasses{}, // No readers
                }},
                .mIsImported = false
            };

            resource.mResource = mDevice->CreatePipeline( graphicsPipelineDesc );
            checked_cast<DeviceObject*>( resource.mResource.GetRaw() )->SetDebugName( desc.mName );
        } else if ( desc.mPipelineType == PipelineType::eCompute ) {
            MKT_ASSERT( desc.mShaders.contains( FGStageType::eCompute ), "Creating compute pipeline without compute shader." );
            auto computePipelineDesc{ ComputePipelineDescription{}
                .SetPipelineLayout(  mResourceManager->GetPipelineLayout() )
                .SetComputeStage( mShaderLibrary->LoadShader( desc.mShaders.at( FGStageType::eCompute ), ShaderType::eCompute ) )
            };

            mNodeControl->mResources[resource.mHandle] = FGNodeResource {
                .mName{ desc.mName },
                .mDescription = desc,
                .mVersions{ ResourceVersion{
                    .mWriterPass{}, // No writers
                    .mReaderPasses{}, // No readers
                }},
                .mIsImported = false
            };

            resource.mResource = mDevice->CreatePipeline( computePipelineDesc );
            checked_cast<DeviceObject*>( resource.mResource.GetRaw() )->SetDebugName( desc.mName );
        }

        return FGPipelineHandle{ resource.mHandle };
    }

    auto FrameGraph::Create( const FGBufferDescription &desc ) -> FGBufferHandle {
        auto& resource{ mResourceManager->Allocate( FGResourceType::eBuffer ) };
        auto bufferDesc{ BufferCreateDescription{}
            .SetBufferUsage( desc.mBufferUsageFlags )
            .SetName( desc.mName )
            .ForElement( desc.mElementSizeBytes, desc.mElementCount )
            .SetCpuAccessType( desc.mHeapType == HeapType::eUpload ? CpuAccessType::eWrite : CpuAccessType::eRead )
            .SetByteSize( desc.mElementSizeBytes )
        };

        if (!desc.mSpanHandle.IsEmpty()) {
            bufferDesc.SetInitialData( desc.mSpanHandle );
        }

        mNodeControl->mResources[resource.mHandle] = FGNodeResource {
            .mName{ desc.mName },
            .mDescription = bufferDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{}, // No writers
                .mReaderPasses{}, // No readers
            }},
            .mIsImported = false
        };

        resource.mResource = mDevice->CreateBuffer( bufferDesc );
        checked_cast<DeviceObject*>( resource.mResource.GetRaw() )->SetDebugName( desc.mName );

        return FGBufferHandle{ .mHandle = resource.mHandle };
    }

    auto FrameGraph::ImportTexture( const Path &path ) -> FGTextureHandle {
        asset::ImageHandle image{ asset::ProcessImage2D( path ) };
        auto textureDesc{ TextureCreateDescription{}
            .SetImageData( image )
            .SetWidth( as<i32>( image->mWidth ) )
            .SetHeight( as<i32>( image->mHeight ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };

        // Use the asset service when it is re
        TextureHandle texture{ mDevice->CreateTexture( textureDesc ) };
        texture->SetDebugName( string::Format( "FG Loaded Texture {}", path.GetC_Str() ) );

        auto& resource{ mResourceManager->Allocate( FGResourceType::eTexture, texture.GetRaw() ) };
        resource.mResource = texture;

        mNodeControl->mResources[resource.mHandle] = FGNodeResource {
            .mName{ string::Format( "FG Loaded Texture {}", path.GetC_Str() ) },
            .mDescription = textureDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{}, // No writers
                .mReaderPasses{}, // No readers
            }},
            .mIsImported = true
        };

        return FGTextureHandle{ .mHandle = resource.mHandle };
    }

    auto FrameGraph::ImportTexture( TextureHandle handle ) -> FGTextureHandle {
        if (handle.IsEmpty()) {
            return {};
        }

        auto& resource{ mResourceManager->Allocate( FGResourceType::eTexture, handle.GetRaw() ) };
        resource.mResource = handle;

        mNodeControl->mResources[resource.mHandle] = FGNodeResource {
            .mName{ string::Format( "FG External Texture {}", handle->GetDebugName() ) },
            .mVersions{ ResourceVersion{
                .mWriterPass{}, // No writers
                .mReaderPasses{}, // No readers
            }},
            .mIsImported = true
        };

        return FGTextureHandle{ .mHandle = resource.mHandle };
    }

    auto FrameGraph::ImportBuffer( BufferHandle handle ) -> FGBufferHandle {
        return {};
    }

    auto FrameGraph::Create( const FGTextureDescription &desc ) -> FGTextureHandle {
        auto& resource{ mResourceManager->Allocate( FGResourceType::eTexture ) };
        auto textureDesc{ TextureCreateDescription{}
            .SetName( desc.mName )
            .SetWidth( desc.mWidth )
            .SetHeight( desc.mHeight )
            .SetMipCount( desc.mMipCount )
            .SetDimensions( desc.mDimension )
            .SetMultisampling( desc.mMSAA )
            .SetUsage( desc.mUsage )
            .SetFormat( desc.mFormat ) };

        mNodeControl->mResources[resource.mHandle] = FGNodeResource {
            .mName{ desc.mName },
            .mDescription = textureDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{}, // No writers
                .mReaderPasses{}, // No readers
            }},
            .mIsImported = false
        };

        // This part should not happen here yet
        resource.mResource = mDevice->CreateTexture( textureDesc );
        checked_cast<DeviceObject*>( resource.mResource.GetRaw() )->SetDebugName( desc.mName );
        return FGTextureHandle{ .mHandle = resource.mHandle };
    }

    auto FrameGraph::Create( const FGSamplerDescription &desc ) -> FGSamplerHandle {
        auto& resource{ mResourceManager->Allocate( FGResourceType::eSampler ) };
        auto samplerDesc{ SamplerCreateDescription{}
            .SetBorderColor( desc.mBorderColor )
            .SetMagFilter( desc.mMagFilter )
            .SetMinFilter( desc.mMinFilter )
            .SetMipLevels( desc.mMipLevels )
            .SetWrapU( desc.mWrapU )
            .SetWrapV( desc.mWrapV )
            .SetWrapW( desc.mWrapW ) };

        mNodeControl->mResources[resource.mHandle] = FGNodeResource {
            .mName{ desc.mName },
            .mDescription = samplerDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{}, // No writers
                .mReaderPasses{}, // No readers
            }},
            .mIsImported = false
        };

        resource.mResource = mDevice->CreateSampler( samplerDesc );
        checked_cast<DeviceObject*>( resource.mResource.GetRaw() )->SetDebugName( desc.mName );

        return FGSamplerHandle{ .mHandle = resource.mHandle };
    }

    auto FrameGraph::SetEnablePass( bool enable, eastl::string_view name ) -> void {

    }
}// namespace Mikoto