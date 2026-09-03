//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_IMGUI_VULKAN_BACKEND_HH
#define MIKOTO_IMGUI_VULKAN_BACKEND_HH

#include <EASTL/memory.h>
#include <EASTL/vector.h>

#include <volk.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <ImGui/ImGuiService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>

namespace mikoto::imgui {

    // Since Mikoto defaults to 1.3
    // ImGuiVulkanBackend wil be using Dynamic Rendering by default
    // (already supported by ImGui)

    class ImGuiVulkanBackend final : public ImGuiBackend {
    public:
        explicit ImGuiVulkanBackend( const ImGuiBackendCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

        MKT_NODISCARD auto GetFinalComposition() -> renderer::rhi::TextureHandle override;

        MKT_NODISCARD auto ConstructImGuiTextureID( const renderer::rhi::ITexture* texture ) -> ImTextureID override;
        MKT_NODISCARD auto ConstructImGuiTextureID( renderer::rhi::TextureHandle texture ) -> ImTextureID override;

    private:
        auto InitImGuiForVulkan() -> void;
        auto CreateImages() -> void;

        auto RecordRenderCommands() -> void;
        auto RecordViewportState() -> void;

    private:
        VkExtent3D mDimensions{ 2560, 1440, 1 };

        renderer::rhi::TextureHandle mColorImage{};
        renderer::rhi::TextureHandle mDepthImage{};
        renderer::rhi::CommandListHandle mCommandList{};

        struct ImGuiTextIDInfo {
            VkDescriptorSet descriptorSet{};
        };
        ankerl::unordered_dense::map<const renderer::rhi::ITexture*, ImGuiTextIDInfo> mImGuiSets{};

        VkDescriptorPool mImGuiDescriptorPool{};
    };
}// namespace Mikoto


#endif // MIKOTO_IMGUI_VULKAN_BACKEND_HH
