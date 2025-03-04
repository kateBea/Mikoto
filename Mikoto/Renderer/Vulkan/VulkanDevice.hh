//
// Created by zanet on 1/26/2025.
//

#ifndef VULKANDEVICE_HH
#define VULKANDEVICE_HH

#include <functional>
#include <unordered_map>

#include <vk_mem_alloc.h>
#include <volk.h>

#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanImage.hh>

namespace Mikoto {

    struct VulkanDeviceCreateInfo {
        VkInstance* Instance{};

        // Set to a non-null surface if we want the device to support presentation
        VkSurfaceKHR* Surface{};

        // Necessary extensions to create the device
        Size_T RequiredExtensionsCount{};
        const char** DeviceRequestedExtensions{};

        // Validation layers Support ( no validations if null )
        Size_T ValidationsLayersCount{};
        const char** ValidationsLayers{ nullptr };

        // VMA Functions, required by the VMA Library
        const VmaVulkanFunctions* VmaCallbacks{ nullptr };
    };

    class VulkanDevice final : public VulkanObject {
    public:
        explicit VulkanDevice(const VulkanDeviceCreateInfo& createInfo);

        auto Init() -> void;

        ~VulkanDevice() override;

        MKT_NODISCARD auto FindSupportedFormat( const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features ) const -> VkFormat;

        auto WaitIdle() const -> void;

        auto CreateBuffer(const VulkanBufferCreateInfo & createInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo& allocationInfo ) const -> void;
        auto CreateImage(const VulkanImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo, VkImage& image, VmaAllocation& allocation, VmaAllocationInfo& allocationInf  ) const -> void;

        auto GetDeviceMinimumOffsetAlignment() const -> VkDeviceSize;

        // Device
        MKT_NODISCARD auto GetPhysicalDevice() const -> const VkPhysicalDevice& { return m_PhysicalDevice; }
        MKT_NODISCARD auto GetPhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures& { return m_PhysicalDeviceInfo.Features; }
        MKT_NODISCARD auto GetPhysicalDeviceProperties() const -> const VkPhysicalDeviceProperties& { return m_PhysicalDeviceInfo.Properties; }
        MKT_NODISCARD auto GetPhysicalDeviceMemoryProperties() const -> const VkPhysicalDeviceMemoryProperties& { return m_PhysicalDeviceInfo.MemoryProperties; }

        MKT_NODISCARD auto GetLogicalDevice() const -> const VkDevice& { return m_LogicalDevice; }
        MKT_NODISCARD auto GetLogicalDeviceQueues() const -> const QueuesData& { return m_QueueFamiliesData; }

        MKT_NODISCARD auto GetAllocatorStats() -> const VmaTotalStatistics&;
        MKT_NODISCARD auto GetAllocator() -> VmaAllocator& { return m_DefaultAllocator; }
        MKT_NODISCARD auto GetAllocator() const -> const VmaAllocator& { return m_DefaultAllocator; }

        // Commands
        auto RegisterGraphicsCommand( VkCommandBuffer cmd ) -> void;
        auto RegisterComputeCommand( VkCommandBuffer cmd ) -> void;

        // Queues
        auto SubmitCommandsGraphicsQueue(const FrameSynchronizationPrimitives& syncPrimitives ) -> void;
        auto SubmitCommandsComputeQueue(const ComputeSynchronizationPrimitives& syncPrimitives ) -> void;

        auto Release() -> void override;

    private:
        // [Internal usage]
        struct PhysicalDeviceRequiredFeatures {
            // Support for Anisotropic filtering
            bool AnysotropicFiltering{ true };

            // Support for wireframe mode
            bool FillModeNonSolid{ true };

            // Not null if we want the device to support presentation
            const VkSurfaceKHR* Surface{ nullptr };

            // List of extensions to support
            const std::vector<const char *>* RequestedExtensions{};
        };

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

        static auto GetPhysicalDeviceInfo(const VkPhysicalDevice& device) -> PhysicalDeviceInfo;
        static auto GetDeviceQueues( const VkDevice& device, QueuesData& queues ) -> void;
        static auto GetQueueFamilyIndices( const VkPhysicalDevice& device, const VkSurfaceKHR* surface ) -> QueuesData;
        static auto CheckExtensionsSupport( const VkPhysicalDevice& device, const std::vector<const char*>& requestedExtensions ) -> bool;
        static auto IsDeviceSuitable( const VkPhysicalDevice& device, const PhysicalDeviceRequiredFeatures& requirements ) -> bool;

    private:
        VkInstance* m_VulkanInstance{ nullptr };
        VkSurfaceKHR* m_Surface{ nullptr };

        QueuesData m_QueueFamiliesData{};
        VmaAllocator m_DefaultAllocator{};
        VmaTotalStatistics m_AllocatorStats{};
        const VmaVulkanFunctions* m_VmaCallbacks{ nullptr };

        VkDevice m_LogicalDevice{};
        VkPhysicalDevice m_PhysicalDevice{};

        PhysicalDeviceInfo m_PhysicalDeviceInfo{};

        std::vector<VkCommandBuffer> m_GraphicsSubmitCommands{};
        std::vector<VkCommandBuffer> m_ComputeSubmitCommands{};

        std::vector<CStr_T> m_ValidationsLayers{};
        std::vector<CStr_T> m_RequestedExtensions{};
    };
}



#endif //VULKANDEVICE_HH
