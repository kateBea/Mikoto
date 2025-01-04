/**
 * VulkanCommandPool.hh
 * Created by kate on 7/4/2023.
 * */

#ifndef MIKOTO_VULKAN_COMMAND_POOL_HH
#define MIKOTO_VULKAN_COMMAND_POOL_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanObject.hh>

namespace Mikoto {
    class VulkanCommandPool final : public VulkanObject<VkCommandPool, VkCommandPoolCreateInfo> {
    public:
        explicit VulkanCommandPool() = default;
        ~VulkanCommandPool() override = default;

        auto Create(const VkCommandPoolCreateInfo& createInfo) -> void override;
        auto Release() -> void override;
    };
}


#endif // MIKOTO_VULKAN_COMMAND_POOL_HH
