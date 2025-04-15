/**
 * VulkanCommands.hh
 * Created by kate on 7/4/2023.
 * */

#ifndef MIKOTO_VULKAN_COMMAND_POOL_HH
#define MIKOTO_VULKAN_COMMAND_POOL_HH

#include <vector>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Library/Utility/Types.hh>
#include <Renderer/DeviceObject.hh>

#include "Renderer/Vulkan/VulkanDevice.hh"

namespace Mikoto {

    struct VulkanCommandPoolCreateInfo {
        VulkanDevice* Device{ nullptr };
        VkCommandPoolCreateInfo CreateInfo{};
    };

    class VulkanCommandList final : public DeviceObject {
    public:


    private:

    };

    class VulkanCommandPool final : public DeviceObject {
    public:
        explicit VulkanCommandPool( const VulkanCommandPoolCreateInfo& createInfo);

        auto Get() -> VkCommandPool& { return m_CommandPool; }
        auto GetCreateInfo() -> VkCommandPoolCreateInfo& { return m_CreateInfo; }

        auto AllocateCommandBuffer( const VkCommandBufferAllocateInfo& allocateInfo) -> VkCommandBuffer*;

        auto GetCommandBuffers() -> std::vector<VkCommandBuffer>& { return m_CommandBuffers; }

        auto Release() -> void override;

        MKT_NODISCARD static auto Create(const VulkanCommandPoolCreateInfo& createInfo) -> Scope_T<VulkanCommandPool>;

        ~VulkanCommandPool() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanCommandPool);

    protected:
        auto Allocate() -> void override;

    private:
        VkCommandPool m_CommandPool{ VK_NULL_HANDLE };
        VkCommandPoolCreateInfo m_CreateInfo{};

        ResourcePoolTyped<VulkanCommandList> m_CommandBuffers{};
    };
}


#endif // MIKOTO_VULKAN_COMMAND_POOL_HH
