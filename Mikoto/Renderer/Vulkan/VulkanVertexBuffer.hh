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
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Buffer/VertexBuffer.hh>

namespace Mikoto {
    class VulkanVertexBuffer final : public VertexBuffer, public VulkanObject {
    public:
        explicit VulkanVertexBuffer(const VertexBufferCreateInfo& createInfo);

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

#ifdef VULKAN_EXTENDED_DYNAMIC_EXTENSION
        auto SetBindingDescriptions() -> void;
        auto SetAttributeDescriptions() -> void;
        MKT_NODISCARD auto GetBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>& { return m_BindingDesc; }
        MKT_NODISCARD auto GetAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>& { return m_AttributeDesc; }
#endif

        MKT_NODISCARD static auto GetDefaultBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>&;
        MKT_NODISCARD static auto GetDefaultAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>&;

        auto Release() -> void override;

        ~VulkanVertexBuffer() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanVertexBuffer);

    private:
        auto SetVertexData(const std::vector<float>& vertices) -> void;

    private:
#ifndef VULKAN_EXTENDED_DYNAMIC_EXTENSION
        inline static std::vector<VkVertexInputBindingDescription> s_BindingDesc{};
        inline static std::vector<VkVertexInputAttributeDescription> s_AttributeDesc{};
#endif

        Scope_T<VulkanBuffer> m_Buffer{};
        std::vector<float> m_RetainedData{};

#ifdef VULKAN_EXTENDED_DYNAMIC_EXTENSION
        // TODO: implement usage of dynamic vertex buffers layout specification. See vkDynamicVertexEXT()
        bool m_UseDynamicVertexBufferLayout{ false };
        std::vector<VkVertexInputBindingDescription> m_BindingDesc{};
        std::vector<VkVertexInputAttributeDescription>  m_AttributeDesc{};
#endif

    };
}

#endif // MIKOTO_VULKAN_VERTEX_BUFFER_HH
