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
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace Mikoto {

    class VulkanBuffer final : public Buffer {
    public:
        explicit VulkanBuffer( const BufferDescription& createInfo );

        auto CopyToBlock( void* ptr, Size size ) -> void override;
        auto CopyFromBlock(const void* ptr, Size size) -> void override;
        auto CopyFromBlock( const void* ptr, Size size, Size offset ) -> void override;

        auto PersistentMap() -> void;
        auto PersistentUnmap() -> void;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto IsMapped() const -> bool { return m_Allocation.AllocationInfo.pMappedData != nullptr; }
        MKT_NODISCARD auto GetMappedAddress() const -> const void* { return m_Allocation.AllocationInfo.pMappedData; }

        ~VulkanBuffer() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

        auto SetDebugInfo() -> void;
        auto UploadHostData() -> void;
        auto UploadHostDataToStaging() -> void;

        auto InitMainBuffer() -> void;
        auto InitStaging() -> void;

    private:
        // When creating uniforms we need specify a minimum size for GPU memory alignment
        // S o basically store the size of the element individually and the count, this information is to be used later in the initialization
        Size m_ElementSize{};
        Size m_ElementCount{};
        Size m_MinPaddedSize{};

        BufferAllocation m_Allocation{};
        BufferAllocation m_StagingAllocation{};
    };
}// namespace Mikoto

#endif // MIKOTO_VULKAN_BUFFER_HH
