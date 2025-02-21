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
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>

#include "Common/Common.hh"
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanVertexBuffer.hh"

namespace Mikoto {
    VulkanVertexBuffer::VulkanVertexBuffer(const VertexBufferCreateInfo& createInfo)
        :   VertexBuffer{ createInfo.Layout }
    {
#ifdef VULKAN_EXTENDED_DYNAMIC_EXTENSION
        SetBindingDescriptions();
        SetAttributeDescriptions();
#endif

        if (createInfo.RetainData) {
            m_RetainedData = createInfo.Data;
        }

        SetVertexData(createInfo.Data);
    }

    auto VulkanVertexBuffer::Bind( const VkCommandBuffer commandBuffer ) const -> void {
        const std::array buffers{ m_Buffer->Get() };
        constexpr std::array<VkDeviceSize, 1> offsets{};

        vkCmdBindVertexBuffers( commandBuffer, 0, 1, buffers.data(), offsets.data() );
    }

    VulkanVertexBuffer::~VulkanVertexBuffer() {
        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }

#ifdef VULKAN_EXTENDED_DYNAMIC_EXTENSION
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
#endif

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
        s_AttributeDesc = std::vector<VkVertexInputAttributeDescription>( s_DefaultBufferLayout.GetCount() );

        /**
         * The binding parameter tells Vulkan from which binding the per-vertex data comes.
         * The location parameter references the location directive of the input in the vertex shader.
         * The input in the vertex shader with location 0 is the position, which has two 32-bit float
         * components. The format parameter describes the type of data for the attribute
         *
         * See: https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description
         * */

        // The index refers to how the vertex attributes are laid out according to s_DefaultBufferLayout
        // so index 0 -> s_DefaultBufferLayout first attribute,
        // index 1 -> s_DefaultBufferLayout second attribute and so on
        for (Size_T index{}; index < s_AttributeDesc.size(); ++index) {
            s_AttributeDesc[index] = {};
            s_AttributeDesc[index].binding = 0;
            s_AttributeDesc[index].location = index;
            s_AttributeDesc[index].format = VulkanHelpers::GetVulkanAttributeDataType( s_DefaultBufferLayout[index].GetType() );
            s_AttributeDesc[index].offset = s_DefaultBufferLayout[index].GetOffset();
        }

        return s_AttributeDesc;
    }

    auto VulkanVertexBuffer::Release() -> void {
        m_Buffer = nullptr;
        m_RetainedData.clear();
    }

    auto VulkanVertexBuffer::SetVertexData(const std::vector<float>& vertices) -> void {

        m_Count = vertices.size();
        m_Size = m_Count * sizeof(float);

        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        // Create staging buffer with VK_BUFFER_USAGE_TRANSFER_SRC_BIT usage flag.
        // This flag tells Vulkan that this buffer will only be used as a source for transfer commands.
        // We won’t be using the staging buffer for rendering.

        // allocate staging buffer
        VkBufferCreateInfo stagingBufferInfo{ VulkanHelpers::Initializers::BufferCreateInfo() };
        stagingBufferInfo.pNext = nullptr;

        stagingBufferInfo.size = m_Size;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        //let the VMA library know that this data should be on CPU RAM
        VmaAllocationCreateInfo vmaStagingAllocationCreateInfo{};
        vmaStagingAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vmaStagingAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        const VulkanBufferCreateInfo stagingBufferBufferCreateInfo{
            .BufferCreateInfo{ stagingBufferInfo },
            .AllocationCreateInfo{ vmaStagingAllocationCreateInfo },
            .WantMapping{ true }
        };

        Scope_T<VulkanBuffer> stagingBuffer{ VulkanBuffer::Create( stagingBufferBufferCreateInfo ) };

        // Copy vertex data to staging buffer
        std::memcpy(stagingBuffer->GetVmaAllocationInfo().pMappedData, vertices.data(), stagingBuffer->GetSize());

        stagingBuffer->PersistentUnmap();

        // Allocate vertex buffer
        VkBufferCreateInfo vertexBufferInfo{ VulkanHelpers::Initializers::BufferCreateInfo() };
        vertexBufferInfo.pNext = nullptr;

        //this is the total size, in bytes, of the buffer we are allocating (same as staging buffer)
        vertexBufferInfo.size = m_Size;

        // This buffer is going to be used as a Vertex buffer, and we are going to transfer data to it
        vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        // Let the VMA library know that this data should be GPU native
        VmaAllocationCreateInfo vmaAllocationCreateInfo{};
        vmaAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vmaAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        vmaAllocationCreateInfo.priority = 1.0f;

        // Allocate the buffer
        const VulkanBufferCreateInfo vertexBufferCreateInfo{
            .BufferCreateInfo{ vertexBufferInfo },
            .AllocationCreateInfo{ vmaAllocationCreateInfo },
            .WantMapping{ false }
        };

        m_Buffer = VulkanBuffer::Create( vertexBufferCreateInfo );

        VulkanContext::Get().ImmediateSubmit([&]( const VkCommandBuffer cmd) -> void {
            VkBufferCopy copy{
                .srcOffset{ 0 },
                .dstOffset{ 0 },
                .size{ m_Size },
            };

            vkCmdCopyBuffer(cmd, stagingBuffer->Get(), m_Buffer->Get(), 1, std::addressof(copy));
        });

        stagingBuffer = nullptr;
    }
}