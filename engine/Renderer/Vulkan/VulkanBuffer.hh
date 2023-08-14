/**
* VulkanBuffer.hh
* Created by kate on 8/13/2023.
* */

#ifndef KATE_ENGINE_VULKAN_BUFFER_HH
#define KATE_ENGINE_VULKAN_BUFFER_HH

// C++ Standard Library

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>

namespace Mikoto {
    class VulkanBuffer {
    public:
        explicit VulkanBuffer() = default;

        auto OnCreate(const BufferAllocateInfo& allocInfo) -> void;
        auto OnRelease() const -> void;

        KT_NODISCARD auto Get() const -> const VkBuffer& { return m_Buffer; }
        KT_NODISCARD auto GetSize() const -> const VkDeviceSize& { return m_Size; }
        KT_NODISCARD auto GetVmaAllocation() const -> const VmaAllocation& { return m_Allocation; }
    private:
        VkBuffer m_Buffer{};
        VkDeviceSize m_Size{};
        VmaAllocation m_Allocation{};
    };
}


#endif // KATE_ENGINE_VULKAN_BUFFER_HH
