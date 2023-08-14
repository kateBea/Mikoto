//
// Created by kate on 7/3/23.
//

#ifndef KATE_ENGINE_VULKAN_INDEX_BUFFER_HH
#define KATE_ENGINE_VULKAN_INDEX_BUFFER_HH

#include <memory>
#include <vector>

#include <volk.h>

#include <Renderer/Buffers/IndexBuffer.hh>

namespace Mikoto {
    class VulkanIndexBuffer : public IndexBuffer {
    public:
        explicit VulkanIndexBuffer(const std::vector<UInt32_T>& indices);
        auto Submit(VkCommandBuffer commandBuffer) const -> void;

        auto Bind() const -> void override {}
        auto Unbind() const -> void override {}

        auto OnRelease() const -> void;

    private:
        auto CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) -> void;
        auto SetIndicesData(const std::vector<UInt32_T> &indices) -> void;
    private:
        VkBuffer                        m_IndexBuffer{};
        VkDeviceMemory                  m_IndexBufferMemory{};
    };
}



#endif //KATE_ENGINE_VULKAN_INDEX_BUFFER_HH
