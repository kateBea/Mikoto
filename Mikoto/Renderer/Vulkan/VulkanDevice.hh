//
// Created by zanet on 1/26/2025.
//

#ifndef VULKANDEVICE_HH
#define VULKANDEVICE_HH

#include <vk_mem_alloc.h>
#include <volk.h>

#include <Renderer/GpuDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

#include "Renderer/Vulkan/VulkanBuffer.hh"
#include "Renderer/Vulkan/VulkanTexture.hh"
#include "VulkanMemoryAllocator.hh"


namespace Mikoto {

    // Forward declare for VulkanCmdList
    class VulkanCommandPool;

    class VulkanCmdList final : public ICommandList {
    public:
        explicit VulkanCmdList(const VkCommandBufferAllocateInfo& createInfo);

        auto Begin() -> void override;
        auto Close() -> void override;

        auto FillTexture(Buffer* src, Texture* dest) -> void override;
        auto CopyTexture(Buffer* src, Buffer* dest) -> void override;
        auto CopyBuffer(Texture* src, Texture* dest) -> void override;

        auto WriteBuffer(Buffer* target, Byte* data, Size size) -> void override;
        auto WriteTexture(Texture* target, Byte* data, Size size) -> void override;

        MKT_NODISCARD auto GetImplHandle() -> VkCommandBuffer* { return std::addressof(m_CmdBuffer); }

        ~VulkanCmdList() override;
    private:
        auto Allocate() -> void override;
        auto Release() -> void override;
    private:
        VkCommandBuffer m_CmdBuffer{ VK_NULL_HANDLE };
        VkCommandBufferAllocateInfo m_AllocInfo{};
    };

    class VulkanCommandPool final : public DeviceObject {
    public:
        explicit VulkanCommandPool(QueueType queue, Size initialCmdListCount = 10);

        MKT_NODISCARD auto GetImplHandle() -> VkCommandPool* { return std::addressof(m_Pool); }

        MKT_NODISCARD auto IsPoolLocked() const -> bool { return m_InUse.load(); }

        auto AllocateCmdList() -> CommandListHandle;

        ~VulkanCommandPool() override;
    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanCommandPool);

        static auto DetermineQueueIndex(const QueuesData& queues, QueueType queue) -> UInt32;
    private:
        auto Allocate() -> void override;
        auto Release() -> void override;

    private:
        // Command pools cannot be shared between threads
        std::atomic_bool m_InUse{};

        QueueType m_QueueType{ QueueType::GRAPHICS_QUEUE };

        VkCommandPool m_Pool{ VK_NULL_HANDLE };
        ResourcePoolTyped<VulkanCmdList> m_CmdLists{};
    };

    class VulkanDevice final : public GpuDevice {
    public:
        explicit VulkanDevice( const GpuDeviceCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto WaitIdle() const -> void;

        MKT_NODISCARD auto CreateTexture( const TextureDescription& description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateBuffer( const BufferDescription& description ) -> BufferHandle override;

        auto SubmitCommands(CommandListHandle cmd) -> void override;

        auto RunGarbageCollection() -> void override;

        MKT_NODISCARD auto CreateCommandList(QueueType queue = QueueType::GRAPHICS_QUEUE) -> CommandListHandle override;

        // Return the minimum required alignment (in bytes) for uniform buffers
        auto GetUniformBufferMinOffsetAlignment() const -> VkDeviceSize;

        MKT_NODISCARD auto GetPhysicalDevice() const -> const VkPhysicalDevice&;
        MKT_NODISCARD auto GetPhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures&;
        MKT_NODISCARD auto GetPhysicalDeviceProperties() const -> const VkPhysicalDeviceProperties&;
        MKT_NODISCARD auto GetPhysicalDeviceMemoryProperties() const -> const VkPhysicalDeviceMemoryProperties&;
        MKT_NODISCARD auto GetAllocator() -> GpuAllocator*;
        MKT_NODISCARD auto GetAllocator() const -> const GpuAllocator*;

        MKT_NODISCARD auto GetLogicalDevice() const -> const VkDevice&;
        MKT_NODISCARD auto GetLogicalDeviceQueues() const -> const QueuesData&;

        MKT_NODISCARD auto GetSwapChain() -> SwapChainHandle;
        MKT_NODISCARD auto GetSwapChainPtr() -> VulkanSwapChain*;

        auto InitializeSwapchain( const VulkanSwapChainCreateInfo& createInfo ) -> void;
        auto CreateSwapChainTextures( const VkImageViewCreateInfo& createInfo ) -> TextureHandle;

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
        auto GetPrimaryPhysicalDevice() -> void;
        auto CreatePrimaryLogicalDevice() -> void;

    private:
        SwapChainHandle m_SwapChain{};

        ResourcePoolTyped<VulkanBuffer> m_Buffers{};
        ResourcePoolTyped<VulkanTexture> m_Textures{};
        ResourcePoolTyped<VulkanCommandPool> m_CmdPools{};

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
