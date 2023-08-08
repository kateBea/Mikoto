/**
* VulkanVertexBuffer.hh
* Created by kate on 6/17/23.
* */

#ifndef KATE_ENGINE_VULKAN_VERTEX_BUFFER_HH
#define KATE_ENGINE_VULKAN_VERTEX_BUFFER_HH

// C++ Standard Library
#include <memory>
#include <vector>

// Third-Party Libraries
#include <volk.h>
#include <glm/glm.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
#include <Renderer/Buffers/VertexBuffer.hh>

namespace kaTe {
    class VulkanVertexBuffer : public VertexBuffer {
    public:
        explicit VulkanVertexBuffer(const VertexBufferCreateInfo& createInfo);

        auto Submit(VkCommandBuffer commandBuffer) const -> void;

        KT_NODISCARD auto GetBufferLayout() const -> const BufferLayout& override { return m_Layout; }
        auto SetBufferLayout(const BufferLayout& layout) -> void override { m_Layout = layout; }

        KT_NODISCARD auto GetBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>& { return m_BindingDesc; }
        KT_NODISCARD auto GetAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>& { return m_AttributeDesc; }

        auto SetBindingDescriptions() -> void;
        auto SetAttributeDescriptions() -> void;

        static auto GetDefaultBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>&;
        static auto GetDefaultAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>&;

        auto OnRelease() -> void override;

        ~VulkanVertexBuffer() override = default;
    public:
        VulkanVertexBuffer(const VulkanVertexBuffer&) = delete;
        auto operator=(const VulkanVertexBuffer&) -> VulkanVertexBuffer& = delete;

        VulkanVertexBuffer(VulkanVertexBuffer&&) = delete;
        auto operator=(VulkanVertexBuffer&&) -> VulkanVertexBuffer& = delete;
    private:
        auto SetVertexData(const std::vector<float>& vertices) -> void;

    private:
        inline static std::vector<VkVertexInputBindingDescription> s_BindingDesc{};
        inline static std::vector<VkVertexInputAttributeDescription> s_AttributeDesc{};

        BufferLayout m_Layout{};
        AllocatedBuffer m_AllocationInfo{};
        std::vector<VkVertexInputBindingDescription> m_BindingDesc{};
        std::vector<VkVertexInputAttributeDescription>  m_AttributeDesc{};
        std::vector<float> m_RetainedData{};
    };
}


#endif //KATE_ENGINE_VULKAN_VERTEX_BUFFER_HH
