/**
 * ImGuiVulkanBackend.hh
 * Created by kate on 9/14/23.
 * */

#ifndef MIKOTO_IMGUI_VULKAN_BACKEND_HH
#define MIKOTO_IMGUI_VULKAN_BACKEND_HH

// C++ Standard Library
#include <any>
#include <memory>
#include <vector>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <ImGui/ImGuiUtils.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>

namespace Mikoto {
    class ImGuiVulkanBackend : public BackendImplementation {
    public:
        auto Init(std::any windowHandle) -> void override;
        auto ShutDown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

    private:
        auto CreateImGuiRenderPass() -> void;
        auto CreateImGuiCommandPool() -> void;
        auto CreateImGuiCommandBuffers() -> void;

        auto BuildCommandBuffers() -> void;

        static auto AcquireNextSwapChainImage(UInt32_T& imageIndex) -> VkResult;
        auto RecordImGuiCommandBuffers(UInt32_T imageIndex) -> void;
        auto SubmitImGuiCommandBuffers(UInt32_T& imageIndex) -> VkResult;

    private:
        /*************************************************************
        * VULKAN MEMBERS
        * ***********************************************************/
        VkRenderPass m_ImGuiRenderPass{};
        VkDescriptorPool m_ImGuiDescriptorPool{};

        std::shared_ptr<VulkanCommandPool> m_CommandPool{};
        std::vector<VkCommandBuffer> m_ImGuiCommandBuffers{};

    };
}


#endif // MIKOTO_IMGUI_VULKAN_BACKEND_HH
