/**
 * VulkanVertexBuffer.cc
 * Created by kate on 6/17/23.
 * */

// C++ Standard Library
#include <array>
#include <memory>
#include <utility>
#include <stdexcept>
#include <cstring>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>

namespace Mikoto {
    VulkanVertexBuffer::VulkanVertexBuffer(const VertexBufferCreateInfo& createInfo)
        :   m_Layout{ createInfo.Layout }
    {
        SetBindingDescriptions();
        SetAttributeDescriptions();

        if (createInfo.RetainData)
            m_RetainedData = createInfo.Data;

        SetVertexData(createInfo.Data);
    }

    auto VulkanVertexBuffer::Submit(VkCommandBuffer commandBuffer) const -> void {
        std::array<VkBuffer, 1> buffers{ m_AllocationInfo.Buffer };
        std::array<VkDeviceSize, 1> offsets{ 0 };

        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers.data(), offsets.data());
    }

    auto VulkanVertexBuffer::SetBindingDescriptions() -> void {
        m_BindingDesc = std::vector<VkVertexInputBindingDescription>(1);
        m_BindingDesc[0].binding = 0;
        m_BindingDesc[0].stride = m_Layout.GetStride();
        m_BindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }

    auto VulkanVertexBuffer::SetAttributeDescriptions() -> void {
        m_AttributeDesc = std::vector<VkVertexInputAttributeDescription>(m_Layout.GetCount());

        // Setup attribute binding
        for (auto attribute : m_AttributeDesc)
            attribute.binding = 0;

        // Setup location, format and offset
        for (Size_T index{}; index < m_Layout.GetCount(); ++index) {
            m_AttributeDesc[index].location = index;
            m_AttributeDesc[index].format = m_Layout[index].GetVulkanAttributeDataType();
            m_AttributeDesc[index].offset = m_Layout[index].GetOffset();
        }
    }

    auto VulkanVertexBuffer::GetDefaultBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>& {
        // All of our per-vertex data is packed together in one array, so we're only going to have one binding.
        // See: https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description

        s_BindingDesc = std::vector<VkVertexInputBindingDescription>(1);

        s_BindingDesc[0] = {};
        s_BindingDesc[0].binding = 0;
        s_BindingDesc[0].stride = s_DefaultBufferLayout.GetStride();
        s_BindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // not using instanced rendering, so we'll stick to per-vertex data.

        return s_BindingDesc;
    }

    auto VulkanVertexBuffer::GetDefaultAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>& {
        s_AttributeDesc = std::vector<VkVertexInputAttributeDescription>(s_DefaultBufferLayout.GetCount());

        /**
         * The binding parameter tells Vulkan from which binding the per-vertex data comes.
         * The location parameter references the location directive of the input in the vertex shader.
         * The input in the vertex shader with location 0 is the position, which has two 32-bit float
         * components. The format parameter describes the type of data for the attribute
         *
         * See: https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description
         * */
        auto layout{ s_DefaultBufferLayout };

        // Position
        s_AttributeDesc[0] = {};
        s_AttributeDesc[0].binding = 0;
        s_AttributeDesc[0].location = 0;
        s_AttributeDesc[0].format = s_DefaultBufferLayout[0].GetVulkanAttributeDataType();
        s_AttributeDesc[0].offset = s_DefaultBufferLayout[0].GetOffset();

        // Normal
        s_AttributeDesc[1] = {};
        s_AttributeDesc[1].binding = 0;
        s_AttributeDesc[1].location = 1;
        s_AttributeDesc[1].format = s_DefaultBufferLayout[1].GetVulkanAttributeDataType();
        s_AttributeDesc[1].offset = s_DefaultBufferLayout[1].GetOffset();

        // Color
        s_AttributeDesc[2] = {};
        s_AttributeDesc[2].binding = 0;
        s_AttributeDesc[2].location = 2;
        s_AttributeDesc[2].format = s_DefaultBufferLayout[2].GetVulkanAttributeDataType();
        s_AttributeDesc[2].offset = s_DefaultBufferLayout[2].GetOffset();

        // Texture Coordinates
        s_AttributeDesc[3] = {};
        s_AttributeDesc[3].binding = 0;
        s_AttributeDesc[3].location = 3;
        s_AttributeDesc[3].format = s_DefaultBufferLayout[3].GetVulkanAttributeDataType();
        s_AttributeDesc[3].offset = s_DefaultBufferLayout[3].GetOffset();

        return s_AttributeDesc;
    }

    auto VulkanVertexBuffer::SetVertexData(const std::vector<float>& vertices) -> void {
        m_Count = vertices.size();
        m_AllocationInfo.Size = m_Count * sizeof(float);
        m_Size = m_AllocationInfo.Size;

        VulkanUtils::UploadBuffer(m_AllocationInfo);

        // Copy data to CPU readable memory
        void* data{};
        vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_AllocationInfo.Allocation, &data);
        std::memcpy(data, (const void*)vertices.data(), m_AllocationInfo.Size);
        vmaUnmapMemory(VulkanContext::GetDefaultAllocator(), m_AllocationInfo.Allocation);
    }

    auto VulkanVertexBuffer::OnRelease() const -> void {
        vmaDestroyBuffer(VulkanContext::GetDefaultAllocator(), m_AllocationInfo.Buffer, m_AllocationInfo.Allocation);
    }
}