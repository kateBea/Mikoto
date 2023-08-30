/**
 * VulkanCommandPool.hh
 * Created by kate on 7/4/2023.
 * */

#ifndef MIKOTO_VULKAN_COMMAND_POOL_HH
#define MIKOTO_VULKAN_COMMAND_POOL_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    class VulkanCommandPool {
    public:
        explicit VulkanCommandPool() = default;
        ~VulkanCommandPool() = default;

        auto OnCreate(VkCommandPoolCreateInfo createInfo) -> void;

        MKT_NODISCARD auto GetCommandPool() const -> VkCommandPool { return m_CommandPool; }
        MKT_NODISCARD auto BeginSingleTimeCommands() const -> VkCommandBuffer;
        auto EndSingleTimeCommands(VkCommandBuffer commandBuffer) const -> void;

        auto OnRelease() const -> void;

    private:
        VkCommandPool  m_CommandPool{};
    };
}


#endif // MIKOTO_VULKAN_COMMAND_POOL_HH
