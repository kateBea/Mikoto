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
#include "volk.h"

// Project Headers
#include "ImGuiUtils.hh"
#include "Renderer/Vulkan/VulkanCommandPool.hh"

namespace Mikoto {
    class ImGuiVulkanBackend : public BackendImplementation {
    public:
        auto Init(std::any functionName) -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

    private:
        auto CreateImGuiRenderPass() -> void;

        auto BuildCommandBuffers() -> void;
        auto RecordImGuiCommandBuffers(UInt32_T imageIndex) -> void;

    private:
        VkRenderPass m_ImGuiRenderPass{};
        VkDescriptorPool m_ImGuiDescriptorPool{};
    };
}


#endif // MIKOTO_IMGUI_VULKAN_BACKEND_HH
