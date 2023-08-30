/**
 * VulkanCommandBuffer.hh
 * Created by kate on 8/8/2023.
 * */

#ifndef MIKOTO_VULKAN_COMMAND_BUFFER_HH
#define MIKOTO_VULKAN_COMMAND_BUFFER_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    class VulkanCommandBuffer {
    public:
        explicit VulkanCommandBuffer() = default;

        auto OnCreate(const VkCommandBufferAllocateInfo& allocateInfo) -> void;

        auto BeginRecording() -> void;
        auto EndRecording() -> void;

        MKT_NODISCARD auto Get() const -> const VkCommandBuffer& { return m_CommandBuffer; }
        MKT_NODISCARD auto GetAllocateInfo() const -> const VkCommandBufferAllocateInfo& { return m_AllocInfo; }

    private:
        bool m_Recording{ false };
        VkCommandBuffer m_CommandBuffer{};
        VkCommandBufferAllocateInfo m_AllocInfo{};
    };
}


#endif // MIKOTO_VULKAN_COMMAND_BUFFER_HH
