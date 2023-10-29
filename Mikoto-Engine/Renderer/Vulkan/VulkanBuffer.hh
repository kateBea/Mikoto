/**
 * VulkanBuffer.hh
 * Created by kate on 8/13/2023.
 * */

#ifndef MIKOTO_VULKAN_BUFFER_HH
#define MIKOTO_VULKAN_BUFFER_HH

// Third-Party Libraries
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/VulkanUtils.hh"

namespace Mikoto {
    class VulkanBuffer {
    public:
        explicit VulkanBuffer() = default;

        auto OnCreate(const BufferAllocateInfo& allocInfo) -> void;

        auto GetAllocationInfo() -> BufferAllocateInfo& { return m_AllocationInfo; }

        MKT_NODISCARD auto Get() const -> VkBuffer { return m_AllocationInfo.Buffer; }
        MKT_NODISCARD auto GetSize() const -> VkDeviceSize { return m_AllocationInfo.Size; }
        MKT_NODISCARD auto GetVmaAllocation() const -> VmaAllocation { return m_AllocationInfo.Allocation; }

    private:
        BufferAllocateInfo m_AllocationInfo{};
    };
}

#endif // MIKOTO_VULKAN_BUFFER_HH
