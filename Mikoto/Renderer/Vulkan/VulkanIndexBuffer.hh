/**
 * VulkanIndexBuffer
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_INDEX_BUFFER_HH
#define MIKOTO_VULKAN_INDEX_BUFFER_HH

// C++ Standard Library
#include <memory>
#include <vector>
#include <span>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Renderer/Buffer/IndexBuffer.hh>

#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanObject.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>

namespace Mikoto {
    struct VulkanIndexBufferCreateInfo {
        std::span<const UInt32_T> Indices{};
    };

    class VulkanIndexBuffer final : public IndexBuffer, public VulkanObject {
    public:

        explicit VulkanIndexBuffer(const VulkanIndexBufferCreateInfo& createInfo);

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        auto Release() -> void override;

        ~VulkanIndexBuffer() override;

        MKT_NODISCARD static auto Create(const VulkanIndexBufferCreateInfo& createInfo) -> Scope_T<VulkanIndexBuffer>;

    private:
        auto LoadIndices(const std::span<const UInt32_T> &indices) -> void;

    private:
        Scope_T<VulkanBuffer> m_Buffer{};
    };
}



#endif // MIKOTO_VULKAN_INDEX_BUFFER_HH
