/**
 * VulkanIndexBuffer
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_INDEX_BUFFER_HH
#define MIKOTO_VULKAN_INDEX_BUFFER_HH

// C++ Standard Library
#include <memory>
#include <vector>

// Third-Party Libraries
#include "volk.h"

// Project Headers
#include <Renderer/Buffer/IndexBuffer.hh>

#include "Common/Common.hh"
#include "Renderer/Vulkan/VulkanUtils.hh"
#include "Renderer/Buffer/IndexBuffer.hh"
#include "VulkanBuffer.hh"

namespace Mikoto {
    class VulkanIndexBuffer final : public IndexBuffer {
    public:
        /**
         * Creates and initializes this index buffer.
         * */
        explicit VulkanIndexBuffer(const std::vector<UInt32_T>& indices);

        /**
         * Binds this index buffer to the given command buffer
         * @param commandBuffer command buffer to bind the implicit parameter to
         * */
        auto Bind(VkCommandBuffer commandBuffer) const -> void;

    private:
        auto SetIndicesData(const std::vector<UInt32_T> &indices) -> void;

    private:
        VulkanBuffer m_Buffer{};
    };
}



#endif // MIKOTO_VULKAN_INDEX_BUFFER_HH
