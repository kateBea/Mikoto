//    Copyright 2026 ケイト
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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_DEVICE_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_DEVICE_HH

#include <mutex>

#include <EASTL/deque.h>
#include <EASTL/atomic.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/memory.h>
#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/functional.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Memory/Allocator.hh>
#include <Memory/MemoryArena.hh>
#include <Memory/LinearAllocator.hh>
#include <Memory/FreeListAllocator.hh>

#include <Filesystem/Path.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Vulkan/VulkanDlss.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Rhi/ShaderTable.hh>
#include <Renderer/Core/RenderContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanBuffer.hh>
#include <Renderer/Rhi/Vulkan/VulkanShader.hh>
#include <Renderer/Rhi/Vulkan/VulkanTexture.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanInstance.hh>
#include <Renderer/Rhi/Vulkan/VulkanPipeline.hh>
#include <Renderer/Rhi/Vulkan/VulkanSwapchain.hh>
#include <Renderer/Rhi/Vulkan/VulkanQueryPool.hh>
#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

#if defined( MIKOTO_ENABLE_TRACY )
namespace tracy {
    class VkCtx;
}
#endif

namespace mikoto::renderer::vulkan {

    class Fence final : public rhi::IFence {
    public:
        explicit Fence( core::u64 initialValue );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetCompletionValue() const -> core::u64 override;

        MKT_NODISCARD auto Signal( core::u64 fenceValue ) -> bool override;
        MKT_NODISCARD auto Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType ) const -> rhi::Object override;

        MKT_NODISCARD operator VkSemaphore() const;

