/**
 * VulkanCommandBuffer.hh
 * Created by kate on 8/8/2023.
 * */

#ifndef KATE_ENGINE_VULKAN_COMMAND_BUFFER_HH
#define KATE_ENGINE_VULKAN_COMMAND_BUFFER_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    class VulkanCommandBuffer {
    public:
        explicit VulkanCommandBuffer() = default;

        auto OnCreate(const VkCommandBufferAllocateInfo& allocateInfo) -> void;

        auto BeginRecording() const -> void;
        auto EndRecording() const -> void;

        MKT_NODISCARD auto Get() const -> const VkCommandBuffer& { return m_CommandBuffer; }
        MKT_NODISCARD auto GetAllocateInfo() const -> const VkCommandBufferAllocateInfo& { return m_AllocInfo; }

    private:
        VkCommandBuffer m_CommandBuffer{};
        VkCommandBufferAllocateInfo m_AllocInfo{};
    };
}


#endif//KATE_ENGINE_VULKAN_COMMAND_BUFFER_HH
