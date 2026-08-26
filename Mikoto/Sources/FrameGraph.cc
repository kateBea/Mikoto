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
#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::material;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD constexpr auto GetShaderFlagsFromStage( FGStageType type ) -> ShaderType {
        switch (type) {
            case FGStageType::eVertex: return ShaderType::eVertex;
            case FGStageType::ePixel: return ShaderType::ePixel;
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
        mInitialContents = data;
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
        mMultisampling = sampleCount;
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

    FGResourceManager::FGResourceManager( IGpuDevice* device )
        : mDevice{ device }
    {
        // Prepare layouts
        const auto layoutDesc{ BindlessLayoutDescription{}
            .SetVisibility( ShaderFlagsBits::kAll )
            .SetRegisterSpace( MKT_DEFAULT_REGISTER_SPACE )
            .AddBindlessItem( BindlessLayoutItem::Samplers( MKT_SAMPLER_BINDING, 4096 ) )
            .AddBindlessItem( BindlessLayoutItem::Texture_SRV( MKT_TEXTURE_SRV_BINDING, 4096 ) )
            .AddBindlessItem( BindlessLayoutItem::Texture_UAV( MKT_TEXTURE_UAV_BINDING, 4096 ) )
            .AddBindlessItem( BindlessLayoutItem::StructuredBuffer_SRV( MKT_STRUCTURED_SRV_BINDING, 4096 ) )
            .AddBindlessItem( BindlessLayoutItem::StructuredBuffer_UAV( MKT_STRUCTURED_UAV_BINDING, 4096 ) )
            .AddBindlessItem( BindlessLayoutItem::StructuredBuffer_SRV( MKT_BUFFER_DEVICE_ADDRESS_BINDING, 4096 ) ) };
            //.AddBindlessItem(BindlessLayoutItem::AccelerationStructures(MKT_ACCELERATION_STRUCTURE_BINDING, 4096)) };
        mBindlessLayout = mDevice->CreateBindlessLayout( layoutDesc );
        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );
        mDescriptorTable->SetDebugName( "FrameGraph Resource Table" );

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

    auto FGResourceManager::GetBufferMappedAddress( FGBufferHandle handle ) const -> const void* {
        auto& resource{ Get(handle.mHandle) };
        BufferHandle buffer{ resource.mResource };
        return mDevice->Map( buffer.GetRaw() ); // Maps whole buffer, not parts of it
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
                .mResourceID = id,
                .mType = type,
            }
        ) };
        return *result;
    }

    auto FGResourceManager::Free( FGResourceHandle handle ) -> bool {
        std::lock_guard lock{ mResourceMutex };
        return false;
    }

    auto FGResourceManager::AllocateTextureIndex_SRV( FGResourceHandle handle  ) -> u32 {
        std::lock_guard lock{ mTableWriteMutex };

        // TextureCube and Texture2D are same binding because they are same type of descriptor
        auto& table{ mResourceTable[ MKT_TEXTURE_SRV_BINDING ] };
        if (table.contains( handle )) {
            return table.at( handle );
        }

        u32 newID{ table[handle] = table.size() };
        auto& resource{ Get(handle) };
        ITexture* texture{ checked_cast<ITexture*>( resource.mResource.GetRaw() ) };
        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Texture_SRV( newID, texture, texture->GetFormat(), kAllSubResources, texture->GetDimension() ) );

        return newID;
    }

    auto FGResourceManager::AllocateTextureIndex_UAV( FGResourceHandle handle ) -> u32 {
        std::lock_guard lock{ mTableWriteMutex };

        // TextureCube and Texture2D are same binding because they are same type of descriptor
        auto& table{ mResourceTable[ MKT_TEXTURE_UAV_BINDING ] };
        if (table.contains( handle )) {
            return table.at( handle );
        }

        u32 newID{ table[handle] = table.size() };
        auto& resource{ Get(handle) };
        ITexture* texture{ checked_cast<ITexture*>( resource.mResource.GetRaw() ) };
        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Texture_UAV( newID, texture, texture->GetFormat(), kAllSubResources, texture->GetDimension() ) );

        return newID;
    }

    auto FGResourceManager::AllocateSamplerIndex( FGResourceHandle handle ) -> u32 {
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

    auto FGResourceManager::AllocateBufferIndex_SRV( FGResourceHandle handle ) -> u32 {
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

    auto FGResourceManager::AllocateBufferIndex_UAV( FGResourceHandle handle ) -> u32 {
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

    auto FGResourceManager::ImportTexture( TextureHandle handle ) -> FGTextureHandle {
        auto& resource{ Allocate( FGResourceType::eTexture, handle.GetRaw() ) };

        resource.mResource = handle;

        return FGTextureHandle{ .mHandle = resource.mResourceID };
    }

    auto FGResourceManager::ImportBuffer( BufferHandle handle ) -> FGBufferHandle {
        auto& resource{ Allocate( FGResourceType::eBuffer, handle.GetRaw() ) };

        resource.mResource = handle;

        return FGBufferHandle{ .mHandle = resource.mResourceID };
    }

    auto FGStatisticsManager::GetNode( eastl::string_view name ) -> FGNodeStatistics* {
        MKT_ASSERT( mGraphNodes.contains( name.data() ), "Node does not exists" );
        return MKT_ADDRESSOF( mGraphNodes.at(name.data()) );
    }

    auto FGStatisticsManager::GetNode( eastl::string_view name ) const -> const FGNodeStatistics* {
        MKT_ASSERT( mGraphNodes.contains( name.data() ), "Node does not exists" );
        return MKT_ADDRESSOF( mGraphNodes.at(name.data()) );
    }

    auto FGStatisticsManager::RegisterNode( eastl::string_view name ) -> FGNodeStatistics* {
        return MKT_ADDRESSOF( mGraphNodes[name.data()] );
    }

    auto FGStatisticsManager::GetStatistics() const -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics>& {
        return mGraphNodes;
    }

    FGReadbackManager::FGReadbackManager( Blackboard& blackboard, FGResourceManager& manager )
        : mBlackboard{ MKT_ADDRESSOF( blackboard ) }, mResourceManager{ MKT_ADDRESSOF( manager ) }
    {}

    auto FGReadbackManager::ExecuteCallbacks( u64 fenceValue ) -> void {
        auto it{ mReadbackTasks.begin() };
        while ( it != mReadbackTasks.end() ) {
            auto& pair{ *it };

            if (fenceValue >= pair.second.mFenceValue) {
                pair.second.mCallback( *mBlackboard, *mResourceManager );
                pair.second.mIsReady = true;

                if (!pair.second.mRunsEveryFrame) {
                    it = mReadbackTasks.erase( it );
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }

    auto FGReadbackManager::UpdateTaskFenceValue( u32 taskID, u64 fenceValue ) -> void {
        MKT_ASSERT( mReadbackTasks.contains( taskID ), "Task with specified ID does not exist" );

        if (mReadbackTasks[taskID].mIsReady) {
            mReadbackTasks[taskID].mIsReady = false;
            mReadbackTasks[taskID].mFenceValue = fenceValue;
        }
    }

    auto FGReadbackManager::RegisterCallback( const FGReadbackTask::Callback& callback, bool runEveryFrame ) -> u32 {
        u32 taskID{ (u32)mReadbackTasks.size() + 1 };
        mReadbackTasks[taskID] = FGReadbackTask{
            .mCallback = callback,
            .mRunsEveryFrame = runEveryFrame,
        };

        return taskID;
    }

    FGNodeControl::FGNodeControl()
        : mNodes{}, mContexts{}, mResources{}
    {

    }

    auto FGNodeControl::Clear() -> void {
        mContexts.clear();
        mResources.clear();
        mNodes.clear();
    }

    FGNodeBuilder::FGNodeBuilder( FGNode& node, FGNodeControl& control )
        : mGraphNode{ MKT_ADDRESSOF( node ) }, mNodeControl{ MKT_ADDRESSOF( control ) }
    {}

    auto FGNodeBuilder::Read( FGTextureHandle handle, FGPipelineStage state ) -> void {
        Read( handle.mHandle );
        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
        };
    }

    auto FGNodeBuilder::Write( FGTextureHandle handle, FGPipelineStage state ) -> void {
        Write( handle.mHandle );
        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
        };
    }

    auto FGNodeBuilder::Read( FGBufferHandle handle, FGPipelineStage state ) -> void {
        Read( handle.mHandle );
        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
        };
    }

    auto FGNodeBuilder::Write( FGBufferHandle handle, FGPipelineStage state ) -> void {
        Write( handle.mHandle );
        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
        };
    }

    auto FGNodeBuilder::UseResource( FGTextureHandle handle, FGPipelineStage state, FGResourceAccess access ) -> void {
        switch (access) {
            case FGResourceAccess::eNone:
                MKT_ASSERT( false, "Invalid resource access type" );
            case FGResourceAccess::eRead:
                Read( handle.mHandle );
                break;
            case FGResourceAccess::eWrite:
                Write( handle.mHandle );
                break;
        }

        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
            .mAccess = access,
        };
    }

    auto FGNodeBuilder::UseResource( FGBufferHandle handle, FGPipelineStage state, FGResourceAccess access ) -> void {
        switch (access) {
            case FGResourceAccess::eNone:
                MKT_ASSERT( false, "Invalid resource access type" );
            case FGResourceAccess::eRead:
                Read( handle.mHandle );
                break;
            case FGResourceAccess::eWrite:
                Write( handle.mHandle );
                break;
        }

        mGraphNode->mResourceStates[handle.mHandle] = FGResourceTrack {
            .mState = state,
            .mAccess = access,
        };
    }

    auto FGNodeBuilder::Read( FGResourceHandle handle ) -> void {
        MKT_ASSERT( handle != 0, "Invalid resource handle" );
        auto& resourceLatestVersion{ mNodeControl->mResources[handle].mVersions.back() };

        // Let writer finish first before we start reading to it (READ-AFTER-WRITE)
        if(resourceLatestVersion.HasWriter()) {
            mGraphNode->mDependsOn.emplace(resourceLatestVersion.mWriterPass);
        }

        // Register myself as reader for this resource
        resourceLatestVersion.mReaderPasses.push_back(mGraphNode->mName);

        // record for barrier insertion
        mGraphNode->mReadResources.push_back(handle);
    }

    auto FGNodeBuilder::Write( FGResourceHandle handle ) -> void {
        MKT_ASSERT( handle != 0, "Invalid resource handle" );
        auto& resourceLatestVersion{ mNodeControl->mResources[handle].mVersions.back() };

        // We depend on previous writer, let
        // it finish first (WRITE-AFTER-WRITE)
        if(resourceLatestVersion.HasWriter()) {
            mGraphNode->mDependsOn.emplace(resourceLatestVersion.mWriterPass);
        }

        // Readers must finish before this pass writes to the resource (WRITE-AFTER-READ)
        for(const auto& reader : resourceLatestVersion.mReaderPasses) {
            mGraphNode->mDependsOn.emplace(reader);
        }

        // Make this pass as the last writer for this resource
        if ( resourceLatestVersion.HasWriter() ) {
            mNodeControl->mResources[handle].mVersions.push_back( {} );
            mNodeControl->mResources[handle].mVersions.back().mWriterPass = mGraphNode->mName;
        } else {
            resourceLatestVersion.mWriterPass = mGraphNode->mName;
        }

        // record for barrier insertion
        mGraphNode->mWriteResources.push_back(handle);
    }

    FrameGraph::FrameGraph( IGpuDevice* device, ShaderLibrary* library )
        : mDevice{ device }, mShaderLibrary{ library }
    {
        mNodeControl = eastl::make_unique<FGNodeControl>();
        mResourceManager = eastl::make_unique<FGResourceManager>( mDevice );
        mReadbackManager = eastl::make_unique<FGReadbackManager>( mBlackboard, *mResourceManager );

        mFence = mDevice->CreateFence( mFenceValue++ );

        mGraphicsCommands = mDevice->CreateCommandList( QueueType::eGraphics );
        mComputeCommands = mDevice->CreateCommandList( QueueType::eCompute );
        mTransferCommands = mDevice->CreateCommandList( QueueType::eTransfer );

        mGraphicsCommands->SetEnableAutomaticBarriers( false );
        mGraphicsCommands->SetDebugName( "FG GraphicsCommands" );

        mComputeCommands->SetEnableAutomaticBarriers( false );
        mComputeCommands->SetDebugName( "FG ComputeCommands" );

        mTransferCommands->SetEnableAutomaticBarriers( false );
        mTransferCommands->SetDebugName( "FG TransferCommands" );

        mStatisticsManager = eastl::make_unique<FGStatisticsManager>();
    }

    auto FrameGraph::Compile() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mExecutionPlan.mSortedExecutionPasses.clear();
        mExecutionPlan.mExecutionTaskMap.clear();
        mExecutionPlan.mBarriers.clear();
        mExecutionPlan.mExecutionTaskGraph.clear();

        // Build edges
        BuildNodeEdges();

        // Cull nodes
        CullGraphNodes();

        // Barriers
        BuildNodeBarriers();

        // Create the contexts and executions tasks
        BuildExecutionContext();

#if !defined(NDEBUG)
        std::ostringstream oss{};
        mExecutionPlan.mExecutionTaskGraph.dump(oss);
        mExecutionPlan.mExecutionTaskGraph.name("FrameGraph TaskExecution");

        // to file example
        // std::ofstream file("taskflow.dot");
        // mExecutionPlan.mExecutionGraph.dump(file);
        // dot -Tpng taskflow.dot -o taskflow.png

        MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_CYAN, "FG Dependencies:\n{}", oss.str() );
