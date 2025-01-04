/**
 * VulkanVertexBuffer.cc
 * Created by kate on 6/17/23.
 * */

// C++ Standard Library
#include <array>
#include <utility>
#include <stdexcept>
#include <cstring>

// Third-Party Libraries
#include "volk.h"

// Project Headers
#include <Renderer/Vulkan/DeletionQueue.hh>

#include "Common/Common.hh"
#include "Models/VertexBufferCreateInfo.hh"
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanVertexBuffer.hh"

namespace Mikoto {
    VulkanVertexBuffer::VulkanVertexBuffer(VertexBufferCreateInfo &&createInfo)
        :   VertexBuffer{ std::move(createInfo.Layout) }
    {
        SetBindingDescriptions();
        SetAttributeDescriptions();

        if (createInfo.RetainData)
            m_RetainedData = createInfo.Data;

        m_Layout = createInfo.Layout;

        SetVertexData(createInfo.Data);
        DeletionQueue::Push([bufferHandle = m_Buffer.Get(), allocation = m_Buffer.GetVmaAllocation()]() -> void {
            vmaDestroyBuffer(VulkanContext::GetDefaultAllocator(), bufferHandle, allocation);
        });
    }

    auto VulkanVertexBuffer::Bind(VkCommandBuffer commandBuffer) const -> void {
        std::array<VkBuffer, 1> buffers{ m_Buffer.Get() };
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
        // FIXME: sign comparison m_Layout.GetCount() should return a Size_T (this should generally be the case unless stuff like vertex buffer since there can be many they would be UInt64's instead)
        for (Size_T index{}; index < m_Layout.GetCount(); ++index) {
            m_AttributeDesc[index].location = index;
            m_AttributeDesc[index].format = VulkanUtils::GetVulkanAttributeDataType(m_Layout[index].GetType());
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
        s_AttributeDesc[0].format = VulkanUtils::GetVulkanAttributeDataType(s_DefaultBufferLayout[0].GetType());
        s_AttributeDesc[0].offset = s_DefaultBufferLayout[0].GetOffset();

        // Normal
        s_AttributeDesc[1] = {};
        s_AttributeDesc[1].binding = 0;
        s_AttributeDesc[1].location = 1;
        s_AttributeDesc[1].format = VulkanUtils::GetVulkanAttributeDataType(s_DefaultBufferLayout[1].GetType());
        s_AttributeDesc[1].offset = s_DefaultBufferLayout[1].GetOffset();

        // Color
        s_AttributeDesc[2] = {};
        s_AttributeDesc[2].binding = 0;
        s_AttributeDesc[2].location = 2;
        s_AttributeDesc[2].format = VulkanUtils::GetVulkanAttributeDataType(s_DefaultBufferLayout[2].GetType());
        s_AttributeDesc[2].offset = s_DefaultBufferLayout[2].GetOffset();

        // Texture Coordinates
        s_AttributeDesc[3] = {};
        s_AttributeDesc[3].binding = 0;
        s_AttributeDesc[3].location = 3;
        s_AttributeDesc[3].format = VulkanUtils::GetVulkanAttributeDataType(s_DefaultBufferLayout[3].GetType());
        s_AttributeDesc[3].offset = s_DefaultBufferLayout[3].GetOffset();

        return s_AttributeDesc;
    }

    auto VulkanVertexBuffer::SetVertexData(const std::vector<float>& vertices) -> void {
        m_Count = vertices.size();
        m_Size = m_Count * sizeof(float);

        auto& vmaAllocator{ VulkanContext::GetDefaultAllocator() };

        // Create staging buffer with VK_BUFFER_USAGE_TRANSFER_SRC_BIT usage flag.
        // This flag tells Vulkan that this buffer will only be used as a source for transfer commands.
        // We won’t be using the staging buffer for rendering.

        // allocate staging buffer
        VkBufferCreateInfo stagingBufferInfo{ VulkanUtils::Initializers::BufferCreateInfo() };
        stagingBufferInfo.pNext = nullptr;

        stagingBufferInfo.size = m_Size;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        //let the VMA library know that this data should be on CPU RAM
        VmaAllocationCreateInfo vmaStagingAllocationCreateInfo{};
        vmaStagingAllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        BufferAllocateInfo stagingBuffer{};

        // Allocate staging buffer
        if (vmaCreateBuffer(vmaAllocator,
                            std::addressof(stagingBufferInfo),
                            std::addressof(vmaStagingAllocationCreateInfo),
                            std::addressof(stagingBuffer.Buffer),
                            std::addressof(stagingBuffer.Allocation),
                            nullptr) != VK_SUCCESS)
        {
            MKT_THROW_RUNTIME_ERROR("Failed to create VMA staging buffer for Vulkan vertex buffer");
        }

        // Copy vertex data to staging buffer
        void* stagingBufferData{};
        vmaMapMemory(vmaAllocator, stagingBuffer.Allocation, std::addressof(stagingBufferData));
        std::memcpy(stagingBufferData, static_cast<const void*>(vertices.data()), m_Size);
        vmaUnmapMemory(vmaAllocator, stagingBuffer.Allocation);


        // Create the actual GPU side buffer

        // Allocate vertex buffer
        VkBufferCreateInfo vertexBufferInfo{ VulkanUtils::Initializers::BufferCreateInfo() };
        vertexBufferInfo.pNext = nullptr;

        //this is the total size, in bytes, of the buffer we are allocating (same as staging buffer)
        vertexBufferInfo.size = m_Size;

        // This buffer is going to be used as a Vertex buffer, and we are going to transfer data to it
        vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        // Let the VMA library know that this data should be GPU native
        VmaAllocationCreateInfo vmaAllocationCreateInfo{};
        vmaAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // Allocate the buffer
        auto& bufferAllocationInfo{ m_Buffer.GetAllocationInfo() };

        if (vmaCreateBuffer(vmaAllocator,
                            std::addressof(vertexBufferInfo),
                            std::addressof(vmaAllocationCreateInfo),
                            std::addressof(bufferAllocationInfo.Buffer),
                            std::addressof(bufferAllocationInfo.Allocation),
                            nullptr) != VK_SUCCESS)
        {
            MKT_THROW_RUNTIME_ERROR("Failed to create VMA staging buffer for Vulkan vertex buffer");
        }

        VulkanContext::ImmediateSubmit([&](VkCommandBuffer cmd) -> void {
            VkBufferCopy copy{};
            copy.dstOffset = 0;
            copy.srcOffset = 0;
            copy.size = m_Size;

            vkCmdCopyBuffer(cmd, stagingBuffer.Buffer, bufferAllocationInfo.Buffer, 1, std::addressof(copy));
        });

        // Delete staging buffer as we are done with it
        vmaDestroyBuffer(vmaAllocator, stagingBuffer.Buffer, stagingBuffer.Allocation);
    }
}