        ~Fence() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        mutable core::u64 mTimeline{};
        VkSemaphore mSemaphore{ VK_NULL_HANDLE };
    };


    class BinarySemaphore final : public rhi::DeviceObject {
    public:
        explicit BinarySemaphore() = default;

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD operator VkSemaphore() const;

        ~BinarySemaphore() override;

        using renderer::rhi::DeviceObject::Initialize;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        VkSemaphore mSemaphore{ VK_NULL_HANDLE };
    };
    using BinarySemaphoreHandle = core::Ref<BinarySemaphore>;


    struct GpuUploadAllocation {
        rhi::IBuffer* mBuffer{};

        // This is the mapped address we should be writing to,
        // it corresponds to the beginning of the slice or
        // range that was given to this allocation.
        // You do not need to offset into it or anything
        void* mMappedMemory{};

        // Size of this sub-allocation
        core::usize mSize{};

        // Specifies the offset of this allocation within the large
        // buffer it was allocated from
        core::usize mOffset{};

        // Metadata to track usage
        memory::Allocation mAllocation{};
        eastl::atomic_flag mInUse{ true };
    };

    // Manages intermediate buffers that are used to upload data from CPU
    // to CPU, whenever we want to upload data from the CPU to the GPU
    // if the GPU buffer is not writeable from CPU we copy to this intermediate buffer
    // and issue a copy command. In order to implement this we can use a first fit or best fit
    // allocator approach, see the GeometryAllocator in the MeshCulling.hh file
    // we can just have a huge chunk that grows as we need??
    class GpuUploadManager final {
    public:
        explicit GpuUploadManager( rhi::IGpuDevice* device );

        auto SubAllocate( core::usize byteSize, core::usize alignment = 4 ) -> GpuUploadAllocation*;
        auto ReclaimMemory() -> void;

        ~GpuUploadManager();

    private:
        struct StagingAllocation {
            rhi::BufferHandle mBuffer{};
            eastl::unique_ptr<memory::MemoryArena<rhi::IBuffer, memory::FreeListFirstFitAllocator>> mMemoryArena{};
        };

    private:
        auto CreateBuffer( core::usize minimumSize = MKT_MEGABYTES( 512 ) ) -> StagingAllocation*;
        auto CreateSubAllocation( rhi::IBuffer* buffer ) -> GpuUploadAllocation*;

    private:
        static constexpr core::u32 kMaxBuffers{ 32 };
        static constexpr core::u32 kMaxSubAllocations{ 100 };

        std::mutex mMutex{};

        rhi::IGpuDevice* mDevice{};
        ankerl::unordered_dense::map<rhi::IBuffer*, eastl::unique_ptr<StagingAllocation>> mBuffers{};
        ankerl::unordered_dense::map<rhi::IBuffer*, eastl::fixed_vector<eastl::unique_ptr<GpuUploadAllocation>, kMaxSubAllocations>> mSubAllocations{};
    };

    struct RecordingContext {
        rhi::CommandListStatistics mStatistics{};

        core::u64 mSubmissionID{};
        VkCommandPool mCommandPool{};
        VkCommandBuffer mCommandBuffer{};

#if defined( MIKOTO_ENABLE_TRACY )
        eastl::shared_ptr<tracy::VkCtx> mGpuProfiler{};
#endif

        // Resources that I'm currently using,
        // I keep a strong reference to them to avoid
        // destroying them while this command buffer is still being executed
        ankerl::unordered_dense::map<const renderer::rhi::DeviceObject*, renderer::rhi::DeviceObjectHandle> mInFlightResources{};

        // Suballocations that I'm currently using
        eastl::fixed_vector<GpuUploadAllocation*, 10> mInFlightSubAllocations{};
    };

    class CommandList final : public rhi::ICommandList {
    public:
        explicit CommandList( rhi::QueueType queueType, rhi::IQueue* queue, core::u32 familyIndex );

        MKT_NODISCARD auto GetStatistics() const -> const rhi::CommandListStatistics& override;

        auto Open( const rhi::CommandListBeginDescription& desc ) -> void override;
        auto Close() -> void override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        auto SetTransition( const rhi::TransitionDescription& description ) -> void override;
        auto SetBarrier( const rhi::BarrierDescription& description ) -> void override;

        auto AcquireResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void override;
        auto ReleaseResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* destQueue ) -> void override;

        auto AcquireResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void override;
        auto ReleaseResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* destQueue ) -> void override;

        auto SetEnableAutomaticBarriers( bool enable ) -> void override;

        auto SetClearColor( rhi::ITexture* renderTarget, rhi::Color color ) -> void override;

        auto Write( rhi::IBuffer* src, rhi::ITexture* dest ) -> void override;
        auto Write( rhi::ITexture* texture, const void* data, core::usize byteSize ) -> void override;
        auto Copy( rhi::ITexture* src, const rhi::TextureSlice& srcSlice, rhi::ITexture* dest, const rhi::TextureSlice& destSlice ) -> void override;

        auto Resolve( renderer::rhi::ITexture* src, const renderer::rhi::TextureSlice& srcSlice, renderer::rhi::ITexture* dest, const renderer::rhi::TextureSlice& destSlice ) -> void override;

        auto Write( rhi::IBuffer* buffer, core::usize destOffset, const void* data, core::usize byteSize ) -> void override;
        auto Write( rhi::IBuffer* buffer, const void* data, core::usize byteSize ) -> void override;
        auto Copy( rhi::IBuffer* src, rhi::IBuffer* dest ) -> void override;
        auto Copy( rhi::IBuffer* src, rhi::IBuffer* dest, core::usize destOffset ) -> void override;

        auto Copy( rhi::IBuffer* dest, rhi::ITexture* src ) -> void override;
        auto Copy( rhi::IBuffer* dest, rhi::ITexture* src, const renderer::rhi::TextureSlice& srcSlice ) -> void override;

        auto BeginRenderPass( rhi::RenderPassDescription& state ) -> void override;
        auto EndRenderPass() -> void override;

        auto BindPipeline( rhi::IPipeline* pipeline ) -> void override;

        // I am not sure if I wanna have these because the viewport is set on the images we
        // render to so it makes more sense to tie them to the graphics state when we specify the render targets
        auto SetViewport( eastl::span<const rhi::Viewport> viewports ) -> void override;
        auto SetScissors( eastl::span<const rhi::Rect> scissorRects ) -> void override;
        auto SetViewportState( const rhi::ViewportState& vs ) -> void override;

        auto SetPolygonLineWidth( core::f32 width ) -> void override;

        auto BindIndexBuffer( rhi::IBuffer* buffer ) -> void override;
        auto BindIndirectBuffer( rhi::IBuffer* buffer ) -> void override;
        auto BindVertexBuffer( const rhi::VertexBufferBinding& binding ) -> void override;
        auto BindVertexBuffers( eastl::span<const rhi::VertexBufferBinding> bindings ) -> void override;

        auto BindPipelineResources( const rhi::BindResourcesDescription& desc ) -> void override;

        auto Draw( const rhi::DrawArguments& args ) -> void override;
        auto DrawIndexed( const rhi::DrawArguments& args ) -> void override;

        auto DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void override;
        auto DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void override;

        auto Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void override;

        auto DispatchRays( const rhi::RayTracingDispatchDescription& desc ) -> void override;

        auto BindShaderTable( rhi::IShaderTable* table ) -> void override;

        auto SetPushConstants( rhi::IPipelineLayout* pipelineLayout, const void* data, core::usize byteSize, rhi::ShaderFlags visibility ) -> void override;

        auto BeginDebugLabel( eastl::string_view name, rhi::Color color ) -> void override;
        auto EndDebugLabel() -> void override;

#if defined( MIKOTO_ENABLE_TRACY )
        auto CollectGpuProfile() -> void override;
        auto BeginGpuProfileZone( eastl::string_view name, std::source_location source ) -> void override;
        auto EndGpuProfileZone() -> void override;
