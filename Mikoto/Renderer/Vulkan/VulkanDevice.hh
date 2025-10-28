//
// Created by zanet on 1/26/2025.
//

#ifndef VULKANDEVICE_HH
#define VULKANDEVICE_HH

#include <volk.h>
#include <vk_mem_alloc.h>

#include <Material/ShaderLibrary.hh>
#include <Renderer/GpuDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

#include "Renderer/Vulkan/VulkanBuffer.hh"
#include "Renderer/Vulkan/VulkanFramebuffer.hh"
#include "Renderer/Vulkan/VulkanMemoryAllocator.hh"
#include "Renderer/Vulkan/VulkanPipeline.hh"
#include "Renderer/Vulkan/VulkanTexture.hh"
#include "Renderer/Vulkan/VulkanShader.hh"
#include "Renderer/Vulkan/VulkanDescriptorManager.hh"

namespace Mikoto {

    class VulkanCmdList final : public ICommandList {
    public:
        explicit VulkanCmdList(const VkCommandBufferAllocateInfo& createInfo);

        auto Begin() -> void override;
        auto End() -> void override;

        auto FillTexture(Buffer* src, Texture* dest) -> void override;
        auto CopyBuffer(Buffer* src, Buffer* dest) -> void override;
        auto CopyTexture(Texture* src, Texture* dest) -> void override;

        auto WriteBuffer(Buffer* target, Byte* data, Size size) -> void override;
        auto WriteTexture(Texture* target, Byte* data, Size size) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~VulkanCmdList() override;
    private:
        auto Initialize() -> void override;
        auto Release() -> void override;
    private:
        VkCommandBuffer m_CmdBuffer{ VK_NULL_HANDLE };
        VkCommandBufferAllocateInfo m_AllocInfo{};
    };

    class VulkanCommandPool final : public DeviceObject {
    public:
        explicit VulkanCommandPool(QueueType queue, Size initialCmdListCount = 10);

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        MKT_NODISCARD auto IsPoolLocked() const -> bool { return m_InUse.load(); }

        auto AllocateCmdList() -> CommandListHandle;

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
        // Command pools cannot be shared between threads
        std::atomic_bool m_InUse{};

        QueueType m_QueueType{ QueueType::GRAPHICS_QUEUE };

        VkCommandPool m_Pool{ VK_NULL_HANDLE };
        ResourcePoolTyped<VulkanCmdList> m_CmdLists{};
    };

    using VulkanCommandPoolHandle = Ref<VulkanCommandPool>;

    class VulkanDevice final : public GpuDevice {
    public:
        explicit VulkanDevice( const GpuDeviceCreateInfo& createInfo );

        // GPU Device Interface ================================================

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const TextureDescription& description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateBuffer( const BufferDescription& description ) -> BufferHandle override;
        MKT_NODISCARD auto CreateFrameBuffer( const FramebufferDescription& description ) -> FramebufferHandle override;
        MKT_NODISCARD auto CreateSampler( const SamplerDescription& description ) -> SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline(const ComputePipelineDescription& description) -> PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline(const GraphicsPipelineDescription& description) -> PipelineHandle override;
        MKT_NODISCARD auto LoadShader(const Path& path, ShaderStage stage) -> ShaderModuleHandle override;

        MKT_NODISCARD auto GetDeviceName() const -> std::string_view override;

        auto SubmitCommands( CommandListHandle cmd ) -> void override;
        auto RunGarbageCollection() -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        MKT_NODISCARD auto CreateCommandList( QueueType queue ) -> CommandListHandle override;

        // Vulkan specifics ================================================

        auto WaitIdle() const -> void;
        auto WaitQueuesIdle() const -> void;

        // Return the minimum required alignment (in bytes) for uniform buffers
        MKT_NODISCARD auto GetUniformBufferMinOffsetAlignment() const -> VkDeviceSize;

        MKT_NODISCARD auto GetPhysicalDevice() const -> const VkPhysicalDevice&;
        MKT_NODISCARD auto GetPhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures&;
        MKT_NODISCARD auto GetPhysicalDeviceProperties() const -> const VkPhysicalDeviceProperties&;
        MKT_NODISCARD auto GetPhysicalDeviceMemoryProperties() const -> const VkPhysicalDeviceMemoryProperties&;
        MKT_NODISCARD auto GetAllocator() -> GpuAllocator*;
        MKT_NODISCARD auto GetAllocator() const -> const GpuAllocator*;

        MKT_NODISCARD auto GetLogicalDevice() const -> const VkDevice&;
        MKT_NODISCARD auto GetLogicalDeviceQueues() const -> const QueuesData&;

        MKT_NODISCARD auto AllocateDescriptorSet(const VkDescriptorSetLayout& layout, const void* pNext = nullptr) -> VkDescriptorSet;
        MKT_NODISCARD auto AllocateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo& layout) -> DescriptorSetLayoutHandle;

        auto FlushPendingCommands( const FrameSynchronizationPrimitives& syncPrimitives ) -> void;

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

    private:
#if defined(MKT_USE_VULKAN_BINDLESS)
        const bool m_IsBindlessEnabled{ true };
#else
        const bool m_IsBindlessEnabled{ false };
#endif

        DescriptorAllocator m_DescriptorAllocator{};

        ResourcePoolTyped<VulkanBuffer> m_Buffers{};
        ResourcePoolTyped<VulkanTexture> m_Textures{};
        ResourcePoolTyped<VulkanCommandPool> m_CmdPools{};
        ResourcePoolTyped<VulkanFramebuffer> m_Framebuffers{};
        ResourcePoolTyped<VulkanGraphicsPipeline> m_GraphicsPipelines{};
        ResourcePoolTyped<VulkanComputePipeline> m_ComputePipelines{};
        ResourcePoolTyped<VulkanSwapChain> m_Swapchains{};
        ResourcePoolTyped<VulkanShader> m_Shaders{};
        ResourcePoolTyped<VulkanSampler> m_Samplers{};
        ResourcePoolTyped<DescriptorSetLayout> m_DescriptorSetLayouts{};

        ankerl::unordered_dense::map<UInt32, VkFence> m_FrameFences{};
        ankerl::unordered_dense::map<UInt32, std::vector<CommandListHandle>> m_PendingCmdLists{};

        QueuesData m_Queues{};

        Unique<GpuAllocator> m_GpuAllocator{ nullptr };

        VkDevice m_LogicalDevice{};
        VkPhysicalDevice m_PhysicalDevice{};

        PhysicalDeviceInfo m_PhysicalDeviceInfo{};
        std::vector<const char*> m_RequestedExtensions{};
    };

// Macro helper to get the VkDevice from a GpuDevice pointer
#define VK_DEVICE(GPU_DEVICE_PTR) \
    dynamic_cast<VulkanDevice*>(GPU_DEVICE_PTR)->GetLogicalDevice()

#define TO_VK_DEVICE(GPU_DEVICE_PTR) \
    dynamic_cast<VulkanDevice*>(GPU_DEVICE_PTR)
}



#endif //VULKANDEVICE_HH
