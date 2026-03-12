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

#ifndef MIKOTO_VULKAN_DEVICE_HH
#define MIKOTO_VULKAN_DEVICE_HH

#include <vector>
#include <mutex>
#include <deque>
#include <functional>

#include <volk.h>
#include <vk_mem_alloc.h>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <tracy/TracyVulkan.hpp>

#include "Renderer/Vulkan/VulkanBuffer.hh"
#include "Renderer/Vulkan/VulkanDescriptorManager.hh"
#include "Renderer/Vulkan/VulkanFramebuffer.hh"
#include "Renderer/Vulkan/VulkanMemoryAllocator.hh"
#include "Renderer/Vulkan/VulkanPipeline.hh"
#include "Renderer/Vulkan/VulkanShader.hh"
#include "Renderer/Vulkan/VulkanTexture.hh"

namespace Mikoto {

    class VulkanContext;

    class VulkanBuffer;
    class VulkanTexture;
    class VulkanTextureCube;

    class VulkanCmdList final : public ICommandList {
    public:
        explicit VulkanCmdList( const VkCommandBufferAllocateInfo& createInfo, QueueType type, bool immediate );

        auto Begin() -> void override;
        auto End() -> void override;

        auto BeginRender(RenderInfo& info) -> void override;
        auto EndRender(RenderInfo& info) -> void override;

        auto FillTexture(Buffer* src, Texture* dest) -> void override;
        auto FillTexture(const void* src, Size size, Texture* dest) -> void override;

        auto CopyBuffer(Buffer* src, Buffer* dest) -> void override;
        auto CopyBuffer(const void* src, Size size, Buffer* dest) -> void override;

        auto CopyBuffer( Buffer* src, Buffer* dest, Size dstOffset ) -> void override;

        auto CopyTexture(Texture* src, Texture* dest) -> void override;
        auto CopyTexture(Texture2D* src, TextureCube* dest, UInt32 mipLevel, UInt32 face) -> void override;

        auto SetPolygonLineWidth(float value) -> void override;

        auto WriteBuffer(Buffer* target, Byte* data, Size size) -> void override;
        auto WriteTexture(Texture* target, Byte* data, Size size) -> void override;

        auto SetViewport( Int32 x, Int32 y, Int32 width, Int32 height) -> void override;
        auto SetViewport( Int32 x, Int32 y, Int32 width, Int32 height, bool flip) -> void override;
        auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void override;

        auto Dispatch(UInt32 x, UInt32 y, UInt32 z) -> void  override;

        auto BindIndexBuffer( BufferHandle indexBuffer)-> void  override;
        auto BindVertexBuffer( BufferHandle vertexBuffer, UInt32 binding) -> void  override;

        auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance) -> void  override;

        auto DrawIndexedIndirect( BufferHandle indexBuffer, UInt32 offset, UInt32 drawCount, UInt32 stride ) -> void override;
        auto DrawIndexed( Size indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance)-> void  override;

        auto BindPipeline(PipelineHandle pipeline) -> void override;

        auto SetDebugName(std::string_view name) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~VulkanCmdList() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto FillTexture( VulkanBuffer* src, VulkanTexture* dest ) -> void;
        auto FillTexture( VulkanBuffer* src, VulkanTextureCube* dest ) -> void;

