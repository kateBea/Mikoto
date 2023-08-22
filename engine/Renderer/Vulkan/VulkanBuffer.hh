/**
* VulkanBuffer.hh
* Created by kate on 8/13/2023.
* */

#ifndef MIKOTO_VULKAN_BUFFER_HH
#define MIKOTO_VULKAN_BUFFER_HH

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

        MKT_NODISCARD auto Get() const -> const VkBuffer& { return m_AllocationInfo.Buffer; }
        MKT_NODISCARD auto GetSize() const -> const VkDeviceSize& { return m_AllocationInfo.Size; }
        MKT_NODISCARD auto GetVmaAllocation() const -> const VmaAllocation& { return m_AllocationInfo.Allocation; }
    private:
        BufferAllocateInfo m_AllocationInfo{};
    };
}


#endif // MIKOTO_VULKAN_BUFFER_HH