#endif

        auto Reset( rhi::IQueryPool* pool ) -> void override;
        auto BeginQuery( rhi::IQueryPool* pool, core::u32 index ) -> void override;
        auto EndQuery( rhi::IQueryPool* pool, core::u32 index ) -> void override;

        auto WriteTimestamp( rhi::IQueryPool* pool, core::u32 index ) -> void override;

        auto Resolve( rhi::IQueryPool* pool, core::u32 first, core::u32 count ) -> void override;

        auto Build( rhi::IAccelerationStructure* structure ) -> void override;
        auto DecodeVideo( const rhi::VideoDecodeDescription& description ) -> void override;

        MKT_NODISCARD auto GetQueueType() const -> rhi::QueueType override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        // Native SDK recordings must retain their device resources until GPU completion.
        auto RetainResource( const rhi::DeviceObject* resource ) -> void;

        // Vulkan Specifics
        MKT_NODISCARD operator VkCommandBuffer() const;

        auto ValidateSubmission( const rhi::IQueue* queue ) const -> void;
        auto MarkExecuted( rhi::IQueue* queue, core::u64 submissionID ) -> void;
        auto ReleaseCompletedResources() -> void;

        ~CommandList() override;

    private:
        auto SetTransition( rhi::IBuffer*, rhi::ResourceStates ) -> void;
        auto SetTransition( rhi::ITexture*, rhi::ResourceStates ) -> void;

        auto SetBarrier( const rhi::BufferBarrierDescription& ) -> void;
        auto SetBarrier( const rhi::TextureBarrierDescription& ) -> void;

        auto CommitBarriers() -> void;

        auto ValidateQueryPool( rhi::IQueryPool* pool ) const -> QueryPool&;

        auto Initialize() -> void override;
        auto Destroy() -> void override;

        auto ClearState() -> void;
        auto ClearState( RecordingContext& context ) -> void;

    private:
        rhi::IQueue* mQueue{};
        rhi::IBuffer* mIndirectBuffer{};

#if defined( MIKOTO_ENABLE_TRACY )
        struct GpuProfileZone;
        eastl::unique_ptr<GpuProfileZone> mGpuProfileZone{};