#endif
    }

    auto FrameGraph::Execute() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mGraphicsCommands->Begin( { .mScopeName = "TaskGraph Graphics Commands" } );
        mComputeCommands->Begin( { .mScopeName = "TaskGraph Compute Commands" } );
        mTransferCommands->Begin( { .mScopeName = "TaskGraph Transfer Commands" } );

        BindResources( mGraphicsCommands );
        BindResources( mComputeCommands );

        for ( auto& passName: mExecutionPlan.mSortedExecutionPasses ) {
            if ( !mNodeControl->mNodes[passName].mIsAlive ) {
                continue;
            }

            auto& pass{ mNodeControl->mNodes[passName] };
            auto& ctx{ mNodeControl->mContexts.at(passName) };

            CommandListHandle commandList{};

            switch (pass.mType) {
                case FGPassType::eGraphics:
                    commandList = mGraphicsCommands;
                    break;
                case FGPassType::eCompute:
                    commandList = mComputeCommands;
                    break;
                case FGPassType::eTransfer:
                    commandList = mTransferCommands;
                    break;
                default:;
            }

            // Off load this work to workers threads?
            ctx.BeginPass( commandList );

            // Place pass barriers
            const auto& barriers{ mExecutionPlan.mBarriers[passName] };
            ctx.CommitBarriers( barriers );

            Timer timer{ false };
            const double elapsed{ timer.GetCurrentProgress( TimeUnit::eMicroseconds ) };

            // Run pass execute calback
            pass.mExecuteCallback( ctx, mBlackboard );

            FGNodeStatistics& stats{ *mStatisticsManager->GetNode( pass.mName ) };

            stats.mLastExecutionTime = elapsed;

            if (elapsed > stats.mMaxExecutionTime) {
                stats.mMaxExecutionTime = elapsed;
            } else {
                if (stats.mMinExecutionTime == 0) stats.mMinExecutionTime = stats.mLastExecutionTime;
                else if (elapsed < stats.mMinExecutionTime) stats.mMinExecutionTime = elapsed;
            }

            ctx.EndPass();
        }

        mGraphicsCommands->End();
        mComputeCommands->End();
        mTransferCommands->End();

        // I think I might end up having each command context own its own command list
        // because the way I see this code its three queues each running commands in parallel
        // which is not the case for Vulkan right now (Vulkan is using one queue now), so this assumes
        // these tasks or commands have no dependency which is not the case necessarily.
        auto submitInfoTransfer{ SubmitInfo{}
            .AddSignal( mFence, mFenceValue++ )
            .AddCommandList( mTransferCommands ) };
        RenderSystem::Get()->BatchSubmission(eastl::move(submitInfoTransfer), QueueType::eTransfer);

        auto submitInfoCompute{ SubmitInfo{}
            .AddCommandList( mComputeCommands ) };
        RenderSystem::Get()->BatchSubmission(eastl::move(submitInfoCompute), QueueType::eCompute);

        auto submitInfoGraphics{ SubmitInfo{}
            .AddCommandList( mGraphicsCommands ) };
        RenderSystem::Get()->BatchSubmission(eastl::move(submitInfoGraphics), QueueType::eGraphics);

        ProcessReadbackTasks();
    }

    auto FrameGraph::ProcessReadbackTasks() -> void {
        auto it{ mReadbackTasks.begin() };
        while ( it != mReadbackTasks.end() ) {
            if (!it->mHasBeenRegistered) {
                it->mHasBeenRegistered = true;
                it->mTaskID = mReadbackManager->RegisterCallback( it->mTask, it->mRunsPerFrame );
            }

            // Update the submission fence value
            mReadbackManager->UpdateTaskFenceValue( it->mTaskID, mFenceValue );

            if (!it->mRunsPerFrame) {
                // Runs only once, delete to not
                // update submission fence value again
                it = mReadbackTasks.erase( it );
            } else {
                ++it;
            }
        }
    }

    auto FrameGraph::ExecuteReadbacks() -> void {
        const u64 currentValue{ mFence->GetCompletionValue() };
        mReadbackManager->ExecuteCallbacks( currentValue );
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

    auto FrameGraph::GetStatisticsManager() const -> const FGStatisticsManager& {
        return *mStatisticsManager;
    }

    auto FrameGraph::GetTexture( FGTextureHandle handle ) const -> TextureHandle {
        return mResourceManager->Get( handle.mHandle ).mResource;
    }

    auto FrameGraph::GetBuffer( FGBufferHandle handle ) const -> BufferHandle {
        return mResourceManager->Get( handle.mHandle ).mResource;
    }

    auto FrameGraph::Create( IGpuDevice* device, ShaderLibrary* shaderLibrary ) -> eastl::unique_ptr<FrameGraph> {
        MKT_BEGIN_PROFILER_NAMED();
        return eastl::make_unique<FrameGraph>( device, shaderLibrary );
    }

    auto FrameGraph::BindResources( CommandListHandle commandList ) -> void {
        PipelineType bindPoint{ PipelineType::eInvalid };
        switch (commandList->GetQueueType()) {
            case QueueType::eGraphics:
                bindPoint = PipelineType::eGraphics;
                break;
            case QueueType::eCompute:
                bindPoint = PipelineType::eCompute;
                break;
            default:
                ;
        }

        // Just sanity check
        MKT_ASSERT( bindPoint != PipelineType::eInvalid, "Invalid pipeline bind point." );

        DescriptorTableHandle table{ mResourceManager->GetDescriptorTable() };
        PipelineLayoutHandle layout{ mResourceManager->GetPipelineLayout() };

        // Resource layout is predefined and fixed for all pipelines
        // see FGResourceManager implementation
        commandList->BindPipelineResources( BindResourcesDescription{}
            .AddResourceSet( 0, table.GetRaw() )
            .SetPipelineLayout( layout.GetRaw() )
            .SetBindPoint( bindPoint ) );
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

            mExecutionPlan.mSortedExecutionPasses.push_back( cur );
            for ( const auto& succ: passesMap[cur].mSuccessors ) {
                if ( --inDegrees[succ] == 0 ) { // all successors dependencies are done?
                    q.push( succ );      // successor is now ready
                }
            }
        }

        // If we didn't visit every pass, the graph has a cycle, invalid.
        MKT_ASSERT( mExecutionPlan.mSortedExecutionPasses.size() == passesMap.size(), "Cycle detected!" );
    }

    auto FrameGraph::CullGraphNodes() -> void {
        auto& passesMap{ mNodeControl->mNodes };

        // Cull Passes
        // This step is done if we for instance remove or disable a pass
        // we need to disable the ones depending on it?
        if (!mExecutionPlan.mSortedExecutionPasses.empty()) {
            for(auto reverseIt{ mExecutionPlan.mSortedExecutionPasses.rbegin() }; reverseIt != mExecutionPlan.mSortedExecutionPasses.rend(); ++reverseIt) {
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
        for ( const auto& passName : mExecutionPlan.mSortedExecutionPasses ) {
            const auto& pass{ mNodeControl->mNodes[passName] };
            if ( !pass.mIsAlive ) {
                continue;
            }

            auto recordTransition = [&]( FGResourceHandle resourceHandle, FGResourceAccess access ) -> void {
                // You cannot set a barrier twice for the same resource in the same pass
                if (mExecutionPlan.mBarriers[passName].contains( resourceHandle )) {
                    return;
                }

                // Barriers are really only needed for dependencies where one of the accesses
                // is a write (WAW, WAR, RAW) or if a layout transition is required
                const FGPipelineStage prevState{ mNodeControl->mResources[resourceHandle].mCurrentState };
                const FGPipelineStage nextState{ mNodeControl->mNodes[passName].mResourceStates[resourceHandle].mState };

                if (prevState == FGPipelineStage::eUnknown) {
                    // First use -> just set state, no barrier
                    // I think I wil probably remove this and transition all resources to general layout so that
                    // I only do the next check for barriers for each pass
                    mExecutionPlan.mBarriers[passName][resourceHandle] = FGBarrier{
                        resourceHandle,
                        access,
                        prevState,
                        nextState };
                    mNodeControl->mResources[resourceHandle].mCurrentState = nextState;
                }
                else if (prevState != nextState || access == FGResourceAccess::eWrite ) {
                    mExecutionPlan.mBarriers[passName][resourceHandle] = FGBarrier{
                        resourceHandle,
                        access,
                        prevState,
                        nextState };

                    mNodeControl->mResources[resourceHandle].mCurrentState = nextState;
                }
            };

            for ( auto& h: pass.mReadResources ) {
                recordTransition( h, FGResourceAccess::eRead );
            }

            for ( auto& h: pass.mWriteResources ) {
                recordTransition( h, FGResourceAccess::eWrite );
            }
        }
    }

    auto FrameGraph::BuildExecutionContext() -> void {
        // TODO: Pending redesign, after parallel command recording is properly implemented
        for (auto& [passName, node] : mNodeControl->mNodes ) {
            mNodeControl->mContexts.try_emplace( passName, MKT_ADDRESSOF( node ), mResourceManager.get(), mStatisticsManager.get() );
        }

        // Create tasks
        for (auto& passName : mExecutionPlan.mSortedExecutionPasses) {
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

            auto task = mExecutionPlan.mExecutionTaskGraph.emplace([this, passName, cmd]() mutable {
                auto& pass{ mNodeControl->mNodes[passName] };
                auto& ctx{ mNodeControl->mContexts.at(passName) };

                if ( pass.mType != FGPassType::eGeneric ) {
                    cmd->Begin( {} );
                }

                // Off load this work to workers threads
                // Vulkan could use secondary command buffers here
                ctx.BeginPass( cmd );

                // Place pass barriers
                const auto it{ mExecutionPlan.mBarriers.find( passName ) };
                if (it != mExecutionPlan.mBarriers.end()) {
                    ctx.CommitBarriers( it->second );
                }

                pass.mExecuteCallback( ctx, mBlackboard );
                ctx.EndPass();

                if ( pass.mType != FGPassType::eGeneric ) {
                    cmd->End();
                }
            });

            task.name( passName.c_str() );

            mExecutionPlan.mExecutionTaskMap[MKT_ADDRESSOF( mNodeControl->mNodes[passName] )] = task;
        }

        // connect dependencies
        for (auto& passName : mExecutionPlan.mSortedExecutionPasses) {
            auto& pass = mNodeControl->mNodes[passName];

            if (!pass.mIsAlive)
                continue;

            auto& task = mExecutionPlan.mExecutionTaskMap[&pass];

            for (auto& succName : pass.mSuccessors) {
                auto& succ = mNodeControl->mNodes[succName];

                if (!succ.mIsAlive)
                    continue;

                mExecutionPlan.mExecutionTaskMap[&succ].succeed(task);
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
                .SetDepthFormat( desc.mDepthFormat ) };

            // Color formats
            for ( const auto &format: desc.mColorFormats ) {
                graphicsPipelineDesc.AddColorFormat( format );
            }

            // Shaders
            for ( const auto &shader: desc.mShaders ) {
                graphicsPipelineDesc.AddShader( mShaderLibrary->LoadShader(shader.second, GetShaderFlagsFromStage( shader.first )) );
            }

            mNodeControl->mResources[resource.mResourceID] = FGNodeResource {
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

            mNodeControl->mResources[resource.mResourceID] = FGNodeResource {
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

        return FGPipelineHandle{ resource.mResourceID };
    }

    auto FrameGraph::Create( const FGBufferDescription &desc ) -> FGBufferHandle {
        auto& resource{ mResourceManager->Allocate( FGResourceType::eBuffer ) };
        auto bufferDesc{ BufferCreateDescription{}
            .SetBufferUsage( desc.mBufferUsageFlags )
            .SetName( desc.mName )
            .ForElement( desc.mElementSizeBytes, desc.mElementCount )
            .SetHeapType( desc.mHeapType )
            .SetCpuAccessType( desc.mHeapType == HeapType::eUpload ? CpuAccessType::eWrite : CpuAccessType::eRead )
            .SetByteSize( desc.mElementSizeBytes )
        };

        if (!desc.mInitialContents.IsEmpty()) {
            bufferDesc.SetInitialData( desc.mInitialContents );
        }

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource {
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

        return FGBufferHandle{ .mHandle = resource.mResourceID };
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

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource {
            .mName{ string::Format( "FG Loaded Texture {}", path.GetC_Str() ) },
            .mDescription = textureDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{}, // No writers
                .mReaderPasses{}, // No readers
            }},
            .mIsImported = true
        };

        return FGTextureHandle{ .mHandle = resource.mResourceID };
    }

    auto FrameGraph::ImportTexture( TextureHandle handle ) -> FGTextureHandle {
        if (handle.IsEmpty()) {
            return {};
        }

        auto& resource{ mResourceManager->Allocate( FGResourceType::eTexture, handle.GetRaw() ) };
        resource.mResource = handle;

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource {
            .mName{ string::Format( "FG External Texture {}", handle->GetDebugName() ) },
            .mVersions{ ResourceVersion{
                .mWriterPass{}, // No writers
                .mReaderPasses{}, // No readers
            }},
            .mIsImported = true
        };

        return FGTextureHandle{ .mHandle = resource.mResourceID };
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
            .SetHeapType( desc.mHeapType )
            .SetMultisampling( desc.mMultisampling )
            .SetUsage( desc.mUsage )
            .SetFormat( desc.mFormat ) };

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource {
            .mName{ desc.mName },
            .mDescription = textureDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{}, // No writers
                .mReaderPasses{}, // No readers
            }},
            .mIsImported = false
        };

        resource.mResource = mDevice->CreateTexture( textureDesc );
        checked_cast<DeviceObject*>( resource.mResource.GetRaw() )->SetDebugName( desc.mName );
        return FGTextureHandle{ .mHandle = resource.mResourceID };
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

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource {
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

        return FGSamplerHandle{ .mHandle = resource.mResourceID };
    }

    auto FrameGraph::RegisterReadback( FGReadbackTask::Callback&& execute, bool runEveryFrame ) -> void {
        mReadbackTasks.emplace_back( ReadbackTask {
            .mTask = eastl::move( execute ),
            .mRunsPerFrame = runEveryFrame,
            .mHasBeenRegistered = false,
        });
    }
}// namespace Mikoto