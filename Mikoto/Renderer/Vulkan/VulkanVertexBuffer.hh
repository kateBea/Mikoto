/**
 * VulkanVertexBuffer.hh
 * Created by kate on 6/17/23.
 * */

#ifndef MIKOTO_VULKAN_VERTEX_BUFFER_HH
#define MIKOTO_VULKAN_VERTEX_BUFFER_HH

// C++ Standard Library
#include <memory>
#include <vector>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanObject.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Buffer/VertexBuffer.hh>

namespace Mikoto {

    class VulkanVertexBuffer final : public VertexBuffer, public VulkanObject {
    public:
        explicit VulkanVertexBuffer(const VertexBufferCreateInfo& createInfo);

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD static auto GetDefaultBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>;
        MKT_NODISCARD static auto GetDefaultAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>;

        MKT_NODISCARD static auto Create(const VertexBufferCreateInfo& createInfo) -> Scope_T<VulkanVertexBuffer>;

        auto Release() -> void override;

        ~VulkanVertexBuffer() override;

        DISABLE_COPY_AND_MOVE_FOR(VulkanVertexBuffer);

    private:
        auto SetVertexData(const std::vector<float>& vertices) -> void;

        Scope_T<VulkanBuffer> m_Buffer{};
        std::vector<float> m_RetainedData{};
    };
}

#endif // MIKOTO_VULKAN_VERTEX_BUFFER_HH