    private:
        VkCommandBuffer m_CmdBuffer{ VK_NULL_HANDLE };
        VkCommandBufferAllocateInfo m_AllocInfo{};
    };

    // Command pool and Command buffers from the same command pool can only be used by a single thread
    // Write operations from this object should be accessed by executed one thread at a time
    class VulkanCommandPool final : public DeviceObject {
    public:
        explicit VulkanCommandPool(QueueType queue, Size initialCmdListCount = 10);

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        MKT_NODISCARD auto IsFree() const -> bool { return m_InUse; }

        auto AllocateCmdList(bool immediate) -> CommandListHandle;
        auto SubmitCommandList() -> void;

        auto RunGarbageCollection() -> void;

        auto Clear() -> void;

        auto begin() { return m_CmdLists.begin(); }
        auto end() { return m_CmdLists.end(); }

        auto cbegin() const { return m_CmdLists.cbegin(); }
        auto cend() const { return m_CmdLists.cend(); }

        ~VulkanCommandPool() override;

        auto DestroyCommandList(CommandListHandle cmd ) -> void;

        using DeviceObject::Initialize;
    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanCommandPool);

        static auto DetermineQueueIndex(const QueuesData& queues, QueueType queue) -> UInt32;
    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        QueueType m_QueueType{ QueueType::GRAPHICS_QUEUE };

        VkCommandPool m_Pool{ VK_NULL_HANDLE };
        ResourcePoolTyped<VulkanCmdList> m_CmdLists{};

        std::atomic_bool m_InUse{ false };
    };

    using VulkanCommandPoolHandle = Ref<VulkanCommandPool>;

    class VulkanDeletionQueue final {
    public:

        explicit VulkanDeletionQueue(GpuDevice* device);

        auto Flush() -> void;
        auto Push(std::function<void(GpuDevice*)>&& callback ) -> void;

        auto Shutdown() -> void;

    private:
        GpuDevice* m_Device{};

        std::mutex m_PushMutex{};

        VulkanContext* m_Context{};

        // Frame index -> Deletion tasks
        ankerl::unordered_dense::map<UInt32, std::deque<std::function<void(GpuDevice*)>>> m_Callbacks{};
    };

    // ----------------------------------------------------------------------------
    // VulkanQueue. (TODO(kaTe): To implement still)
    // ----------------------------------------------------------------------------
    // Abstraction over a VkQueue that enables safe multithreaded command buffer
    // recording.
    //
    // Design goals:
    //  - Allow multiple threads to record command buffers in parallel.
    //  - Guarantee that no VkCommandPool is used concurrently by multiple threads.
    //  - Centralize submission orchestration as it is not a thread safe operation.
    //  - Block threads when no command pool is available.
    //
    // Overview:
    //
    //  Each VulkanQueue owns a set of VulkanCommandPools. A command pool is
    //  exclusively acquired by one thread at a time for recording. When a thread
    //  requests a command list via AllocateCommandList(), the queue:
    //
    //      1. Acquires a free command pool (blocking if none are available).
    //      2. Allocates a command buffer from that pool.
    //      3. Marks the pool as "in use" for that thread.
    //
    //  The CommandListHandle is aware of its originating pool. When all the command
    //  lists are submitted through SubmitCommandList(), it is added to the queue’s
    //  pending submission list and the its pool is marked as available (not "in use").
    //
    //  Flush() requests the submission thread to submit all pending command buffers
    //  to the underlying VkQueue. After GPU execution completes (via fence or
    //  frame-based synchronization), the associated command pools are reset and
    //  returned to the available pool set.
    //
    //  Important invariants:
    //      - A VkCommandPool is never accessed by multiple threads concurrently.
    //      - A pool is not reset until the GPU has finished executing all command
    //        buffers allocated from it.
    //      - Submission ordering is controlled centrally by VulkanQueue.
    //
    // ----------------------------------------------------------------------------
    class VulkanQueue final {
    public:
        explicit VulkanQueue(VulkanDevice* device, QueueType type);

        auto Init() -> void;
        auto Shutdown() -> void;

        // Tells the background thread to submit the pending command buffers to the gpu queue
        auto Flush() -> void;

        auto AllocateCommandList() -> CommandListHandle;
        auto SubmitCommandList(CommandListHandle cmd) -> void;
    private:
        VulkanDevice* m_Device{};

        VkQueue m_SubmissionQueue{};
        QueueType m_QueueType{ QueueType::GRAPHICS_QUEUE };

        // List of commands that have not been submitted yet
        std::vector<CommandListHandle> m_PendingCommandList{};

        ResourcePoolTyped<VulkanCommandPool> m_CmdPools{};
    };

    class VulkanDevice final : public GpuDevice {
    public:
        explicit VulkanDevice( const GpuDeviceCreateInfo& createInfo );

        // GPU Device Interface ================================================

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture(const TextureCubeCreateDescription& description) -> TextureHandle  override;
        MKT_NODISCARD auto CreateTexture( const TextureDescription& description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateBuffer( const BufferDescription& description ) -> BufferHandle override;
        MKT_NODISCARD auto CreateFrameBuffer( const FramebufferDescription& description ) -> FramebufferHandle override;
        MKT_NODISCARD auto CreateSampler( const SamplerDescription& description ) -> SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline(const ComputePipelineDescription& description) -> PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline(const GraphicsPipelineDescription& description) -> PipelineHandle override;
        MKT_NODISCARD auto LoadShader(const Path& path, ShaderStage stage) -> ShaderModuleHandle override;

        MKT_NODISCARD auto GetDeviceName() const -> std::string_view override;

        auto SubmitCommands( CommandListHandle cmd ) -> void override;
        MKT_NODISCARD auto CreateCommandList( QueueType queue, bool immediate ) -> CommandListHandle override;

        auto RunGarbageCollection() -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        MKT_NODISCARD auto GetMemoryUsage() const -> Size override;
        MKT_NODISCARD auto GetMemoryTotal() const -> Size override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> Size override;

        MKT_NODISCARD auto GetDummySampler() const -> SamplerHandle override;

        // Vulkan specifics ================================================
        MKT_NODISCARD auto CreateStaging( const void* src, Size size ) -> BufferHandle;

        auto WaitIdle() const -> void;
        auto WaitQueuesIdle() const -> void;

        auto GetTracyContext() -> TracyVkCtx&;

        auto FlushImmediateCommands() -> void;
        auto SetCurrentFrameIndex(UInt32 frameIndex) -> void;
        auto FlushPendingCommands( const FrameSynchronizationPrimitives& syncPrimitives ) -> void;

        auto SubmitDeletion(std::function<void(GpuDevice*)>&& callback) -> void;

        MKT_NODISCARD auto GetDummyDescriptorLayout() -> DescriptorSetLayoutHandle;

        MKT_NODISCARD auto GetUniformBufferMinOffsetAlignment() const -> VkDeviceSize;
        MKT_NODISCARD auto GetStorageBufferMinOffsetAlignment() const -> VkDeviceSize;

        MKT_NODISCARD auto GetPhysicalDevice() const -> const VkPhysicalDevice&;
        MKT_NODISCARD auto GetPhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures&;
        MKT_NODISCARD auto GetPhysicalDeviceProperties() const -> const VkPhysicalDeviceProperties&;
        MKT_NODISCARD auto GetPhysicalDeviceMemoryProperties() const -> const VkPhysicalDeviceMemoryProperties&;

        MKT_NODISCARD auto GetAllocator() -> GpuAllocator*;
        MKT_NODISCARD auto GetAllocator() const -> const GpuAllocator*;

        MKT_NODISCARD auto GetLogicalDevice() const -> const VkDevice&;
        MKT_NODISCARD auto GetLogicalDeviceQueues() const -> const QueuesData&;

        MKT_NODISCARD auto IsScalarBlockLayoutEnabled() const -> bool;

        MKT_NODISCARD auto AllocateDescriptorSet(const VkDescriptorSetLayout* layout, const void* pNext = nullptr) -> VkDescriptorSet;
        MKT_NODISCARD auto AllocateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo& layout) -> DescriptorSetLayoutHandle;

        auto CreateSwapChain( const VulkanSwapChainCreateInfo& createInfo ) -> SwapChainHandle;
        auto CreateSwapChainTextures( const VkImageViewCreateInfo& createInfo, VkExtent2D extent ) -> TextureHandle;

        MKT_NODISCARD auto IsBindlessEnabled() const -> bool;

        ~VulkanDevice() override = default;

    private:
        // [Internal usage]
        struct PhysicalDeviceInfo {
            VkPhysicalDeviceFeatures Features{};
            VkPhysicalDeviceProperties Properties{};
            VkPhysicalDeviceMemoryProperties MemoryProperties{};
        };

    private:
        // [Internal usage]
        auto InitMemoryAllocator() -> void;
        auto InitDescriptorAllocator() -> void;
        auto GetPrimaryPhysicalDevice() -> void;
        auto CreatePrimaryLogicalDevice() -> void;

        auto InitTracyContext() -> void;
        auto ShutdownTracyContext() -> void;

        auto CreateDummyResources() -> void;
        auto DestroyDummyResources() -> void;

    private:
#if defined(MKT_USE_VULKAN_BINDLESS)
        const bool m_IsBindlessEnabled{ true };
#else
        const bool m_IsBindlessEnabled{ false };
#endif

        // [Dummy resources]
        SamplerHandle m_sampler{};
        DescriptorSetLayoutHandle m_EmptyDescriptorSetLayout{};

        // [Resource Pools]
        ResourcePoolTyped<VulkanBuffer> m_Buffers{};
        ResourcePoolTyped<VulkanTexture> m_Textures{};
        ResourcePoolTyped<VulkanTextureCube> m_TexturesCube{};
        ResourcePoolTyped<VulkanCommandPool> m_CmdPools{};
        ResourcePoolTyped<VulkanFramebuffer> m_Framebuffers{};
        ResourcePoolTyped<VulkanGraphicsPipeline> m_GraphicsPipelines{};
        ResourcePoolTyped<VulkanComputePipeline> m_ComputePipelines{};
        ResourcePoolTyped<VulkanSwapChain> m_Swapchains{};
        ResourcePoolTyped<VulkanShader> m_Shaders{};
        ResourcePoolTyped<VulkanSampler> m_Samplers{};
        ResourcePoolTyped<DescriptorSetLayout> m_DescriptorSetLayouts{};

        // [Pool Mutexes]
        std::mutex m_BufferPoolMutex{};
        std::mutex m_StagingBufferPoolMutex{};
        std::mutex m_TexturePoolMutex{};
        std::mutex m_TextureCubePoolMutex{};
        std::mutex m_CommandPoolMutex{};
        std::mutex m_FramebufferPoolMutex{};
        std::mutex m_GraphicsPipelinePoolMutex{};
        std::mutex m_ComputePipelinePoolMutex{};
        std::mutex m_SwapchainPoolMutex{};
        std::mutex m_ShaderPoolMutex{};
        std::mutex m_SamplerPoolMutex{};
        std::mutex m_DescriptorSetLayoutPoolMutex{};

        DescriptorAllocator m_DescriptorAllocator{};

        Unique<VulkanDeletionQueue> m_DeletionQueue{};

        // [Command list management]
        QueuesData m_Queues{};

        std::mutex m_OneTimeSubmitMutex{};
        VulkanCommandPoolHandle m_OneTimeSubmitPool{};

        std::mutex m_CommandSubmitMutex{};
        std::mutex m_CommandCreateMutex{};

        VulkanCommandPoolHandle m_MainTimeSubmitPool{};

        UInt32 m_CurrentFrameIndex{};
        ankerl::unordered_dense::map<UInt32, VkFence> m_FrameFences{};
        ankerl::unordered_dense::map<UInt32, std::vector<CommandListHandle>> m_AvailableGraphicsCommandLists{};
        ankerl::unordered_dense::map<UInt32, std::vector<CommandListHandle>> m_PendingGraphicsCommandLists{};
        ankerl::unordered_dense::map<UInt32, std::vector<CommandListHandle>> m_SubmittedGraphicsCommandLists{};

        std::vector<CommandListHandle> m_ImmediateSubmitCmds{};

        // [Device management]
        VkDevice m_LogicalDevice{};
        VkPhysicalDevice m_PhysicalDevice{};

        PhysicalDeviceInfo m_PhysicalDeviceInfo{};
        std::vector<const char*> m_RequestedExtensions{};
        Unique<GpuAllocator> m_GpuAllocator{ nullptr };

        VkPhysicalDeviceVulkan12Features m_Vulkan12EnabledFeatures{};

        // [Tracy debug]
        VkCommandPool m_TracyPool{};
        VkCommandBuffer m_TracyCmd{};
        TracyVkCtx m_TracyContext{};
    };

// Macro helper to get the VkDevice from a GpuDevice pointer
#define VK_DEVICE(GPU_DEVICE_PTR) \
    dynamic_cast<VulkanDevice*>(GPU_DEVICE_PTR)->GetLogicalDevice()

#define MKT_VK_LOGICAL_DEVICE( GPU_DEVICE_PTR ) \
    dynamic_cast<VulkanDevice*>( GPU_DEVICE_PTR )->GetLogicalDevice()

#define TO_VK_DEVICE(GPU_DEVICE_PTR) \
    dynamic_cast<VulkanDevice*>(GPU_DEVICE_PTR)

#define MKT_VK_DEVICE( GPU_DEVICE_PTR ) \
    dynamic_cast<VulkanDevice*>( GPU_DEVICE_PTR )
}

#endif //MIKOTO_VULKAN_DEVICE_HH