#endif

        core::u32 mQueueFamilyIndex{};
        // Retained by the recording's resource list; cleared on Open and non-ray binds.
        rhi::RayTracingPipeline* mBoundRayTracingPipeline{};
        rhi::IShaderTable* mBoundShaderTable{};

        VkCommandBuffer mCurrentCommandBuffer{};

        GpuUploadManager* mUploadManager{ nullptr };

        // We picked 5 at most, but can grow if needed
        // there are generally 3 frames in flight at most, more
        // may introduce unnecessary latency. With 4 we could be sure
        // that we will have at least one command buffer we can recycle
        // because GPU has finished executing its commands
        core::u32 mRecordingContextIndex{};
        static constexpr core::u32 kMaxRecordingContext{ 5 };
        eastl::vector<RecordingContext> mRecordingContext{};

        mutable std::mutex mRecordingMutex{};

        bool mRecording{ false };
        bool mReadyForSubmission{ false  };
        bool mIsRenderScopeActive{ false };
        bool mEnableAutomaticBarriers{ true };
        
        eastl::vector<VkBufferMemoryBarrier2> mBufferBarriers{};
        eastl::vector<VkImageMemoryBarrier2> mImageBarriers{};

        rhi::QueueType mQueueType{ rhi::QueueType::eInvalid };
        core::u32 mSelfManagedCommandListCount{ 4 };

        // For debug
        rhi::Color mLabelColor{};
        eastl::string mRecordingScopeName{};
        eastl::string mRenderingScopeName{};
    };


    struct SubmitSemaphoresInfo {
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 5> mWaitSemaphores{};
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 5> mSignalSemaphores{};

        eastl::fixed_vector<rhi::CommandListHandle, 5> mCommands{};
        // The native submit arrays borrow these semaphores until completion.
        eastl::vector<rhi::DeviceObjectHandle> mSyncObjects{};

        auto AddCommandList( rhi::CommandListHandle cmd ) -> SubmitSemaphoresInfo&;

        auto AddWaitFence( rhi::FenceHandle fence, core::u64 value, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo&;
        auto AddSignalFence( rhi::FenceHandle fence, core::u64 value, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo&;

        auto AddWaitSemaphore( BinarySemaphoreHandle semaphore, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo&;
        auto AddSignalSemaphore( BinarySemaphoreHandle semaphore, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo&;
    };

    // Every Queue is a unique VkQueue this is very important
    // cause submission to a queue via ExecuteCommands is required to be thread safe
    // bye the RHI, to guarantee that every instance of Queue musts protect its own VkQueue object
    class Queue final : public rhi::IQueue {
    public:
        explicit Queue( rhi::QueueType type, rhi::QueueOpSupportFlags opFlags, core::u32 queueFamilyIndex, core::u32 queueIndex = 0 );

        auto ExecuteCommandLists( const renderer::rhi::SubmitInfo& submitInfo ) -> void override;

        // Vulkan Specifics
        auto WaitCompletionValue( core::u64 value ) -> void;
        auto ExecuteCommandLists( SubmitSemaphoresInfo&& submitInfo ) -> core::u64;

        // We still specify queue type because this queue might be a graphics queue
        // but still support transfer operations
        auto AllocateCmdList( rhi::QueueType type ) -> renderer::rhi::CommandListHandle;

        auto WaitIdle() -> void;
        auto CollectGarbage() -> void;

        MKT_NODISCARD auto GetType() const -> rhi::QueueType override;
        MKT_NODISCARD auto GetOpSupportFlags() const -> rhi::QueueOpSupportFlags override;

        MKT_NODISCARD auto GetCurrentTimeline() -> core::u64;

        MKT_NODISCARD auto Present( const VkPresentInfoKHR& info ) -> VkResult;

        MKT_NODISCARD auto GetQueue() const -> VkQueue;
        MKT_NODISCARD auto GetQueueIndex() const -> core::u32;
        MKT_NODISCARD auto GetFamilyIndex() const -> core::u32;

        // Conversion operators
        operator core::u32() const;// Queue family index
        operator VkQueue() const;  // Logical queue

        ~Queue() override;

    protected:
        auto Destroy() -> void override;
        auto Initialize() -> void override;

    private:
        friend class CommandList;

#if defined( MIKOTO_ENABLE_TRACY )
        auto InitializeGpuProfiler( core::u64 connection ) -> void;
        eastl::shared_ptr<tracy::VkCtx> mGpuProfiler{};
        eastl::optional<core::u64> mGpuProfilerConnection{};
#endif

        core::u32 mFamilyIndex{ 0 };
        core::u32 mQueueIndex{ 0 };

        core::f32 mPriority{ 1.0f };
        VkQueue mQueue{ VK_NULL_HANDLE };

        static constexpr core::u32 kMaxSubmits{ 115 };

        rhi::QueueType mType{ rhi::QueueType::eInvalid };
        rhi::QueueOpSupportFlags mOpSupportFlags{ rhi::QueueOpSupportFlagsBits::Graphics };

        eastl::atomic<core::u64> mTimelineValue{};
        rhi::FenceHandle mTimelineSemaphore{};

        std::mutex mSubmissionMutex{};

        // For debug
        eastl::string mSubmissionLabel{};
        rhi::Color mSubmissionLabelColor{};
    };


    // -------------------------------------------------
    // Initial implementation taken from https://github.com/vblanco20-1/Vulkan-Descriptor-Allocator.git
    // and adapted to Mikoto environment
    // -------------------------------------------------
    class IDescriptorAllocatorPool;

    // initialization ------------------
    // create an allocator pool. Its recommended you store it on a unique_ptr
    // allocator_pool = vke::DescriptorAllocatorPool::Create(device, numFrames);
    //
    // when allocating new VkDescriptorPools, it will now reserve 3 uniform buffer descriptors per descriptor set.
    // allocator_pool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3.f /* multiplier*/) = 0;
    //
    //
    // //when rendering ------------------
    // //get an allocator handle. This is thread safe,
    // //but can be expensive as it will allocate new VkDescriptorPools if there are no reusable ones.
    // //Its recommended to create them only on a per-task basis.
    // auto allocator_handle = allocator_pool->GetAllocator();
    //
    // VkDescriptorSet newSet;
    // if( allocator_handle.Allocate(descriptorLayout,newSet)) {
    //     //if it returns true, the allocation succeeded and now you can use the newly allocated set.
    // }
    //
    // once per frame
    // this will update the internal state to the next frame.
    // By doing this, the allocator will cycle its frames (numFrames).
    // When a frame is reused, it will clear those descriptor pools for reuse.
    // Don't keep allocator handles alive when calling Flip
    // allocator_pool->Flip();
    struct DescriptorAllocatorHandle {
        friend class IDescriptorAllocatorPool;
        DescriptorAllocatorHandle() = default;
        auto operator=( const DescriptorAllocatorHandle& ) -> DescriptorAllocatorHandle& = delete;

        ~DescriptorAllocatorHandle();
        DescriptorAllocatorHandle( DescriptorAllocatorHandle&& other ) noexcept;
        auto operator=( DescriptorAllocatorHandle&& other ) noexcept -> DescriptorAllocatorHandle&;

        //return this handle to the pool. Will make this handle orphaned
        auto Return() -> void;

        //allocate new descriptor. handle has to be valid
        //returns true if allocation succeeded, and false if it didn't
        //will mutate the handle if it requires a new vkDescriptorPool
        MKT_NODISCARD auto Allocate( const VkDescriptorSetLayout& layout, VkDescriptorSet& builtSet ) -> bool;

        IDescriptorAllocatorPool* mOwnerPool{};
        VkDescriptorPool mDescriptorPool{};
        core::i8 mPoolIndex{};
    };


    class IDescriptorAllocatorPool {
    public:
        virtual ~IDescriptorAllocatorPool() = default;

        MKT_NODISCARD static auto Create( const VkDevice& device, core::i32 nFrames = 3 ) -> eastl::unique_ptr<IDescriptorAllocatorPool>;

        //not thread safe
        //switches default allocators to the next frame. When frames loop it will reset the descriptors of that frame
        virtual auto Flip() -> void = 0;

        //not thread safe
        //override the pool size for a specific descriptor type. This will be used new pools are allocated
        virtual auto SetPoolSizeMultiplier( VkDescriptorType type, float multiplier ) -> void = 0;

        //thread safe, uses lock
        //get handle to use when allocating descriptors
        MKT_NODISCARD virtual auto GetAllocator() -> DescriptorAllocatorHandle = 0;
    };


    struct DescriptorLayoutBuilder {
        eastl::vector<VkDescriptorSetLayoutBinding> mBindings{};

        auto Build( const void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0 ) const -> VkDescriptorSetLayoutCreateInfo;
        // if the descriptor is an array arraySize != -1
        auto WithBinding( core::u32 binding, VkDescriptorType type, core::i32 arraySize, VkShaderStageFlags shaderStages ) -> DescriptorLayoutBuilder&;
    };


    struct DescriptorWriter final {
        // Keep descriptor payloads stable until UpdateSet consumes the writes.
        eastl::vector<VkWriteDescriptorSet> mWrites{};
        eastl::deque<VkDescriptorImageInfo> mImageInfos{};
        eastl::deque<VkDescriptorBufferInfo> mBufferInfos{};
        eastl::deque<VkAccelerationStructureKHR> mAccelerationStructures{};
        eastl::deque<VkWriteDescriptorSetAccelerationStructureKHR> mAccelerationInfos{};

        VkShaderStageFlags mShaderStages{};

        auto WriteSampler( core::u32 binding, VkSampler sampler, core::u32 arrayIndex = 0 ) -> DescriptorWriter&;
        auto WriteAccelerationStructure( core::u32 binding, VkAccelerationStructureKHR structure, core::u32 arrayIndex = 0 ) -> DescriptorWriter&;
        auto WriteBuffer( core::u32 binding, VkBuffer buffer, core::usize size, core::usize offset, VkDescriptorType type, core::u32 arrayIndex = 0 ) -> DescriptorWriter&;
        auto WriteImage( core::u32 binding, VkImageView image, VkDescriptorType type, VkImageLayout layout, core::u32 arrayIndex = 0 ) -> DescriptorWriter&;

        auto SetVisibility( VkShaderStageFlags visibility ) -> DescriptorWriter&;

        auto Clear() -> void;
        auto UpdateSet( VkDevice device, VkDescriptorSet set ) -> void;
    };


    class BindingLayout final : public rhi::IBindingLayout {
    public:
        explicit BindingLayout( const rhi::BindingLayoutDescription& desc );
        explicit BindingLayout( const rhi::DescriptorTableLayoutDescription& desc );

        MKT_NODISCARD auto IsBindless() const -> bool override;
        MKT_NODISCARD auto GetRegisterSpace() const -> core::u32 override;
        MKT_NODISCARD auto GetBindlessLayoutDesc() const -> const rhi::DescriptorTableLayoutDescription&;

        auto SetDebugName( eastl::string_view name ) -> void override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD operator VkDescriptorSetLayout() const;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        core::u32 mSetIndex{};
        bool mIsBindless{ false };

        rhi::BindingLayoutDescription mBindingLayoutDesc{};
        rhi::DescriptorTableLayoutDescription mBindlessLayoutDesc{};

        VkDescriptorSetLayout mDescriptorSetLayout{};
        VkDescriptorSetLayoutCreateInfo mCreateInfo{};
    };


    class DescriptorTable : public rhi::IDescriptorTable {
    public:
        MKT_NODISCARD auto GetBindingLayout() const -> rhi::BindingLayoutHandle override;
        auto ReleaseIndex( rhi::DescriptorTableIndex index, rhi::ResourceType type, core::u32 slot ) -> void override;
        explicit DescriptorTable( rhi::BindingLayoutHandle setLayout );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetCapacity( core::u32 slot ) const -> core::u32 override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto FreeIndex( core::u32 slot, rhi::DescriptorTableIndex index ) const -> void;
        MKT_NODISCARD auto AllocateNextIndex( core::u32 slot ) const -> rhi::DescriptorTableIndex;

        ~DescriptorTable() override;

    private:
        friend class CommandList;
        friend class Device;

        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        VkDescriptorSet mDescriptorSet{};
        VkDescriptorSetAllocateInfo mCreateInfo{};

        DescriptorAllocatorHandle mDescriptorAllocatorHandle{};

        mutable std::mutex mDescriptorMutex{};

        mutable ankerl::unordered_dense::map<core::usize,
        ankerl::unordered_dense::set<renderer::rhi::DescriptorTableIndex>>
            mDescriptorTableItemIndices{};

        mutable ankerl::unordered_dense::map<core::u64, rhi::DeviceObjectHandle> mResources{};

        ankerl::unordered_dense::map<core::usize, core::usize> mSlotSize{};

        rhi::BindingLayoutHandle mBindingLayout{};
    };


    class BindingTable : public rhi::IBindingTable {
    public:
        explicit BindingTable( const rhi::BindingTableDescription& desc, rhi::BindingLayoutHandle layout );

        auto SetDebugName( eastl::string_view name ) -> void override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~BindingTable() override;

    protected:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        VkDescriptorSet mDescriptorSet{};
        VkDescriptorSetAllocateInfo mCreateInfo{};

        DescriptorAllocatorHandle mDescriptorAllocatorHandle{};

        rhi::BindingLayoutHandle mBindingLayout{};
        rhi::BindingTableDescription mBindingDescription{};
        eastl::vector<rhi::DeviceObjectHandle> mResources{};
    };


    class InputLayout : public rhi::IInputLayout {
    public:
        explicit InputLayout( const rhi::InputLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetVertexBindingDesc() const -> const eastl::fixed_vector<VkVertexInputBindingDescription, rhi::kMaxVertexBindings>&;
        MKT_NODISCARD auto GetVertexAttributesDesc() const -> const eastl::fixed_vector<VkVertexInputAttributeDescription, rhi::kMaxVertexAttributes>&;

        MKT_NODISCARD auto GetNumAttributes() const -> core::u32 override;
        MKT_NODISCARD auto GetAttributeDescription( core::u32 index ) const -> const rhi::VertexAttributeDescription& override;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::InputLayoutCreateDescription mDesc{};
        eastl::fixed_vector<VkVertexInputBindingDescription, rhi::kMaxVertexBindings> mVertexBindingDescriptions{};
        eastl::fixed_vector<VkVertexInputAttributeDescription, rhi::kMaxVertexAttributes> mVertexAttributeDescriptions{};
    };


    class PipelineLayout : public rhi::IPipelineLayout {
    public:
        explicit PipelineLayout( const rhi::PipelineLayoutCreateDescription& desc );

        auto SetDebugName(eastl::string_view name) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        // Vulkan Specifics
        MKT_NODISCARD operator VkPipelineLayout() const;

        ~PipelineLayout() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        VkPipelineLayout mPipelineLayout{};

        rhi::PipelineLayoutCreateDescription mDescription{};
    };

    class ShaderTable final : public rhi::IShaderTable {
    public:
        enum class AddressRegion {
            eRayGeneration,
            eRayMiss,
            eHitGroup,
            eCallable,
        };

    public:
        explicit ShaderTable( const rhi::ShaderTableDescription& description );

        MKT_NODISCARD auto GetDesc() const -> rhi::ShaderTableDescription const& override;
        MKT_NODISCARD auto GetNumEntries() const -> core::u32 override;
        MKT_NODISCARD auto GetPipeline() const -> rhi::IPipeline* override;

        MKT_NODISCARD auto GetBuffer() const -> const rhi::BufferHandle&;
        MKT_NODISCARD auto GetRegion( AddressRegion region ) const -> const VkStridedDeviceAddressRegionKHR&;

        ~ShaderTable() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::ShaderTableDescription mDescription{};
        rhi::RayTracingPipelineHandle mPipeline{};
        rhi::BufferHandle mSbtBuffer{};

        VkStridedDeviceAddressRegionKHR mRayGenRegion{};
        VkStridedDeviceAddressRegionKHR mMissRegion{};
        VkStridedDeviceAddressRegionKHR mHitGroupsRegion{};
        VkStridedDeviceAddressRegionKHR mCallableRegion{};
    };

    class Device final : public rhi::IGpuDevice {
    public:
        explicit Device( const rhi::GpuDeviceCreateInfo& createInfo );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const rhi::TextureCreateDescription& description ) -> rhi::TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( rhi::ObjectType type, rhi::Object object, const rhi::TextureCreateDescription& description ) -> rhi::TextureHandle override;

        MKT_NODISCARD auto CreateBuffer( const rhi::BufferCreateDescription& description ) -> rhi::BufferHandle override;

        // Creates a video buffer through the shared allocator using the supplied codec profile.
        MKT_NODISCARD auto CreateBuffer( const rhi::BufferCreateDescription& description, const VkVideoProfileInfoKHR& profile ) -> rhi::BufferHandle;

        MKT_NODISCARD auto CreateSampler( const rhi::SamplerCreateDescription& description ) -> rhi::SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const rhi::ComputePipelineDescription& description ) -> rhi::PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const rhi::GraphicsPipelineDescription& description ) -> rhi::PipelineHandle override;

        MKT_NODISCARD auto CreatePipeline( const rhi::RayTracingPipelineDescription& description ) -> rhi::RayTracingPipelineHandle override;

        MKT_NODISCARD auto CreateShaderTable( const rhi::ShaderTableDescription& description ) -> rhi::ShaderTableHandle override;
        MKT_NODISCARD auto CreateAccelStructure( const rhi::AccelStructureCreateDescription& description ) -> rhi::AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( rhi::QueueType type ) -> rhi::CommandListHandle override;
        MKT_NODISCARD auto CreateCommandList( const rhi::CommandListCreateDescription& desc ) -> rhi::CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const rhi::ShaderModuleCreateDescription& desc ) -> rhi::ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout( const rhi::InputLayoutCreateDescription& desc ) -> rhi::InputLayoutHandle override;

        MKT_NODISCARD auto CreateBindingLayout( const rhi::BindingLayoutDescription& desc ) -> rhi::BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const rhi::PipelineLayoutCreateDescription& desc ) -> rhi::PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingTable( const rhi::BindingTableDescription& desc, rhi::BindingLayoutHandle layout ) -> rhi::BindingTableHandle override;

        auto UpdateBindingTable( const rhi::BindingTableDescription& desc, rhi::BindingTableHandle table ) -> void override;

        MKT_NODISCARD auto CreateFence( core::u64 fenceInitialValue ) -> rhi::FenceHandle override;

        auto UnMap( rhi::IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map( rhi::IBuffer* buffer ) -> void* override;
        MKT_NODISCARD auto ReadTextureRegion(
            const rhi::ITexture* texture, const rhi::Rect& region,
            eastl::span<core::u8> destination ) -> bool override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateDescriptorTableLayout( const rhi::DescriptorTableLayoutDescription& desc ) -> rhi::BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( rhi::BindingLayoutHandle layout ) -> rhi::DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( rhi::DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool override;

        auto ReleaseDescriptorTableIndex( rhi::DescriptorTableHandle descriptorTable, core::u32 bindingIndex, rhi::DescriptorTableIndex index ) -> void override;
        MKT_NODISCARD auto WriteDescriptorTable( rhi::DescriptorTableHandle descriptorTable, const rhi::BindingTableItem& item ) -> rhi::DescriptorTableIndex override;

        MKT_NODISCARD auto CreateQueryPool( const rhi::QueryPoolDescription& description ) -> rhi::QueryPoolHandle override;
        MKT_NODISCARD auto GetQueryCapabilities( rhi::QueryType type, rhi::QueueType queue ) const -> rhi::QueryCapabilities override;

        MKT_NODISCARD auto CreateVideoDecoder( const rhi::VideoDecoderDescription& description ) -> rhi::VideoDecoderHandle override;

        MKT_NODISCARD auto GetQueue( rhi::QueueType type ) -> renderer::rhi::IQueue* override;

        MKT_NODISCARD auto CreateSwapChain( const renderer::rhi::SwapChainDescription& description ) -> rhi::SwapChainHandle override;

        MKT_NODISCARD auto SupportsRayTracing() const -> bool override;

        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;
        MKT_NODISCARD auto GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory override;

        MKT_NODISCARD auto IsFeatureSupported( rhi::GpuFeatureSupportFlags supportFlags ) const -> bool override;

        MKT_NODISCARD auto IsFeatureRequested( rhi::GpuFeatureSupportFlags requestFlags ) const -> bool override;

        MKT_NODISCARD auto IsInitialized() const -> bool override;

        MKT_NODISCARD auto GetGraphicsApi() const -> renderer::rhi::GraphicsAPI override;
        MKT_NODISCARD auto SupportsBCTextureCompression() const -> bool override;
        MKT_NODISCARD auto IsGraphicsApi( renderer::rhi::GraphicsAPI api ) const -> bool override;

        MKT_NODISCARD auto GetVendorID() const -> eastl::string_view override;
        MKT_NODISCARD auto GetDeviceName() const -> eastl::string_view override;
        MKT_NODISCARD auto GetDriverVersion() const -> eastl::string_view override;

        auto WaitIdle() -> void override;

        auto RunGarbageCollection() -> void override;

        // Vulkan specifics ================================================
        MKT_NODISCARD auto CreateTexture( const ExternalTextureDescription& info ) -> rhi::TextureHandle;
        MKT_NODISCARD auto CreateBinarySemaphore() -> BinarySemaphoreHandle;

        auto SetDebugName( VkObjectType objectType, core::u64 handle, eastl::string_view name ) -> void;

        MKT_NODISCARD auto GetDummySampler() -> Sampler*;
        MKT_NODISCARD auto GetDummyPipelineLayout() -> PipelineLayout*;
        MKT_NODISCARD auto GetLayoutForEmptySet() -> VkDescriptorSetLayout;

        MKT_NODISCARD auto GetUploadManager() -> GpuUploadManager*;
        MKT_NODISCARD auto GetDescriptorAllocator() -> DescriptorAllocatorHandle;

        MKT_NODISCARD auto GetDevice() -> VkDevice;
        MKT_NODISCARD auto SupportsOidn() const -> bool;
        MKT_NODISCARD auto GetDlssContext() -> DlssContext &;
        MKT_NODISCARD auto GetEnabledExtensions() const -> eastl::span<const eastl::string>;
        MKT_NODISCARD auto GetAllocator() -> GpuMemoryAllocator*;
        MKT_NODISCARD auto GetPhysicalDevice() const -> const PhysicalDevice*;
        MKT_NODISCARD auto SupportsSceneMultisampling( rhi::Multisampling samples ) const -> bool override;

        // Query the physical device features we have enabled ourselves
        MKT_NODISCARD auto GetActivePhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures&;
        MKT_NODISCARD auto GetActivePhysicalDeviceFeatures2() const -> const VkPhysicalDeviceFeatures2&;

        // Query the vulkan physical device features we have enabled ourselves
        MKT_NODISCARD auto GetActive11Features() const -> const VkPhysicalDeviceVulkan11Features&;
        MKT_NODISCARD auto GetActive12Features() const -> const VkPhysicalDeviceVulkan12Features&;
        MKT_NODISCARD auto GetActive13Features() const -> const VkPhysicalDeviceVulkan13Features&;

        auto CreateSwapChain( const SwapChainCreateInfo& createInfo ) -> VulkanSwapChainHandle;

        ~Device() override = default;

    private:
        // [Internal usage]
        auto InitLogicalDevice() -> void;
        auto InitLogicalQueues() -> void;
        auto InitMemoryAllocator() -> void;
        auto InitPipelineCache() -> void;
        auto InitDescriptorAllocator() -> void;

        auto InitDummyResources() -> void;
        auto DestroyDummyResources() -> void;

        auto SerializePipelineCache() -> void;

        MKT_NODISCARD auto IsDeviceSuitable( const PhysicalDevice& device ) -> bool;

    private:
        eastl::string mVendorID{};
        eastl::string mDeviceName{};
        eastl::string mDriverVersion{};

        rhi::GraphicsAPI mApi{};
        bool mIsInitialized{ false };

        // OpenGL convention
        rhi::ViewportConvention mViewportConvention{ renderer::rhi::ViewportConvention::eRightHanded_OriginBottomLeft };
        rhi::GpuDeviceType mDeviceType{ renderer::rhi::GpuDeviceType::eInvalid };

        // Requests constrain selection; support describes the selected GPU independently.
        rhi::GpuFeatureSupportFlags mRequestedFeatures{ rhi::GpuFeatureSupportFlagsBits::None };
        rhi::GpuFeatureSupportFlags mSupportedFeatures{ rhi::GpuFeatureSupportFlagsBits::None };

        // [Command list management]
        // One queue per family index
        ankerl::unordered_dense::map<rhi::QueueType, core::Ref<Queue>> mQueues{};

        // [Device management]
        VkDevice mLogicalDevice{};
        PhysicalDevice* mPhysicalDevice{};

        static constexpr core::u32 kMaxQueuesPerFamily{ 4 };
        static constexpr core::f32 kQueueDefaultPriority{ 1.0f };

        // [Memory management]
        eastl::unique_ptr<rhi::IGpuAllocator> mGpuAllocator{};
        eastl::unique_ptr<GpuUploadManager> mUploadManager{};

        // [Cache]
        VkPipelineCache mPipelineCache{};
        filesystem::Path mPipelineCachePath{};
        static constexpr eastl::string_view kPipelineCacheDirectory{ ".data/vulkan/" };

        rhi::SamplerHandle mDummySampler{};
        rhi::PipelineLayoutHandle mEmptyPipelineLayout{};
        rhi::BindingLayoutHandle mEmptyBindingLayout{};

        eastl::unique_ptr<IDescriptorAllocatorPool> mDescriptorAllocatorPool{};

        eastl::vector<eastl::string> mExtensions{};

        bool mOidnSupported{};
        bool mDlssExtensionsReady{};
        DlssContext mDlssContext{};

        // Presentation support (only if requested)
        VkSurfaceKHR mSurface{};

        VkPhysicalDeviceFeatures mEnabledFeatures{};
        VkPhysicalDeviceFeatures2 mEnabledFeatures2{};
        VkPhysicalDeviceVulkan11Features mEnabled11Features{};
        VkPhysicalDeviceVulkan12Features mEnabled12Features{};
        VkPhysicalDeviceVulkan13Features mEnabled13Features{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_DEVICE_HH
