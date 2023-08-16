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
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>

namespace Mikoto {
    class VulkanIndexBuffer : public IndexBuffer {
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

        /**
         * Releases all the resources held by this index buffer
         * */
        auto OnRelease() const -> void;

    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        auto SetIndicesData(const std::vector<UInt32_T> &indices) -> void;
    private:
        /*************************************************************
        * MEMBER VARIABLES
        * ***********************************************************/
        VulkanBuffer m_Buffer{};
    };
}



#endif // MIKOTO_VULKAN_INDEX_BUFFER_HH
