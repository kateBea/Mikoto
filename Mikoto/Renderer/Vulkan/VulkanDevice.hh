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

    // TODO: Turn into Device object, not sure if I should expose these to RHI that necessites a more solid RHI design, not for now
    // For the time thi will be specific to vulkan
    struct Fence {
        VkFence mFence{ VK_NULL_HANDLE };

        auto Create( const VkFenceCreateInfo& info, VkDevice device ) -> void;
        auto Destroy( VkDevice device ) -> void;

        operator VkFence() const noexcept;
    };

    // We use this to provide additional semaphores to
    // the queues in order to specify external semaphores they need to
    // sync with
    struct QueueSubmitSync {
        VkSemaphore mWaitBinarySemaphore{};
        VkPipelineStageFlags2 mWaitBinaryStage{};

        VkSemaphore mSignalBinarySemaphore{};
        VkPipelineStageFlags2 mSignalBinaryStage{};
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
        using DeletionFunc = eastl::function<void( GpuDevice* )>;
        using DeletionQueueList = eastl::deque<DeletionFunc>;

        GpuDevice* mDevice{};
        RenderContext* mContext{};

        std::mutex mPushMutex{};
        DeletionQueueList mCallbacks{};

        explicit DeletionQueue( GpuDevice* device );

        auto Flush() -> void;
        auto Push( eastl::function<void( GpuDevice* )>&& callback ) -> void;

        auto Shutdown() -> void;
    };

    struct GpuUploadAllocation {
        IBuffer* mBuffer{};
        void* mMappedMemory{}; // This is the mapped address we should be writing to, it corresponds to the slice or range that is available within this buffer allocation
        size_t mSize{};
        size_t mOffset{};

        eastl::atomic_flag mInUse{ true };

        Allocation mAllocation{};
    };

    // Manages intermediate buffers that are used to upload data from CPU
    // to CPU, whenever we want to upload data from the CPU to the GPU
    // if the GPU buffer is not writeable from CPU we copy to this intermediate buffer
    // and issue a copy command. In order to implement this we can use a first fit or best fit
    // allocator approach, see the GeometryAllocator in the MeshCulling.hh file
    // we can just have a huge chunk that grows as we need??
    class GpuUploadManager final {
    public:
        explicit GpuUploadManager( GpuDevice* device );

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

        GpuDevice* mDevice{};
        ankerl::unordered_dense::map<IBuffer*, eastl::unique_ptr<StagingAllocation>> mBuffers{};
        ankerl::unordered_dense::map<IBuffer*, eastl::vector<eastl::unique_ptr<GpuUploadAllocation>>> mSubAllocations{};
    };

    // Keeps tract of bound resources
    // for pipelines that match the same layout could be useful to avoid rebinding same resources twice for compatible pipeline
    class CommandList final : public ICommandList {
    public:
        explicit CommandList( const VkCommandBufferAllocateInfo& createInfo, QueueType type );

        auto Begin() -> void override;
        auto End() -> void override;

        auto SetDebugName( eastl::string_view name) -> void override;

        auto BeginTrackingState( IBuffer* buffer, ResourceStates newState ) -> void override;
        auto BeginTrackingState( ITexture* texture, ResourceStates newState ) -> void override;
        auto SetResourceState( IBuffer* buffer, ResourceStates newState ) -> void override;
        auto SetResourceState( ITexture* texture, ResourceStates newState ) -> void override;

        auto CommitBarriers() -> void override;
        auto SetEnableAutomaticBarriers(  bool enable  ) -> void override;

        auto SetClearColor( FramebufferHandle frameBuffer, Color color ) -> void override;
        auto SetClearColor( TextureHandle renderTargets, Color color ) -> void override;

        auto WriteTexture( IBuffer* src, ITexture* dest, u32 mipLevel ) -> void override;
        auto WriteTexture( ITexture* texture, u32 mipLevel,const void* data, size_t byteSize ) -> void override;
        auto CopyTexture( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void override;

        auto WriteBuffer( IBuffer* buffer, const void* data, size_t byteSize ) -> void override;
        auto CopyBuffer( IBuffer* src, IBuffer* dest ) -> void override;
        auto CopyBuffer( IBuffer* src, IBuffer* dest, size_t destOffset ) -> void override;

        auto BeginRendering( GraphicsState& state ) -> void override;
        auto EndRendering() -> void override;

        auto BindPipeline( IPipeline* pipeline ) -> void override;

        // I am not sure if I wanna have these because the viewport is set on the images we
        // render to so it makes more sense to tie them to the graphics state when we specify the render targets
        auto SetViewport( eastl::span<const Viewport> viewports ) -> void override;
        auto SetScissors( eastl::span<const Rect> scissorRects ) -> void override;
        auto SetViewportState( const ViewportState& vs ) -> void override;

        auto BindIndexBuffer( IBuffer* buffer ) -> void override;
        auto BindVertexBuffer( const VertexBufferBinding& binding ) -> void override;

        auto BindIndirectBuffer( IBuffer* buffer, u32 stride ) -> void override;

        auto BindPipelineResources( IPipelineLayout* pipelineLayout, IBindingSet* resourceSet, u32 bindingSlot ) -> void override;

        auto Draw( const DrawArguments& args ) -> void override;
        auto DrawIndexed( const DrawArguments& args ) -> void override;

        auto DrawIndirect( u32 offset, u32 drawCount ) -> void override;
        auto DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void override;

        auto Dispatch( u32 groupsX, u32 groupsY, u32 groupsZ ) -> void override;

        auto SetPushConstants( const void* data, size_t byteSize, ShaderStage visibility ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        // Vulkan specifics
        auto TransitionLayout( ITexture* texture, VkImageLayout newLayout ) -> void;
        auto MarkSubmitted( u64 lastSubmissionID ) -> void;

        ~CommandList() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto InitializeArena() -> void;

        auto ClearState() -> void;

        auto TryRecycle(IQueue* queue) -> void;

    private:
        struct SubmissionItem {
            u64 mId{};
            VkCommandBuffer mCommandBuffer{};
        };

    private:
        VkCommandBuffer mCmdBuffer{ VK_NULL_HANDLE };
        VkCommandBufferAllocateInfo mAllocInfo{};

        bool mEnableAutomaticBarriers{ true };
        bool mRenderPassIsActive{ false };

        bool mIsSubmitted{ false };

        // State
        GraphicsState mCurrentGraphicsState{};

        GpuUploadManager* mUploadManager{ nullptr };

        // Barriers to be submitted
        eastl::fixed_vector<VkBufferMemoryBarrier2, kMaxBarriers> mBufferBarriers{};
        eastl::fixed_vector<VkImageMemoryBarrier2, kMaxBarriers> mImageBarriers{};

        // We will be using a bump allocator for data that needs to copied to GPU per frame (bigger than 64KB)
        // And reset it everytime we call End(). The arena remains initialized until it is actually needed.
        // The reason why this belongs to the Command is that it makes it easier to reset the allocator
        // everytime we need to start consuming it again.
        static constexpr size_t kArenaInitialSize{ MKT_MEGABYTES( 10 ) };
        eastl::unique_ptr<memory::MemoryArena<IBuffer, LinearAllocator>> mMemoryArena{};

        eastl::vector<GpuUploadAllocation*> mUploadAllocations{};

        u64 mLastSubmissionID{ 0 };
        eastl::fixed_vector<SubmissionItem, 10> mSubmissionItems{};
    };

    // The thread pool is not safe and may only be accessed by one thread at a time
    class CommandPool final : public DeviceObject {
    public:
        explicit CommandPool( QueueType type, u32 queueFamilyIndex );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        auto AllocateCmdList() -> CommandListHandle;

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

    class Queue final : public IQueue {
    public:
        explicit Queue(GpuDevice* device, QueueType type,  u32 queueFamilyIndex, u32 queueIndex = 0);

        auto Initialize() -> void;
        auto Shutdown() -> void;

        auto Flush() -> void;

        auto ExecuteCommandList( CommandListHandle cmd ) -> u64;
        auto SubmitCommandList( CommandListHandle cmd ) -> u64;

        auto AllocateCmdList() -> CommandListHandle;

        MKT_NODISCARD auto GetCompletedValue() const -> u64;

        auto PushDelete( VkCommandBuffer cmd, VkCommandPool pool, u64 submitID ) -> void;

        auto WaitForSubmission( u64 submissionID ) -> void;

        auto AddQueueWaitFence( Fence* semaphore ) -> void;
        auto AddQueueSignalSemaphore( BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void;
        auto AddQueueWaitSemaphore( BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void;

        auto WaitIdle() const -> void;

        MKT_NODISCARD auto GetQueue() const -> VkQueue;
        MKT_NODISCARD auto GetQueueIndex() const -> u32;
        MKT_NODISCARD auto GetFamilyIndex() const -> u32;

        // Conversion operators
        operator u32() const; // Queue family index
        operator VkQueue() const; // Logical

        auto AcquireThreadCmdPool() -> CommandPoolHandle;

        MKT_NODISCARD auto SubmitCommands( eastl::span<CommandListHandle> cmds ) -> u64;

    private:
        GpuDevice *mDevice{};

        std::mutex mSubmissionMutex{};

        u32 mFamilyIndex{ 0 };
        u32 mQueueIndex{ 0 };

        float mPriority{ 1.0f };
        VkQueue mQueue{ VK_NULL_HANDLE };

        eastl::fixed_vector<VkSemaphoreSubmitInfo, 10> mWaitInfos{};
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 10> mSignalInfos{};

        std::mutex mPendingSubmitMutex{};
        SemaphoreHandle mTimelineSemaphore{};
        eastl::fixed_vector<CommandListHandle, 100> mPendingSubmits{};

        struct DeleteItem {
            u64 mSubmissionID{};
            VkCommandPool mPool{};
            VkCommandBuffer mBuffer{};
        };
        std::mutex mDeleteCmdsMutex{};
        eastl::fixed_vector<DeleteItem, 100> mDeleteCmds{};

        std::mutex mPoolsMutex{};
        ankerl::unordered_dense::map<std::thread::id, CommandPoolHandle> mPools{};
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
        //returns true if allocation succeeded, and false if it didnt
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

        auto WriteSampler( u32 binding, VkSampler sampler ) -> DescriptorWriter&;
        auto WriteBuffer( u32 binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type ) -> DescriptorWriter&;
        auto WriteImage( u32 binding, VkImageView image, VkDescriptorType type, VkImageLayout layout ) -> DescriptorWriter&;

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

    class DescriptorTable : public IDescriptorTable {
    public:
        MKT_NODISCARD auto GetCapacity() const -> u32 override;
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

        MKT_NODISCARD auto GetBindPoint() const -> PipelineType override;

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

    class Device final : public GpuDevice {
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

        auto UnMap( IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map(IBuffer* buffer) -> const void* override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateBindlessLayout( const BindlessLayoutDescription& desc ) -> BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem& item ) -> bool override;

        auto Flush() -> void override;
        auto RunGarbageCollection() -> void override;
        auto SubmitCommands( CommandListHandle cmdList ) -> u64 override;
        auto ExecuteCommands( CommandListHandle cmd ) -> u64 override;

        auto WaitIdle() -> void override;

        // Vulkan specifics ================================================
        MKT_NODISCARD auto CreateTexture( const ExternalTextureDescription& info ) -> TextureHandle;
        MKT_NODISCARD auto CreateTimelineSemaphore( u64 initialValue ) -> SemaphoreHandle;
        MKT_NODISCARD auto CreateBinarySemaphore() -> SemaphoreHandle;

        auto WaitForSubmission( QueueType queueType, u64 submissionID ) -> void;

        auto AddQueueWaitFence( QueueType queueType, Fence* fence ) -> void;
        auto AddQueueSignalSemaphore( QueueType queueType, BinarySemaphore* , VkPipelineStageFlags2 stageFlags ) -> void;
        auto AddQueueWaitSemaphore( QueueType queueType, BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void;

        auto SetDebugName( VkObjectType objectType, u64 handle, eastl::string_view name ) -> void;

        auto WaitQueuesIdle() const -> void;

        auto SubmitDeletion( eastl::function<void( GpuDevice* )>&& callback ) -> void;

        MKT_NODISCARD auto GetDummySampler() -> Sampler*;
        MKT_NODISCARD auto GetGetLayoutForEmptySet() -> VkDescriptorSetLayout;

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
        auto InitDummyResources() -> void;
        auto InitDescriptorAllocator() -> void;
        auto GetPrimaryPhysicalDevice() -> void;
        auto CreatePrimaryLogicalDevice() -> void;

        auto InitTracyContext() -> void;
        auto ShutdownTracyContext() -> void;

        auto CreateDummyResources() -> void;
        auto DestroyDummyResources() -> void;

        MKT_NODISCARD auto IsDeviceSuitable( const PhysicalDevice& device) -> bool;

    private:
#if defined( MKT_USE_VULKAN_BINDLESS )
        const bool mIsBindlessEnabled{ true };
#else
        const bool mIsBindlessEnabled{ false };
#endif

        // [Command list management]
        ankerl::unordered_dense::map<QueueType, eastl::unique_ptr<Queue>> mQueues{};
        ankerl::unordered_dense::map<QueueType, eastl::unique_ptr<DeletionQueue>> mDeletionQueues{};

        // [Device management]
        VkDevice mLogicalDevice{};
        PhysicalDevice* mPhysicalDevice{};

        // [Memory management]
        eastl::unique_ptr<IGpuAllocator> mGpuAllocator{};
        eastl::unique_ptr<GpuUploadManager> mUploadManager{};

        SamplerHandle mDummySampler{};
        VkDescriptorSetLayout mEmptyDescriptorSetLayout{};

        eastl::unique_ptr<IDescriptorAllocatorPool> mDescriptorAllocatorPool{};

        std::vector<eastl::string> mExtensions{};

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
