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
#include <EASTL/atomic.h>
#include <EASTL/functional.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <EASTL/optional.h>
#include <EASTL/fixed_vector.h>

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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <Renderer/Core/RenderContext.hh>

#include <Renderer/Rhi/Vulkan/VulkanBuffer.hh>
#include <Renderer/Rhi/Vulkan/VulkanShader.hh>
#include <Renderer/Rhi/Vulkan/VulkanTexture.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanInstance.hh>
#include <Renderer/Rhi/Vulkan/VulkanPipeline.hh>
#include <Renderer/Rhi/Vulkan/VulkanSwapchain.hh>
#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    // Timeline semaphore
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
        auto Release() -> void override;

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

        using DeviceObject::Initialize;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkSemaphore mSemaphore{ VK_NULL_HANDLE };
    };

    using BinarySemaphoreHandle = core::Ref<BinarySemaphore>;

    struct GpuUploadAllocation {
        rhi::IBuffer* mBuffer{};

        // This is the mapped address we should be writing to,
        // it corresponds to the beginning of the slice or
        // range that is available within this allocation.
        // You do not need to offset or anything
        void* mMappedMemory{};

        // Size of this sub-allocation
        core::size_t mSize{};

        // Specifies the offset of this allocation within the large
        // buffer it was allocated from
        core::size_t mOffset{};

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

        auto SubAllocate( core::size_t byteSize ) -> GpuUploadAllocation*;
        auto ReclaimMemory() -> void;

        ~GpuUploadManager();

    private:
        struct StagingAllocation {
            rhi::BufferHandle mBuffer{};
            eastl::unique_ptr<memory::MemoryArena<rhi::IBuffer, memory::FreeListFirstFitAllocator>> mMemoryArena{};
        };

        static constexpr core::u32 kMaxBuffers{ 32 };
        static constexpr core::u32 kMaxSubAllocations{ 100 };

        auto CreateBuffer() -> StagingAllocation*;
        auto CreateSubAllocation( rhi::IBuffer* buffer ) -> GpuUploadAllocation*;

    private:
        std::mutex mMutex{};

        rhi::IGpuDevice* mDevice{};
        ankerl::unordered_dense::map<rhi::IBuffer*, eastl::unique_ptr<StagingAllocation>> mBuffers{};
        ankerl::unordered_dense::map<rhi::IBuffer*, eastl::fixed_vector<eastl::unique_ptr<GpuUploadAllocation>, kMaxSubAllocations>> mSubAllocations{};
    };

    class CommandPool final : public rhi::DeviceObject {
    public:
        explicit CommandPool( rhi::IQueue* queue );

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        auto AllocateCmdList( bool isSecondary ) -> VkCommandBuffer;
        auto ReleaseCmdList( VkCommandBuffer cmd ) -> void;

        ~CommandPool() override;

        using DeviceObject::Initialize;

    public:
        DISABLE_COPY_AND_MOVE_FOR( CommandPool );

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        rhi::IQueue* mQueue{};
        VkCommandPool mPool{};

        core::u32 mQueueFamilyIndex{};
    };

    using CommandPoolHandle = core::Ref<CommandPool>;

    struct RecordingContext {
        core::u64 mSubmissionID{};
        VkCommandBuffer mCommandBuffer{};

        // Resources that I'm currently using,
        // I keep a strong reference to them to avoid
        // destroying them while this command buffer is still being executed
        eastl::fixed_vector<DeviceObjectHandle, 50> mInFlightResources{};

        // Suballocations that I'm currently using
        eastl::fixed_vector<GpuUploadAllocation*, 10> mInFlightSubAllocations{};
    };

    class CommandList final : public rhi::ICommandList {
    public:
        explicit CommandList( rhi::QueueType queueType, rhi::IQueue* queue, CommandPoolHandle pool );

        auto Begin( const rhi::CommandListBeginDescription& desc ) -> void override;
        auto End() -> void override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        // More relaxed versions of SetResourceState
        auto RecordBarrier( const rhi::BufferBarrierDescription& barrier ) -> void override;
        auto RecordBarrier( const rhi::TextureBarrierDescription& barrier ) -> void override;

        auto RecordTransition( rhi::IBuffer* buffer, rhi::ResourceStates stateBits ) -> void override;
        auto RecordTransition( rhi::ITexture* buffer, rhi::ResourceStates stateBits ) -> void override;

        auto SetTransition( rhi::IBuffer* buffer, rhi::ResourceStates stateBits ) -> void override;
        auto SetTransition( rhi::ITexture* buffer, rhi::ResourceStates stateBits ) -> void override;

        auto SetBarrier( const rhi::BufferBarrierDescription& barrier ) -> void override;
        auto SetBarrier( const rhi::TextureBarrierDescription& barrier ) -> void override;

        auto CommitBarriers() -> void override;

        auto SetEnableAutomaticBarriers( bool enable ) -> void override;

        auto SetClearColor( rhi::TextureHandle image, rhi::Color color ) -> void override;

        auto Write( rhi::IBuffer* src, rhi::ITexture* dest ) -> void override;
        auto Write( rhi::ITexture* texture, const void* data, core::size_t byteSize ) -> void override;
        auto Copy( rhi::ITexture* src, const rhi::TextureSlice& srcSlice, rhi::ITexture* dest, const rhi::TextureSlice& destSlice ) -> void override;

        auto Resolve( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void override;

        auto Write( rhi::IBuffer* buffer, core::size_t destOffset, const void* data, core::size_t byteSize ) -> void override;
        auto Write( rhi::IBuffer* buffer, const void* data, core::size_t byteSize ) -> void override;
        auto Copy( rhi::IBuffer* src, rhi::IBuffer* dest ) -> void override;
        auto Copy( rhi::IBuffer* src, rhi::IBuffer* dest, core::size_t destOffset ) -> void override;

        auto Copy( rhi::IBuffer* dest, rhi::ITexture* src ) -> void override;

        auto BeginRendering( rhi::GraphicsState& state ) -> void override;
        auto EndRendering() -> void override;

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

        auto SetPushConstants( rhi::IPipelineLayout* pipelineLayout, const void* data, size_t byteSize, rhi::ShaderFlags visibility ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto BeginDebugLabel( eastl::string_view name, rhi::Color color ) -> void override;
        auto EnbDebugLabel() -> void override;

        // Vulkan Specifics
        MKT_NODISCARD auto IsInUse() const -> bool;

        auto MarkExecuted( rhi::IQueue* queue, core::u64 submissionID) -> void;

        ~CommandList() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto ClearState() -> void;

    private:
        rhi::IQueue* mQueue{};
        rhi::IBuffer* mIndirectBuffer{};

        CommandPoolHandle mCommandPool{};

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

        bool mIsRenderScopeActive{};
        bool mEnableAutomaticBarriers{ true };

        eastl::vector<VkBufferMemoryBarrier2> mBufferBarriers{};
        eastl::vector<VkImageMemoryBarrier2> mImageBarriers{};

        // For debug
        rhi::Color mLabelColor{};
        eastl::string mRecordingScopeName{};
        eastl::string mRenderingScopeName{};
    };

    struct SubmitSemaphoresInfo {
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 5> mWaitSemaphores{};
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 5> mSignalSemaphores{};

        eastl::fixed_vector<rhi::CommandListHandle, 5> mCommands{};

        auto AddCommandList( rhi::CommandListHandle cmd ) -> SubmitSemaphoresInfo&;

        auto AddWaitFence( rhi::FenceHandle fence, core::u64 value, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo&;
        auto AddSignalFence( rhi::FenceHandle fence, core::u64 value, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo&;

        auto AddWaitSemaphore( BinarySemaphoreHandle semaphore, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo&;
        auto AddSignalSemaphore( BinarySemaphoreHandle semaphore, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo&;
    };

    // Internally multiple queues might map to the exact same
    // VkQueue from the same family index
    class Queue final : public rhi::IQueue {
    public:
        explicit Queue( rhi::QueueType type, rhi::QueueOpSupportFlags opFlags, core::u32 queueFamilyIndex, core::u32 queueIndex = 0 );

        auto ExecuteCommandLists( const SubmitInfo& submitInfo ) -> void override;

        // Vulkan Specifics
        auto WaitCompletionValue( u64 value ) -> void;
        auto ExecuteCommandLists( SubmitSemaphoresInfo&& submitInfo ) -> core::u64;

        // We still specify queue type because this queue might be a graphics queue
        // but still support transfer operations
        auto AllocateCmdList( rhi::QueueType type ) -> CommandListHandle;

        auto RunGarbageCollection() -> void;

        auto WaitIdle() const -> void;

        MKT_NODISCARD auto GetCurrentTimeline() -> core::u64;

        MKT_NODISCARD auto Present( const VkPresentInfoKHR& info ) -> VkResult;

        MKT_NODISCARD auto GetQueue() const -> VkQueue;
        MKT_NODISCARD auto GetQueueIndex() const -> core::u32;
        MKT_NODISCARD auto GetFamilyIndex() const -> core::u32;

        // Conversion operators
        operator core::u32() const;// Queue family index
        operator VkQueue() const;  // Logical queue

        ~Queue() override;

    private:
        MKT_NODISCARD auto AcquireThreadCmdPool() -> CommandPoolHandle;

    protected:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        std::mutex mSubmissionMutex{};

        core::u32 mFamilyIndex{ 0 };
        core::u32 mQueueIndex{ 0 };

        float mPriority{ 1.0f };
        VkQueue mQueue{ VK_NULL_HANDLE };

        static constexpr core::u32 kMaxSubmits{ 115 };

        eastl::atomic<core::u64> mTimelineValue{};
        rhi::FenceHandle mTimelineSemaphore{};

        std::mutex mPoolsMutex{};
        ankerl::unordered_dense::map<std::thread::id, CommandPoolHandle> mPools{};

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
        // std::deque is guaranteed to keep references to elements valid
        eastl::vector<VkWriteDescriptorSet> mWrites{};
        eastl::deque<VkDescriptorImageInfo> mImageInfos{};
        eastl::deque<VkDescriptorBufferInfo> mBufferInfos{};

        VkShaderStageFlags mShaderStages{};

        auto WriteSampler( core::u32 binding, VkSampler sampler, core::u32 arrayIndex = 0 ) -> DescriptorWriter&;
        auto WriteBuffer( core::u32 binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type, core::u32 arrayIndex = 0 ) -> DescriptorWriter&;
        auto WriteImage( core::u32 binding, VkImageView image, VkDescriptorType type, VkImageLayout layout, core::u32 arrayIndex = 0 ) -> DescriptorWriter&;

        auto SetVisibility( VkShaderStageFlags visibility ) -> DescriptorWriter&;

        auto Clear() -> void;
        auto UpdateSet( VkDevice device, VkDescriptorSet set ) -> void;
    };

    class BindingLayout final : public rhi::IBindingLayout {
    public:
        explicit BindingLayout( const rhi::BindingLayoutDescription& desc );
        explicit BindingLayout( const rhi::BindlessLayoutDescription& desc );

        MKT_NODISCARD auto GetRegisterSpace() const -> core::u32 override;

        MKT_NODISCARD auto IsBindless() const -> bool override;

        MKT_NODISCARD auto GetBindlessLayoutDesc() const -> const rhi::BindlessLayoutDescription&;

        auto SetDebugName( eastl::string_view name ) -> void override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD operator VkDescriptorSetLayout() const;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        core::u32 mSetIndex{};
        bool mIsBindless{ false };
        rhi::BindingLayoutDescription mBindingLayoutDesc{};
        rhi::BindlessLayoutDescription mBindlessLayoutDesc{};

        VkDescriptorSetLayout mDescriptorSetLayout{};
        VkDescriptorSetLayoutCreateInfo mCreateInfo{};
    };

    class DescriptorTable : public rhi::IDescriptorTable {
    public:
        explicit DescriptorTable( rhi::BindingLayoutHandle setLayout );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetCapacity( core::u32 slot ) const -> core::u32 override;

        MKT_NODISCARD auto GetResourceSlot( rhi::ResourceType type ) const -> core::i32;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~DescriptorTable() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkDescriptorSet mDescriptorSet{};
        VkDescriptorSetAllocateInfo mCreateInfo{};

        DescriptorAllocatorHandle mDescriptorAllocatorHandle{};

        // Slot -> Array index
        eastl::fixed_hash_map<rhi::ResourceType, core::i32, rhi::kMaxSlotsPerTable> mSlotResourceType{};

        rhi::BindingLayoutHandle mBindingLayout{};
    };

    class BindingSet : public rhi::IBindingSet {
    public:
        explicit BindingSet( const rhi::BindingSetDescription& desc, rhi::BindingLayoutHandle layout );

        auto SetDebugName( eastl::string_view name ) -> void override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~BindingSet() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkDescriptorSet mDescriptorSet{};
        VkDescriptorSetAllocateInfo mCreateInfo{};

        DescriptorAllocatorHandle mDescriptorAllocatorHandle{};

        rhi::BindingLayoutHandle mBindingLayout{};
        rhi::BindingSetDescription mBindingDescription{};
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
        auto Release() -> void override;

    private:
        rhi::InputLayoutCreateDescription mDesc{};
        eastl::fixed_vector<VkVertexInputBindingDescription, rhi::kMaxVertexBindings> mVertexBindingDescriptions{};
        eastl::fixed_vector<VkVertexInputAttributeDescription, rhi::kMaxVertexAttributes> mVertexAttributeDescriptions{};
    };

    class PipelineLayout : public rhi::IPipelineLayout {
    public:
        explicit PipelineLayout( const rhi::PipelineLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        // Vulkan Specifics
        MKT_NODISCARD operator VkPipelineLayout() const;

        ~PipelineLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkPipelineLayout mPipelineLayout{};

        rhi::PipelineLayoutCreateDescription mDescription{};
    };

    class Device final : public rhi::IGpuDevice {
    public:
        explicit Device( const rhi::GpuDeviceCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const rhi::TextureCreateDescription& description ) -> rhi::TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( rhi::ObjectType type, rhi::Object object, const rhi::TextureCreateDescription& description ) -> rhi::TextureHandle override;

        MKT_NODISCARD auto CreateBuffer( const rhi::BufferCreateDescription& description ) -> rhi::BufferHandle override;

        MKT_NODISCARD auto CreateSampler( const rhi::SamplerCreateDescription& description ) -> rhi::SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const rhi::ComputePipelineDescription& description ) -> rhi::PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const rhi::GraphicsPipelineDescription& description ) -> rhi::PipelineHandle override;

        MKT_NODISCARD auto CreateAccelStructure( const rhi::AccelStructureCreateDescription& description ) -> rhi::AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( rhi::QueueType type ) -> rhi::CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const rhi::ShaderModuleCreateDescription& desc ) -> rhi::ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout( const rhi::InputLayoutCreateDescription& desc ) -> rhi::InputLayoutHandle override;

        MKT_NODISCARD auto CreateBindingLayout( const rhi::BindingLayoutDescription& desc ) -> rhi::BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const rhi::PipelineLayoutCreateDescription& desc ) -> rhi::PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingSet( const rhi::BindingSetDescription& desc, rhi::BindingLayoutHandle layout ) -> rhi::BindingSetHandle override;

        MKT_NODISCARD auto CreateFence( core::u64 fenceInitialValue ) -> rhi::FenceHandle override;

        auto UnMap( rhi::IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map( rhi::IBuffer* buffer ) -> void* override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateBindlessLayout( const rhi::BindlessLayoutDescription& desc ) -> rhi::BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( rhi::BindingLayoutHandle layout ) -> rhi::DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( rhi::DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( rhi::DescriptorTableHandle descriptorTable, const rhi::BindingSetItem& item ) -> bool override;

        MKT_NODISCARD auto GetQueue( rhi::QueueType type ) -> IQueue* override;

        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;

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
        MKT_NODISCARD auto GetPhysicalDevice() -> PhysicalDevice*;
        MKT_NODISCARD auto GetAllocator() -> GpuMemoryAllocator*;

        // Query the physical device features we have enabled ourselves
        MKT_NODISCARD auto GetActivePhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures&;
        MKT_NODISCARD auto GetActivePhysicalDeviceFeatures2() const -> const VkPhysicalDeviceFeatures2&;

        // Query the vulkan physical device features we have enabled ourselves
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

        MKT_NODISCARD auto IsDeviceSuitable( const PhysicalDevice& device ) -> bool;

    private:
#if defined( MKT_USE_VULKAN_BINDLESS )
        const bool mIsBindlessEnabled{ true };
#else
        const bool mIsBindlessEnabled{ false };
#endif

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
        static constexpr eastl::string_view kPipelineCacheDirectory{ "Assets/.vulkan/" };

        rhi::SamplerHandle mDummySampler{};
        rhi::PipelineLayoutHandle mEmptyPipelineLayout{};
        rhi::BindingLayoutHandle mEmptyBindingLayout{};

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
