/**
 * VulkanCommandPool.hh
 * Created by kate on 7/4/2023.
 * */

#ifndef MIKOTO_VULKAN_COMMAND_POOL_HH
#define MIKOTO_VULKAN_COMMAND_POOL_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <STL/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanObject.hh>

namespace Mikoto {
    class VulkanCommandPool final : public VulkanObject {
    public:
        explicit VulkanCommandPool( const VkCommandPoolCreateInfo& createInfo );

        auto Get() const -> VkCommandPool { return m_CommandPool; }
        auto Release() -> void override;

        static auto Create(const VkCommandPoolCreateInfo& createInfo) -> Ref_T<VulkanCommandPool>;

        ~VulkanCommandPool() override {
            Release();
        }

        DISABLE_COPY_AND_MOVE_FOR(VulkanCommandPool);
    private:
        VkCommandPool m_CommandPool{ VK_NULL_HANDLE };
        VkCommandPoolCreateInfo m_CreateInfo{};
    };
}


#endif // MIKOTO_VULKAN_COMMAND_POOL_HH
