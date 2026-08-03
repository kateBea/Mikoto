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

#ifndef MIKOTO_VULKAN_DEVICE_HH
#define MIKOTO_VULKAN_DEVICE_HH

#include <mutex>

#include <EASTL/deque.h>
#include <EASTL/functional.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <EASTL/optional.h>

#include <volk.h>

#include <tracy/TracyVulkan.hpp>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

#include <Memory/Allocator.hh>
#include <Memory/MemoryArena.hh>
#include <Memory/LinearAllocator.hh>
#include <Memory/FreeListAllocator.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderContext.hh>

#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanShader.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanInstance.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanSwapchain.hh>
#include <Renderer/Vulkan/VulkanFramebuffer.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::memory;

    // TODO: Turn into Device object, not sure if I should expose these to RHI that need a more solid RHI design, not for now
    // For the time thi will be specific to vulkan
    struct FencePlain {
        VkFence mFence{ VK_NULL_HANDLE };

        auto Create( const VkFenceCreateInfo& info, VkDevice device ) -> void;
        auto Destroy( VkDevice device ) -> void;

        operator VkFence() const noexcept;
    };

    class Fence final : public IFence {
    public:
        explicit Fence(u64 initialValue);

        MKT_NODISCARD auto GetCompletionValue() const -> u64 override;

        auto SetDebugName( eastl::string_view name) -> void override;

        MKT_NODISCARD auto GetNativeHandle(ObjectType) -> Object override;
        MKT_NODISCARD auto GetNativeHandle(ObjectType) const -> Object override;

        // Increments by incVal and returns the current value
        auto GetCurrentID() const -> u64;
        auto GetAndIncrement( u64 value ) -> u64;

        ~Fence() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;
    private:
        eastl::atomic<u64> mTimeline{ 0 };
        VkSemaphore mSemaphore{ VK_NULL_HANDLE };
    };

    class TimelineSemaphore final : public ISemaphore {
    public:
        explicit TimelineSemaphore(u64 initialValue);

        auto SetDebugName( eastl::string_view name) -> void override;

        MKT_NODISCARD auto GetNativeHandle(ObjectType) -> Object override;
        MKT_NODISCARD auto GetNativeHandle(ObjectType) const -> Object override;

        // Increments by incVal and returns the current value
        auto GetCurrentID() -> u64;
        auto GetAndIncrement( u64 value ) -> u64;

        ~TimelineSemaphore() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;
    private:
        eastl::atomic<u64> mTimeline{ 0 };
        VkSemaphore mSemaphore{ VK_NULL_HANDLE };
    };

    class BinarySemaphore final : public ISemaphore {
    public:
        explicit BinarySemaphore();

        auto SetDebugName( eastl::string_view name) -> void override;

        MKT_NODISCARD auto GetNativeHandle(ObjectType type) -> Object override;
        MKT_NODISCARD auto GetNativeHandle(ObjectType type) const -> Object override;

        ~BinarySemaphore() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkSemaphore mSemaphore{ VK_NULL_HANDLE };
    };

    struct DeletionQueue final {
        using DeletionFunc = eastl::function<void( IGpuDevice* )>;
        using DeletionQueueList = eastl::deque<DeletionFunc>;

        IGpuDevice* mDevice{};
        RenderContext* mContext{};

        std::mutex mPushMutex{};
        DeletionQueueList mCallbacks{};

        explicit DeletionQueue( IGpuDevice* device );

        auto Flush() -> void;
        auto Push( eastl::function<void( IGpuDevice* )>&& callback ) -> void;

        auto Shutdown() -> void;
    };

    struct GpuUploadAllocation {
        IBuffer* mBuffer{};

        // This is the mapped address we should be writing to,
        // it corresponds to the beginning of the slice or
        // range that is available within this allocation.
        // You do not need to offset or anything
        void* mMappedMemory{};

        // Size of this sub-allocation
        size_t mSize{};

        // Specifies the offset of this allocation within the large
        // buffer it was allocated from
        size_t mOffset{};

        // Metadata to track usage
        Allocation mAllocation{};
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
        explicit GpuUploadManager( IGpuDevice* device );

        auto SubAllocate( size_t byteSize ) -> GpuUploadAllocation*;
        auto ReclaimMemory() -> void;

        ~GpuUploadManager();
    private:
        struct StagingAllocation {
            BufferHandle mBuffer{};
            eastl::unique_ptr<memory::MemoryArena<IBuffer, memory::FreeListFirstFitAllocator>> mMemoryArena{};
        };

        static constexpr u32 kMaxBuffers{ 32 };
        static constexpr u32 kMaxSubAllocations{ 100 };

        auto CreateBuffer() -> StagingAllocation*;
        auto CreateSubAllocation( IBuffer* buffer ) -> GpuUploadAllocation*;

    private:
        std::mutex mMutex{};

        IGpuDevice* mDevice{};
        ankerl::unordered_dense::map<IBuffer*, eastl::unique_ptr<StagingAllocation>> mBuffers{};
        ankerl::unordered_dense::map<IBuffer*, eastl::fixed_vector<eastl::unique_ptr<GpuUploadAllocation>, kMaxSubAllocations>> mSubAllocations{};
    };

    class CommandPool;

    struct CommandThreadContext {
        u64 mSubmissionId{};
        IBuffer* mIndirectBuffer{};
        bool mIsRenderScopeActive{};

        CommandPool* mPool{};
        VkCommandBuffer mCommandBuffer{};
    };

    // https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/performance/command_buffer_usage
    // Keeps tract of bound resources
    // for pipelines that match the same layout could be useful to avoid rebinding same resources twice for compatible pipeline
    // Recycles submitted command buffers when they are done
    // If it does not have available command buffers to record it allocates a new one
    // to avoid using the one the GPU is consuming
    class CommandList final : public ICommandList {
    public:
        explicit CommandList( const VkCommandBufferAllocateInfo& createInfo, QueueType type, CommandPool* pool );
        explicit CommandList( const VkCommandBufferAllocateInfo& createInfo, const CommandListParameters& desc, CommandPool* pool );

        auto Begin( const CommandListBeginDescription& desc ) -> void override;
        auto End() -> void override;

        auto BeginParallel() -> void override;
        auto EndParallel() -> void override;

        auto SetDebugName( eastl::string_view name) -> void override;

        // More relaxed versions of SetResourceState
        auto PushBarrier( const BufferBarrierDescription& barrier ) -> void override;
        auto PushBarrier( const TextureBarrierDescription& barrier ) -> void override;

        auto BeginTrackingState(IBuffer* buffer, ResourceStates stateBits) -> void override;
        auto BeginTrackingState(ITexture* buffer, ResourceStates stateBits) -> void override;

        auto SetResourceState(IBuffer* buffer, ResourceStates stateBits) -> void override;
        auto SetResourceState(ITexture* buffer, ResourceStates stateBits) -> void override;

        auto SetBarrier( const BufferBarrierDescription& barrier ) -> void override;
        auto SetBarrier( const TextureBarrierDescription& barrier ) -> void override;

        auto CommitBarriers() -> void override;

        auto SetEnableAutomaticBarriers(  bool enable  ) -> void override;

        auto SetClearColor( FramebufferHandle frameBuffer, Color color ) -> void override;
        auto SetClearColor( TextureHandle image, Color color ) -> void override;

        auto Write( IBuffer* src, ITexture* dest, u32 mipLevel ) -> void override;
        auto Write( ITexture* texture, u32 mipLevel,const void* data, size_t byteSize ) -> void override;
        auto Copy( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void override;

        auto WriteVolatile( IBuffer* target, size_t dstOffset, const void* data, size_t byteSize ) -> void override;

        auto Write( IBuffer* buffer, size_t destOffset, const void* data, size_t byteSize ) -> void override;
        auto Write( IBuffer* buffer, const void* data, size_t byteSize ) -> void override;
        auto Copy( IBuffer* src, IBuffer* dest ) -> void override;
        auto Copy( IBuffer* src, IBuffer* dest, size_t destOffset ) -> void override;

        auto Copy( IBuffer *dest, ITexture *src ) -> void override;

        auto BeginRendering( GraphicsState& state ) -> void override;
        auto EndRendering() -> void override;

        auto BindPipeline( IPipeline* pipeline ) -> void override;

        // I am not sure if I wanna have these because the viewport is set on the images we
        // render to so it makes more sense to tie them to the graphics state when we specify the render targets
        auto SetViewport( eastl::span<const Viewport> viewports ) -> void override;
        auto SetScissors( eastl::span<const Rect> scissorRects ) -> void override;
        auto SetViewportState( const ViewportState& vs ) -> void override;

        auto BindIndexBuffer( IBuffer* buffer ) -> void override;
        auto BindIndirectBuffer( IBuffer* buffer ) -> void override;
        auto BindVertexBuffer( const VertexBufferBinding& binding ) -> void override;
        auto BindVertexBuffer( eastl::span<const VertexBufferBinding> binding ) -> void override;

        auto BindPipelineResources( const BindResourcesDescription& desc ) -> void override;

        auto Draw( const DrawArguments& args ) -> void override;
        auto DrawIndexed( const DrawArguments& args ) -> void override;

        auto DrawIndirect( u32 offset, u32 drawCount ) -> void override;
        auto DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void override;

        auto Dispatch( u32 groupsX, u32 groupsY, u32 groupsZ ) -> void override;

        auto SetPushConstants( IPipelineLayout* pipelineLayout, const void* data, size_t byteSize, ShaderStage visibility ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto GetPool() const -> const CommandPool*;

        ~CommandList() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto InitializeArenaAllocators() -> void;
        auto GetThreadContext() -> CommandThreadContext*;

        auto ClearState() -> void;
        auto TryRecycle(IQueue* queue) -> void;

    private:
        CommandPool* mCommandPool{ nullptr };
        VkCommandBufferAllocateInfo mAllocInfo{};

        GpuUploadManager* mUploadManager{ nullptr };

        bool mEnableAutomaticBarriers{ true };

        // TODO(kate): It would be interesting to expose pool management
        // through the RHI or have it be handled automatically just like barriers
        bool mEnableCommandPoolManagement{ true };

        eastl::fixed_vector<VkBufferMemoryBarrier2, kMaxBarriers> mBufferBarriers{};
        eastl::fixed_vector<VkImageMemoryBarrier2, kMaxBarriers> mImageBarriers{};

        // We will be using a bump allocator for data that needs to copied to GPU per frame (bigger than 64KB)
        // And reset it everytime we call End(). The arena remains initialized until it is actually needed.
        // The reason why this belongs to the Command is that it makes it easier to reset the allocator
        // everytime we need to start consuming it again.
        static constexpr size_t kMaxVolatileBufferVersions{ 4 };
        static constexpr size_t kArenaInitialSize{ MKT_MEGABYTES( 64 ) };

        u32 mCurrentVolatileVersion{ 0 };
        ankerl::unordered_dense::map<u32, eastl::unique_ptr<memory::MemoryArena<IBuffer, LinearAllocator>>> mArenasAllocators{};

        std::mutex mUploadAllocationsMutex{};
        eastl::fixed_vector<GpuUploadAllocation*, 10> mUploadAllocations{};

        // Multithread
        std::thread::id mHostThread{};
        u32 mMaxThreadConcurrency{ 0 };
        static constexpr size_t kMaxCmdConcurrency{ 16 };

        // It holds the main command buffer that can be submitted
        // after recording is done (once we call End())
        VkCommandBuffer mPrimaryCommandBuffer{};
        ankerl::unordered_dense::map<std::thread::id, eastl::unique_ptr<CommandThreadContext>> mThreadContexts{};

        // For debug
        Color mLabelColor{};
    };

    // The thread pool is not safe and may only be accessed by one thread at a time
    class CommandPool final : public DeviceObject {
    public:
        explicit CommandPool( QueueType type, u32 queueFamilyIndex );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        auto SetDebugName( eastl::string_view name) -> void override;

        auto AllocateCmdList() -> CommandListHandle;
        auto AllocateCmdList( const CommandListParameters& params ) -> CommandListHandle;

        ~CommandPool() override;

        using DeviceObject::Initialize;

    public:
        DISABLE_COPY_AND_MOVE_FOR( CommandPool );

    private:
        static constexpr u32 kMaximumCmdBuffersPerFrame{ 15 };

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkCommandPool mPool{};

        u32 mQueueFamilyIndex{};
        QueueType mQueueType{ QueueType::eInvalid };
    };

    using CommandPoolHandle = Ref<CommandPool>;

    // Internally multiple queues might map to the exact same
    // VkQueue from the same family index
    class Queue final : public IQueue {
    public:
        explicit Queue(QueueType type, QueueOpSupportFlags opFlags, u32 queueFamilyIndex, u32 queueIndex = 0);

        auto Wait( IFence* fence, u64 value ) -> void override;
        auto Signal( IFence* fence, u64 value ) -> void override;

        auto ExecuteCommandLists( eastl::span<CommandListHandle> commands ) -> void override;

        auto Flush() -> void;

        auto RunGarbageCollection() -> void;

        auto ExecuteCommandList( CommandListHandle cmd ) -> void;
        auto SubmitCommandList( CommandListHandle cmd ) -> u64;

        auto AllocateCmdList() -> CommandListHandle;
        auto AllocateCmdList( const CommandListParameters& params ) -> CommandListHandle;

        auto AllocateSecondaryCmdList() -> eastl::pair<VkCommandBuffer, CommandPool*>;

        auto PushDelete( VkCommandBuffer cmd, VkCommandPool pool, u64 submitID ) -> void;
        auto PushDelete( VkCommandBuffer cmd, VkCommandPool pool, const FencePlain& fence ) -> void;

        auto AddQueueWaitFence( FencePlain* semaphore ) -> void;

        auto AddQueueSignalSemaphore( Fence* semaphore, u64 value, VkPipelineStageFlags2 stageFlags ) -> void;
        auto AddQueueSignalSemaphore( BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void;
        auto AddQueueWaitSemaphore( BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void;

        auto WaitIdle() const -> void;

        MKT_NODISCARD auto Present( const VkPresentInfoKHR& info ) -> VkResult;

        MKT_NODISCARD auto GetCompletedValue() const -> u64;

        MKT_NODISCARD auto GetQueue() const -> VkQueue;
        MKT_NODISCARD auto GetQueueIndex() const -> u32;
        MKT_NODISCARD auto GetFamilyIndex() const -> u32;

        // Conversion operators
        operator u32() const; // Queue family index
        operator VkQueue() const; // Logical

        ~Queue() override;

    private:
        MKT_NODISCARD auto SubmitCommands() -> u64;
        MKT_NODISCARD auto AcquireThreadCmdPool() -> CommandPoolHandle;

    protected:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        std::mutex mSubmissionMutex{};

        u32 mFamilyIndex{ 0 };
        u32 mQueueIndex{ 0 };

        float mPriority{ 1.0f };
        VkQueue mQueue{ VK_NULL_HANDLE };

        eastl::fixed_vector<VkSemaphoreSubmitInfo, 10> mWaitInfos{};
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 10> mSignalInfos{};

        static constexpr u32 kMaxSubmits{ 115 };
        std::mutex mPendingSubmitMutex{};
        SemaphoreHandle mTimelineSemaphore{};
        eastl::fixed_vector<VkCommandBuffer, kMaxSubmits> mPendingSubmits{};

        struct DeleteItem {
            FencePlain mFence{};
            u64 mSubmissionID{};
            VkCommandPool mPool{};
            VkCommandBuffer mBuffer{};
        };
        std::mutex mDeleteCmdsMutex{};
        eastl::fixed_vector<DeleteItem, 100> mDeleteCmds{};

        std::mutex mPoolsMutex{};
        ankerl::unordered_dense::map<std::thread::id, CommandPoolHandle> mPools{};

        // For debug
        eastl::string mSubmissionLabel{};
        Color mSubmissionLabelColor{};
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
        i8 mPoolIndex{};
    };

    class IDescriptorAllocatorPool {
    public:
        virtual ~IDescriptorAllocatorPool() = default;

        MKT_NODISCARD static auto Create( const VkDevice& device, i32 nFrames = 3 ) -> eastl::unique_ptr<IDescriptorAllocatorPool>;

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
        auto WithBinding( u32 binding, VkDescriptorType type, i32 arraySize, VkShaderStageFlags shaderStages ) -> DescriptorLayoutBuilder&;
    };

    struct DescriptorWriter final {
        // std::deque is guaranteed to keep references to elements valid
        eastl::vector<VkWriteDescriptorSet> mWrites{};
        eastl::deque<VkDescriptorImageInfo> mImageInfos{};
        eastl::deque<VkDescriptorBufferInfo> mBufferInfos{};

        VkShaderStageFlags mShaderStages{};

        auto WriteSampler( u32 binding, VkSampler sampler, u32 arrayIndex = 0 ) -> DescriptorWriter&;
        auto WriteBuffer( u32 binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type, u32 arrayIndex = 0 ) -> DescriptorWriter&;
        auto WriteImage( u32 binding, VkImageView image, VkDescriptorType type, VkImageLayout layout, u32 arrayIndex = 0 ) -> DescriptorWriter&;

        auto SetVisibility( VkShaderStageFlags visibility ) -> DescriptorWriter&;

        auto Clear() -> void;
        auto UpdateSet( VkDevice device, VkDescriptorSet set ) -> void;
    };

    class BindingLayout final : public IBindingLayout {
    public:

        explicit BindingLayout( const BindingLayoutDescription& desc );
        explicit BindingLayout( const BindlessLayoutDescription& desc );

        MKT_NODISCARD auto GetRegisterSpace() const -> u32 override;

        MKT_NODISCARD auto IsBindless() const -> bool override;

        MKT_NODISCARD auto GetBindlessLayoutDesc() const -> const BindlessLayoutDescription&;

        auto SetDebugName( eastl::string_view name ) -> void override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        u32 mSetIndex{};
        bool mIsBindless{ false };
        BindingLayoutDescription mBindingLayoutDesc{};
        BindlessLayoutDescription mBindlessLayoutDesc{};

        VkDescriptorSetLayout mDescriptorSetLayout{};
        VkDescriptorSetLayoutCreateInfo mCreateInfo{};
    };

    class DescriptorTable : public IDescriptorTable {
    public:
        explicit DescriptorTable(BindingLayoutHandle setLayout);

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetCapacity( u32 slot ) const -> u32 override;

        MKT_NODISCARD auto GetResourceSlot( ResourceType type ) const -> i32;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~DescriptorTable() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkDescriptorSet mDescriptorSet{};
        VkDescriptorSetAllocateInfo mCreateInfo{};

        DescriptorAllocatorHandle mDescriptorAllocatorHandle{};

        // Slot -> Array index
        eastl::fixed_hash_map<ResourceType, i32, kMaxSlotsPerTable> mSlotResourceType{};

        BindingLayoutHandle mBindingLayout{};
    };

    class BindingSet : public IBindingSet {
    public:

        explicit BindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout );

        auto SetDebugName( eastl::string_view name ) -> void override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~BindingSet() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkDescriptorSet mDescriptorSet{};
        VkDescriptorSetAllocateInfo mCreateInfo{};

        DescriptorAllocatorHandle mDescriptorAllocatorHandle{};

        BindingLayoutHandle mBindingLayout{};
        BindingSetDescription mBindingDescription{};
    };

    class InputLayout : public IInputLayout {
    public:
        explicit InputLayout( const InputLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetVertexBindingDesc() const -> const eastl::fixed_vector<VkVertexInputBindingDescription, kMaxVertexBindings>&;
        MKT_NODISCARD auto GetVertexAttributesDesc() const -> const eastl::fixed_vector<VkVertexInputAttributeDescription, kMaxVertexAttributes>&;

        MKT_NODISCARD auto GetNumAttributes() const -> u32 override;
        MKT_NODISCARD auto GetAttributeDescription(u32 index) const -> const VertexAttributeDescription& override;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        InputLayoutCreateDescription mDesc{};
        eastl::fixed_vector<VkVertexInputBindingDescription, kMaxVertexBindings> mVertexBindingDescriptions{};
        eastl::fixed_vector<VkVertexInputAttributeDescription, kMaxVertexAttributes> mVertexAttributeDescriptions{};
    };

    class PipelineLayout : public IPipelineLayout {
    public:
        explicit PipelineLayout( const PipelineLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~PipelineLayout() override;
    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkPipelineLayout mPipelineLayout{};

        PipelineLayoutCreateDescription mDesc{};
    };

    class Device final : public IGpuDevice {
    public:
        explicit Device( const GpuDeviceCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const TextureCreateDescription& description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription& description ) -> TextureHandle override;

        MKT_NODISCARD auto CreateFrameBuffer(const FramebufferDescription& description) -> FramebufferHandle override;

        MKT_NODISCARD auto CreateBuffer( const BufferCreateDescription& description ) -> BufferHandle override;

        MKT_NODISCARD auto CreateSampler( const SamplerCreateDescription& description ) -> SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle override;

        MKT_NODISCARD auto CreateAccelStructure( const AccelStructureCreateDescription& description ) -> AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( QueueType queue ) -> CommandListHandle override;
        MKT_NODISCARD auto CreateCommandList( const CommandListParameters& parameters ) -> CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const ShaderModuleCreateDescription& desc ) -> ShaderModuleHandle override;
        MKT_NODISCARD auto CreateShader( ShaderStage type, const void* code, size_t codeSize  ) -> ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout(const InputLayoutCreateDescription& desc) -> InputLayoutHandle override;

        MKT_NODISCARD auto CreateBindingLayout( const BindingLayoutDescription& desc ) -> BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout ) -> BindingSetHandle override;

        MKT_NODISCARD auto CreateFence( u64 fenceInitialValue ) -> FenceHandle override;

        auto UnMap( IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map(IBuffer* buffer) -> const void* override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateBindlessLayout( const BindlessLayoutDescription& desc ) -> BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem& item ) -> bool override;

        auto WaitIdle() -> void override;

        auto Wait( QueueType type, FenceHandle handle, u64 fenceValue ) -> void override;
        auto Signal( QueueType type, FenceHandle handle, u64 fenceValue ) -> void override;

        auto RunGarbageCollection() -> void override;

        auto SubmitCommands( CommandListHandle cmdList ) -> u64 override;

        auto ExecutePendingCommands() -> void override;
        auto ExecuteCommands( CommandListHandle cmd ) -> void override;
        auto ExecuteCommands( eastl::span<CommandListHandle> cmdList ) -> void override;

        // Vulkan specifics ================================================
        MKT_NODISCARD auto CreateTexture( const ExternalTextureDescription& info ) -> TextureHandle;
        MKT_NODISCARD auto CreateTimelineSemaphore( u64 initialValue ) -> SemaphoreHandle;
        MKT_NODISCARD auto CreateBinarySemaphore() -> SemaphoreHandle;

        auto AddQueueWaitFence( QueueType queueType, FencePlain* fence ) -> void;
        auto AddQueueSignalSemaphore( QueueType queueType, BinarySemaphore* , VkPipelineStageFlags2 stageFlags ) -> void;
        auto AddQueueWaitSemaphore( QueueType queueType, BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void;

        auto SetDebugName( VkObjectType objectType, u64 handle, eastl::string_view name ) -> void;

        auto WaitQueuesIdle() const -> void;

        auto SubmitDeletion( eastl::function<void( IGpuDevice* )>&& callback ) -> void;

        MKT_NODISCARD auto GetDummySampler() -> Sampler*;
        MKT_NODISCARD auto GetLayoutForEmptySet() -> VkDescriptorSetLayout;

        MKT_NODISCARD auto GetUploadManager() -> GpuUploadManager*;
        MKT_NODISCARD auto GetDescriptorAllocator() -> DescriptorAllocatorHandle;

        MKT_NODISCARD auto GetDevice() -> VkDevice;
        MKT_NODISCARD auto GetPhysicalDevice() -> PhysicalDevice*;
        MKT_NODISCARD auto GetAllocator() -> GpuMemoryAllocator*;

        MKT_NODISCARD auto GetQueue( QueueType type ) -> Queue*;
        MKT_NODISCARD auto GetQueue( QueueType type ) const -> const Queue*;

        MKT_NODISCARD auto GetActivePhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures&;
        MKT_NODISCARD auto GetActivePhysicalDeviceFeatures2() const -> const VkPhysicalDeviceFeatures2&;

        MKT_NODISCARD auto GetActive11Features() const -> const VkPhysicalDeviceVulkan11Features&;
        MKT_NODISCARD auto GetActive12Features() const -> const VkPhysicalDeviceVulkan12Features&;
        MKT_NODISCARD auto GetActive13Features() const -> const VkPhysicalDeviceVulkan13Features&;

        auto CreateSwapChain( const SwapChainCreateInfo& createInfo ) -> SwapChainHandle;

        ~Device() override = default;

    private:
        // [Internal usage]
        auto InitLogicalDevice() -> void;
        auto InitLogicalQueues() -> void;
        auto InitMemoryAllocator() -> void;
        auto InitPipelineCache() -> void;
        auto InitDescriptorAllocator() -> void;

        auto InitTracyContext() -> void;
        auto ShutdownTracyContext() -> void;

        auto InitDummyResources() -> void;
        auto DestroyDummyResources() -> void;

        auto SerializePipelineCache() -> void;

        MKT_NODISCARD auto IsDeviceSuitable( const PhysicalDevice& device) -> bool;

    private:
#if defined( MKT_USE_VULKAN_BINDLESS )
        const bool mIsBindlessEnabled{ true };
#else
        const bool mIsBindlessEnabled{ false };
#endif

        // [Command list management]
        // One queue per family index
        ankerl::unordered_dense::map<QueueType, Ref<Queue>> mQueues{};

        // [Device management]
        VkDevice mLogicalDevice{};
        PhysicalDevice* mPhysicalDevice{};

        static constexpr u32 kMaxQueuesPerFamily{ 4 };
        static constexpr f32 kQueueDefaultPriority{ 1.0f };

        // [Memory management]
        eastl::unique_ptr<IGpuAllocator> mGpuAllocator{};
        eastl::unique_ptr<GpuUploadManager> mUploadManager{};

        // [Cache]
        VkPipelineCache mPipelineCache{};
        Path mPipelineCachePath{};
        static constexpr eastl::string_view kPipelineCacheDirectory{ "Assets/.vulkan/" };

        SamplerHandle mDummySampler{};
        BindingLayoutHandle mEmptyBindingLayout{};

        eastl::unique_ptr<IDescriptorAllocatorPool> mDescriptorAllocatorPool{};

        eastl::vector<eastl::string> mExtensions{};

        // Presentation support (only if requested)
        VkSurfaceKHR mSurface{};

        VkPhysicalDeviceFeatures mEnabledFeatures{};
        VkPhysicalDeviceFeatures2 mEnabledFeatures2{};
        VkPhysicalDeviceVulkan11Features mEnabled11Features{};
        VkPhysicalDeviceVulkan12Features mEnabled12Features{};
        VkPhysicalDeviceVulkan13Features mEnabled13Features{};

        // [Tracy debug]
        VkCommandPool mTracyPool{};
        VkCommandBuffer mTracyCmd{};
        TracyVkCtx mTracyContext{};
    };
}// namespace mikoto::renderer::vulkan

#endif//MIKOTO_VULKAN_DEVICE_HH
