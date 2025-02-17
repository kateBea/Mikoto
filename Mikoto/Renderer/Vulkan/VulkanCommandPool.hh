/**
 * VulkanCommandPool.hh
 * Created by kate on 7/4/2023.
 * */

#ifndef MIKOTO_VULKAN_COMMAND_POOL_HH
#define MIKOTO_VULKAN_COMMAND_POOL_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Library/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanObject.hh>

namespace Mikoto {

    struct VulkanCommandPoolCreateInfo {
        VkCommandPoolCreateInfo CreateInfo{};
    };

    class VulkanCommandPool final : public VulkanObject {
    public:
        explicit VulkanCommandPool( const VulkanCommandPoolCreateInfo& createInfo);

        auto Get() -> VkCommandPool& { return m_CommandPool; }
        auto GetCreateInfo() -> VkCommandPoolCreateInfo& { return m_CreateInfo; }

        auto Release() -> void override;

        MKT_NODISCARD static auto Create(const VulkanCommandPoolCreateInfo& createInfo) -> Scope_T<VulkanCommandPool>;

        ~VulkanCommandPool() override;

        DISABLE_COPY_AND_MOVE_FOR(VulkanCommandPool);

    private:
        VkCommandPool m_CommandPool{ VK_NULL_HANDLE };
        VkCommandPoolCreateInfo m_CreateInfo{};
    };
}


#endif // MIKOTO_VULKAN_COMMAND_POOL_HH
