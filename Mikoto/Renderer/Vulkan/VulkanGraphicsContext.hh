//    Copyright 2025 ケイト
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

#ifndef MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
#define MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH

#include <utility>

#include <volk.h>
#include <ankerl/unordered_dense.h>

#include <Assets/Texture.hh>

#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>

namespace Mikoto {

    class VulkanGraphicsContext final : public GraphicsContext {
    public:
        explicit  VulkanGraphicsContext(GpuDevice* device);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginPass(FramePass* pass) -> void override;
        auto EndPass(FramePass* pass) -> void override;

        auto BeginFrame(FrameBlackboard* blackboard)-> void override;
        auto EndFrame()-> void  override;

        auto PushImage(TextureHandle texture) -> Int32 override;

        auto BindTextureList(CommandListHandle cmdList) -> void override;
        auto BindPassResources(FramePass* pass, CommandListHandle cmdList ) -> void override;
        auto CommitShaderResources(FramePass* pass ) -> void override;

        auto CreatePipelineResources(FramePass* pass, PipelineHandle pipeline) -> void override;

        auto GetPassSRG( FramePass* pass ) -> SRGPerPass* override;

        auto InsertResourceBarrier(FramePass* pass, CommandListHandle cmdList ) -> void override;

        ~VulkanGraphicsContext() override = default;

    private:
        MKT_NODISCARD auto HasDescriptorSets(FramePass* pipeline) -> bool;

        auto CreatePassDescriptors(FramePass* pass) -> void;
        auto UpdatePassDescriptors(FramePass* pass) -> void;

        auto BindPassDescriptors(FramePass* pass, CommandListHandle cmdList) -> void;

        auto PushBuffer( FramePass* pass, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType) -> void;
        auto PushImage( FramePass* pass, std::string_view textureName, std::string_view samplerName, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType) -> void;

        auto CreateBindlessTexturesSet() -> void;
        auto UpdateBindlessTexturesSet(Texture* texture, Sampler* sampler, Size setIndex ) const -> void;

        auto InitSharedShaderResourceGroups() -> void;

    private:
        static constexpr UInt32 TEXTURES_DESCRIPTOR_SET_INDEX{ 0 };
        static constexpr UInt32 PER_FRAME_DESCRIPTOR_SET_INDEX{ 1 };
        static constexpr UInt32 PER_PASS_DESCRIPTOR_SET_INDEX{ 2 };

    private:
        // Information I store for each pass
        struct FramePassInfo {
            // Set index -> Descriptor Set handle
            ankerl::unordered_dense::map<UInt32, VkDescriptorSet> DescriptorSets{};

            SRGPerPass PassResources{};

            PipelineHandle Pipeline{};

            // Whether we need to update the descriptor sets or not
            bool Dirty{ true };
        };

        FrameBlackboard* m_Blackboard{};

#if defined( MKT_USE_VULKAN_BINDLESS )
        DescriptorSetLayoutHandle m_LayoutTextures{};
        VkDescriptorSet m_BindlessTexturesSet{};
        VkPipelineLayout m_TexturesPipelineLayout{};
        bool m_UpdateTextureDescriptor{ false };
#endif
    private:
        ankerl::unordered_dense::map<FramePass*, FramePassInfo> m_PassInfo{};
    };
}// namespace Mikoto


#endif//MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
