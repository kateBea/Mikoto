/**
 * VulkanBuffer.hh
 * Created by kate on 8/13/2023.
 * */

#ifndef MIKOTO_VULKAN_BUFFER_HH
#define MIKOTO_VULKAN_BUFFER_HH

// C++ Standard Library


// Third-Party Libraries
#include "volk.h"
#include "vk_mem_alloc.h"

// Project Headers
#include "Common/Common.hh"
#include "Renderer/Vulkan/VulkanUtils.hh"

namespace Mikoto {
    class VulkanBuffer {
    public:
        explicit VulkanBuffer() = default;

        auto PersistentMap() -> void;
        auto PersistentUnmap() -> void;
        auto OnCreate(const BufferAllocateInfo& allocInfo) -> void;

        auto GetAllocationInfo() -> BufferAllocateInfo& { return m_AllocationInfo; }

        MKT_NODISCARD auto IsMapped() const -> bool { return m_IsMapped; }
        MKT_NODISCARD auto GetMappedPtr() -> void* { return m_MappedAddress; }
        MKT_NODISCARD auto Get() const -> VkBuffer { return m_AllocationInfo.Buffer; }
        MKT_NODISCARD auto GetSize() const -> VkDeviceSize { return m_AllocationInfo.Size; }
        MKT_NODISCARD auto GetVmaAllocation() const -> VmaAllocation { return m_AllocationInfo.Allocation; }

    private:
        void* m_MappedAddress{};
        BufferAllocateInfo m_AllocationInfo{};

        bool m_IsMapped{};
    };
}

#endif // MIKOTO_VULKAN_BUFFER_HH
