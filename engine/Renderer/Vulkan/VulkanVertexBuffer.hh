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
#include <glm/glm.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>

namespace Mikoto {
    class VulkanVertexBuffer : public VertexBuffer {
    public:
        explicit VulkanVertexBuffer(const VertexBufferCreateInfo& createInfo);

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto GetBufferLayout() const -> const BufferLayout& override { return m_Layout; }
        auto SetBufferLayout(const BufferLayout& layout) -> void override { m_Layout = layout; }

        MKT_NODISCARD auto GetBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>& { return m_BindingDesc; }
        MKT_NODISCARD auto GetAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>& { return m_AttributeDesc; }

        auto SetBindingDescriptions() -> void;
        auto SetAttributeDescriptions() -> void;

        static auto GetDefaultBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>&;
        static auto GetDefaultAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>&;

        auto OnRelease() const -> void override;

        ~VulkanVertexBuffer() override = default;
    public:
        /*************************************************************
        * FORBIDDEN OPERATIONS
        * ***********************************************************/
        VulkanVertexBuffer(const VulkanVertexBuffer&) = delete;
        auto operator=(const VulkanVertexBuffer&) -> VulkanVertexBuffer& = delete;

        VulkanVertexBuffer(VulkanVertexBuffer&&) = delete;
        auto operator=(VulkanVertexBuffer&&) -> VulkanVertexBuffer& = delete;
    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        auto SetVertexData(const std::vector<float>& vertices) -> void;

    private:
        /*************************************************************
        * STATIC MEMBERS
        * ***********************************************************/
        inline static std::vector<VkVertexInputBindingDescription> s_BindingDesc{};
        inline static std::vector<VkVertexInputAttributeDescription> s_AttributeDesc{};

    private:
        /*************************************************************
        * MEMBER VARIABLES
        * ***********************************************************/
        BufferLayout m_Layout{};
        VulkanBuffer m_Buffer{};
        std::vector<float> m_RetainedData{};
        std::vector<VkVertexInputBindingDescription> m_BindingDesc{};
        std::vector<VkVertexInputAttributeDescription>  m_AttributeDesc{};
    };
}


#endif // MIKOTO_VULKAN_VERTEX_BUFFER_HH
