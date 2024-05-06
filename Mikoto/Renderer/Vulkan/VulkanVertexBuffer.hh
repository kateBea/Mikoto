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
#include "glm/glm.hpp"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/VulkanUtils.hh"
#include "Renderer/Buffers/VertexBuffer.hh"
#include "VulkanBuffer.hh"

namespace Mikoto {
    class VulkanVertexBuffer : public VertexBuffer {
    public:
        explicit VulkanVertexBuffer(VertexBufferCreateInfo&& createInfo);

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto GetBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>& { return m_BindingDesc; }
        MKT_NODISCARD auto GetAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>& { return m_AttributeDesc; }

        auto SetBindingDescriptions() -> void;
        auto SetAttributeDescriptions() -> void;

        static auto GetDefaultBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>&;
        static auto GetDefaultAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>&;

        ~VulkanVertexBuffer() override = default;
    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanVertexBuffer);

    private:
        auto SetVertexData(const std::vector<float>& vertices) -> void;

    private:
        inline static std::vector<VkVertexInputBindingDescription> s_BindingDesc{};
        inline static std::vector<VkVertexInputAttributeDescription> s_AttributeDesc{};

    private:
        VulkanBuffer m_Buffer{};
        std::vector<float> m_RetainedData{};
        std::vector<VkVertexInputBindingDescription> m_BindingDesc{};
        std::vector<VkVertexInputAttributeDescription>  m_AttributeDesc{};

        // TODO: implement usage of dynamic vertex buffers layout specification. See vkDynamicVertexEXT()
        bool m_UseDynamicVertexBufferLayout{ false };
    };
}

#endif // MIKOTO_VULKAN_VERTEX_BUFFER_HH
