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
#include <Common/Common.hh>
#include <Library/Random/Random.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanObject.hh>

namespace Mikoto {
    struct VulkanBufferCreateInfo {
        VkBufferCreateInfo BufferCreateInfo{};
        VmaAllocationCreateInfo AllocationCreateInfo{};

        // True if the allocation was mapped, false otherwise.
        // This is only used at the moment of creation of the buffer,
        // not to keep track of whether the buffer is mapped or not.
        // Allocation must be unmapped before destruction.
        bool WantMapping{};
    };

    class VulkanBuffer final  : public VulkanObject {
    public:
        explicit VulkanBuffer(const VulkanBufferCreateInfo& createInfo);

        auto Release() -> void override;

        ~VulkanBuffer() override;

        MKT_NODISCARD auto IsMapped() const -> bool { return m_IsMapped; }

        MKT_NODISCARD auto Get() const -> VkBuffer { return m_Buffer; }
        MKT_NODISCARD auto GetSize() const -> VkDeviceSize { return m_Size; }
        MKT_NODISCARD auto GetMappedPtr() const -> void* { return m_MappedAddress; }

        MKT_NODISCARD auto GetVmaAllocation() const -> VmaAllocation { return m_VmaAllocation; }
        MKT_NODISCARD auto GetVmaAllocationInfo() const -> VmaAllocationInfo { return m_VmaAllocationInfo; }

        MKT_NODISCARD auto GetBufferCreateInfo() const -> VkBufferCreateInfo { return m_BufferCreateInfo; }
        MKT_NODISCARD auto GetVamAllocationCreateInfo() const -> VmaAllocationCreateInfo { return m_AllocationCreateInfo; }

        MKT_NODISCARD static auto Create(const VulkanBufferCreateInfo& createInfo) -> Scope_T<VulkanBuffer>;

        auto PersistentMap() -> void;
        auto PersistentUnmap() -> void;

    private:
        VkBuffer m_Buffer{};

        // See vmaCreteBuffer for details on these
        VmaAllocation m_VmaAllocation{};
        VmaAllocationInfo m_VmaAllocationInfo{};

        VkDeviceSize m_Size{};
        VkBufferCreateInfo m_BufferCreateInfo{};
        VmaAllocationCreateInfo m_AllocationCreateInfo{};

        bool m_IsMapped{};
        void* m_MappedAddress{};
    };
}

#endif // MIKOTO_VULKAN_BUFFER_HH
