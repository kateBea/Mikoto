/**
 * VulkanCommandPool.hh
 * Created by kate on 7/4/2023.
 * */

#ifndef MIKOTO_VULKAN_COMMAND_POOL_HH
#define MIKOTO_VULKAN_COMMAND_POOL_HH

// Third-Party Libraries
#include "volk.h"

// Project Headers
#include "Common/Common.hh"

namespace Mikoto {
    class VulkanCommandPool {
    public:
        explicit VulkanCommandPool() = default;
        ~VulkanCommandPool() = default;

        auto OnCreate(const VkCommandPoolCreateInfo& createInfo) -> void;

        MKT_NODISCARD auto Get() const -> VkCommandPool { return m_CommandPool; }

    private:
        VkCommandPool  m_CommandPool{};
        VkCommandPoolCreateInfo m_CreateInfo{};
    };
}


#endif // MIKOTO_VULKAN_COMMAND_POOL_HH
