//
// Created by zanet on 1/26/2025.
//

#ifndef VULKANDEVICE_HH
#define VULKANDEVICE_HH

#include <vk_mem_alloc.h>
#include <volk.h>

#include <Renderer/GpuDevice.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

#include "VulkanDescriptorManager.hh"


namespace Mikoto {
    class VulkanDescriptorAllocator;
}
namespace Mikoto {
    // Forward declarations
    class VulkanTexture;
    class VulkanShader;
    class VulkanSampler;
    class VulkanRenderPass;
    class VulkanPipeline;
    class VulkanPipelineLayout;
    class VulkanDescriptorSetLayout;

    struct VulkanRenderPassDescription;
    struct VulkanPipelineLayoutDescription;

    struct VulkanDeviceDescription {
        VkInstance* Instance{};

        // Set to a non-null surface if we want the device to support presentation
        VkSurfaceKHR* Surface{};

        std::span<const char*> DeviceExtensions{};
        std::span<const char*> ValidationLayers{};

        // VMA Functions, required by the VMA Library
        const VmaVulkanFunctions* VmaCallbacks{ nullptr };
    };

    using VulkanRenderPassHandle = Ref<VulkanRenderPass>;
    using PipelineLayoutHandle = Ref<VulkanPipelineLayout>;
    using DescriptorSetLayoutHandle = Ref<VulkanDescriptorSetLayout>;

    class VulkanDevice final : public GpuDevice {
    public:
        explicit VulkanDevice(const VulkanDeviceDescription& createInfo);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        // Gpu resources
        MKT_NODISCARD auto CreateTexture( const TextureDescription& description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateBuffer( const BufferDescription& description ) -> BufferHandle override;
        MKT_NODISCARD auto CreateGraphicsPipeline( const GraphicsPipelineDescription& description ) -> GraphicsPipelineHandle override;
        MKT_NODISCARD auto CreateComputePipeline( const ComputePipelineDescription& description ) -> ComputePipelineHandle override;
        MKT_NODISCARD auto CreateShaderModule( const ShaderModuleDescription& description ) -> ShaderModuleHandle override;
        MKT_NODISCARD auto CreateSampler( const SamplerDescription& description ) -> SamplerHandle override;
        MKT_NODISCARD auto CreateFramebuffer( const FramebufferDescription& description ) -> FramebufferHandle override;

        // Dummy resources
        auto AccessDummyResource( Size_T resourceTypeID ) -> Ref<IResource> override;

        // Gpu Vulkan specific resources
        auto CreateRenderPass(const VulkanRenderPassDescription& description) -> VulkanRenderPassHandle;
        auto CreatePipelineLayout(const VulkanPipelineLayoutDescription& description) -> PipelineLayoutHandle;
        auto CreateDescriptorSetLayout(const VulkanShader* shaders, Size_T count) -> DescriptorSetLayoutHandle;

        auto WaitIdle() const -> void;

        auto RunGarbageCollection() -> void override;

        MKT_NODISCARD auto GetDeviceMinimumOffsetAlignment() const -> VkDeviceSize;

        MKT_NODISCARD auto GetPhysicalDevice() const -> const VkPhysicalDevice&;
        MKT_NODISCARD auto GetPhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures&;
        MKT_NODISCARD auto GetPhysicalDeviceProperties() const -> const VkPhysicalDeviceProperties&;
        MKT_NODISCARD auto GetPhysicalDeviceMemoryProperties() const -> const VkPhysicalDeviceMemoryProperties&;

        MKT_NODISCARD auto GetLogicalDevice() const -> const VkDevice&;
        MKT_NODISCARD auto GetLogicalDeviceQueues() const -> const QueuesData&;

        MKT_NODISCARD auto GetAllocator() -> VmaAllocator&;
        MKT_NODISCARD auto GetAllocator() const -> const VmaAllocator&;
        MKT_NODISCARD auto GetAllocatorStats() -> const VmaTotalStatistics&;

        // Commands
        auto RegisterGraphicsCommand( VkCommandBuffer cmd ) -> void;
        auto RegisterComputeCommand( VkCommandBuffer cmd ) -> void;

        // Queues
        auto SubmitCommandsGraphicsQueue(const GraphicsQueueSyncPrimitives& syncPrimitives ) -> void;
        auto SubmitCommandsComputeQueue(const ComputeQueueSynchPrimitives& syncPrimitives ) -> void;

        ~VulkanDevice() override;

    private:
        // [Internal usage]

        struct PhysicalDeviceInfo {
            VkPhysicalDeviceFeatures Features{};
            VkPhysicalDeviceProperties Properties{};
            VkPhysicalDeviceMemoryProperties MemoryProperties{};
        };

    private:
        // [Internal usage]
        auto CreateDummyResources() -> void;
        auto CreateDummyTexture() -> void;
        auto CreateDummyTextureSampler() -> void;

        auto InitDescriptorAllocator() -> void;
        auto InitMemoryAllocator() -> void;

        auto GetPrimaryPhysicalDevice() -> void;
        auto CreatePrimaryLogicalDevice() -> void;

    private:
        // We can create some dummy resources like a sampler, and empty texture
        // which can be used everywhere, for instance the sampler can be poassed to descriptorsetlayout in the immutable sampler
        // or probably use it for other images instead of creating a new one. If the requested sampler has to be custom tho we dont use the dummy one
        ResourcePoolTyped<Buffer> m_Buffers{};
        ResourcePoolTyped<Texture> m_Textures{};
        ResourcePoolTyped<Sampler> m_Samplers{};
        ResourcePoolTyped<Framebuffer> m_Framebuffers{};
        ResourcePoolTyped<ShaderModule> m_ShaderModules{};
        ResourcePoolTyped<ComputePipeline> m_ComputePipelines{};
        ResourcePoolTyped<GraphicsPipeline> m_GraphicsPipelines{};

        // Vulkan-specific GPU resource pools
        ResourcePoolTyped<VulkanRenderPass> m_RenderPasses{};
        ResourcePoolTyped<VulkanPipelineLayout> m_PipelineLayouts{};
        ResourcePoolTyped<VulkanDescriptorSetLayout> m_DescriptorSetLayouts{};

        VulkanDescriptorAllocator m_DescriptorAllocator{};

        QueuesData m_QueueFamiliesData{};
        VmaAllocator m_DefaultAllocator{};
        VmaTotalStatistics m_AllocatorStats{};

        VkDevice m_LogicalDevice{};
        VkPhysicalDevice m_PhysicalDevice{};

        PhysicalDeviceInfo m_PhysicalDeviceInfo{};

        std::vector<VkCommandBuffer> m_GraphicsSubmitCommands{};
        std::vector<VkCommandBuffer> m_ComputeSubmitCommands{};

        // Vulkan context
        VkSurfaceKHR* m_Surface{ nullptr };
        VkInstance* m_VulkanInstance{ nullptr };

        const VmaVulkanFunctions* m_VmaCallbacks{ nullptr };

        std::vector<CStr_T> m_ValidationsLayers{};
        std::vector<CStr_T> m_RequestedExtensions{};
    };
}



#endif //VULKANDEVICE_HH
