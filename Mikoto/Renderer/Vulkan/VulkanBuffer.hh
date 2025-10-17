/**
 * VulkanBuffer.hh
 * Created by kate on 8/13/2023.
 * */

#ifndef MIKOTO_VULKAN_BUFFER_HH
#define MIKOTO_VULKAN_BUFFER_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/Random/Random.hh>
#include <Renderer/Buffer.hh>

namespace Mikoto {

    class VulkanBuffer final : public Buffer {
    public:
        explicit VulkanBuffer( const BufferDescription& createInfo );

        auto CopyFromBlock(const void* ptr, Size size) -> void override;

        MKT_NODISCARD auto GetImplHandle() -> VkBuffer* { return std::addressof(m_Buffer); }

        MKT_NODISCARD auto GetBuffer() -> VkBuffer* { return std::addressof(m_Buffer); }
        MKT_NODISCARD auto GetBuffer() const -> const VkBuffer* { return std::addressof(m_Buffer); }

        MKT_NODISCARD auto GetVmaAllocation() -> VmaAllocation* { return std::addressof(m_VmaAllocation); }
        MKT_NODISCARD auto GetVmaAllocation() const -> const VmaAllocation* { return std::addressof(m_VmaAllocation);; }

        MKT_NODISCARD auto GetVmaAllocationInfo() -> VmaAllocationInfo* { return std::addressof(m_VmaAllocationInfo); }
        MKT_NODISCARD auto GetVmaAllocationInfo() const -> const VmaAllocationInfo* { return std::addressof(m_VmaAllocationInfo); }

        MKT_NODISCARD auto GetBufferCreateInfo() -> VkBufferCreateInfo* { return std::addressof(m_BufferCreateInfo); }
        MKT_NODISCARD auto GetBufferCreateInfo() const -> const VkBufferCreateInfo* { return std::addressof(m_BufferCreateInfo); }

        MKT_NODISCARD auto GetAllocationCreateInfo() -> VmaAllocationCreateInfo* { return std::addressof(m_AllocationCreateInfo); }
        MKT_NODISCARD auto GetAllocationCreateInfo() const -> const VmaAllocationCreateInfo* { return std::addressof(m_AllocationCreateInfo); }

        auto PersistentMap() -> void;
        auto PersistentUnmap() -> void;
        MKT_NODISCARD auto IsMapped() const -> bool { return m_VmaAllocationInfo.pMappedData != nullptr; }
        MKT_NODISCARD auto GetMappedAddress() const -> const void* { return m_VmaAllocationInfo.pMappedData; }

        ~VulkanBuffer() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        VkBuffer m_Buffer{};

        // See vmaCreteBuffer for details on these
        VmaAllocation m_VmaAllocation{};
        VmaAllocationInfo m_VmaAllocationInfo{};

        VkBufferCreateInfo m_BufferCreateInfo{};
        VmaAllocationCreateInfo m_AllocationCreateInfo{};
    };
}// namespace Mikoto

#endif // MIKOTO_VULKAN_BUFFER_HH
