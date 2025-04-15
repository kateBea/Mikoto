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
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Buffer.hh>

namespace Mikoto {

    class VulkanBuffer final : public Buffer {
    public:
        explicit VulkanBuffer( const BufferDescription& createInfo );

        MKT_NODISCARD auto IsMapped() const -> bool { return m_VmaAllocationInfo.pMappedData != nullptr; }

        MKT_NODISCARD auto Get() -> VkBuffer& { return m_Buffer; }
        MKT_NODISCARD auto Get() const -> const VkBuffer& { return m_Buffer; }

        MKT_NODISCARD auto GetMappedAddress() const -> void* { return m_VmaAllocationInfo.pMappedData; }

        MKT_NODISCARD auto GetVmaAllocation() const -> const VmaAllocation& { return m_VmaAllocation; }
        MKT_NODISCARD auto GetVmaAllocationInfo() const -> const VmaAllocationInfo& { return m_VmaAllocationInfo; }

        MKT_NODISCARD auto GetBufferCreateInfo() const -> VkBufferCreateInfo { return m_BufferCreateInfo; }
        MKT_NODISCARD auto GetVamAllocationCreateInfo() const -> VmaAllocationCreateInfo { return m_AllocationCreateInfo; }

        auto PersistentMap() -> void;
        auto PersistentUnmap() -> void;

        ~VulkanBuffer() override;

    private:
        auto Release() -> void override;
        auto Allocate() -> void override;

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
