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

#ifndef MIKOTO_VULKAN_CONTEXT_HH
#define MIKOTO_VULKAN_CONTEXT_HH

#include <volk.h>
#include <vk_mem_alloc.h>

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/RenderContext.hh>

#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanInstance.hh>
#include <Renderer/Vulkan/VulkanSwapchain.hh>


namespace mikoto::renderer::vulkan {

    // Reference: https://github.com/nvpro-samples/vk_minimal_latest
    class Context final : public RenderContext, public Singleton<Context> {
    public:
        explicit Context( const RenderContextCreateInfo& createInfo )
            : RenderContext{ createInfo } {}

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto SetRefreshRate( RefreshRate rate ) -> void override;
        auto SetPresentTarget( TextureHandle texture ) -> void override;

        auto Present() -> void override;
        auto SubmitFrame() -> void override;
        auto PrepareFrame() -> void override;

        auto Update() -> void override;

        // Vulkan Specifics
        MKT_NODISCARD auto GetMaxFramesInFlight() const -> u32;
        MKT_NODISCARD auto GetCurrentImageIndex() const -> u32;
        MKT_NODISCARD auto GetCurrentFrameIndex() const -> u32;

        MKT_NODISCARD auto GetInstance() -> Instance&;
        MKT_NODISCARD auto GetInstance() const -> const Instance&;

        MKT_NODISCARD auto GetSwapchain() -> SwapChainHandle;

        MKT_NODISCARD auto GetApiVersion() const -> u32;

    private:
        auto CreateSwapchain() -> void;

        auto InitSwapchainRender() -> void;
        auto InitSynchronization() -> void;

    private:
        static constexpr u32 kVersionMajor{ 1 };
        static constexpr u32 kVersionMinor{ 3 };
        static constexpr u32 kVersionPatch{ 0 };

        static constexpr u32 kMaxFramesInFlight{ 3 };

        TextureHandle mPresentTarget{};
        bool mTableUpdateRequired{ false };

        SwapChainHandle mSwapchain{};

        // Current frame
        u32 mCurrentFrameIndex{};
        u32 mCurrentImageIndex{};
        u32 mMaxFramesInFlight{};

        eastl::unique_ptr<Instance> mInstance{};

        struct FrameContext {
            u64 mSubmissionID{};
            SemaphoreHandle mImageAvailableSemaphore{};
            SemaphoreHandle mRenderFinishedSemaphore{};
        };

        CommandListHandle mCommandList{};

        eastl::fixed_vector<FrameContext, kMaxFramesInFlight> mFrames{};

        // Swapchain blit objects
        renderer::rhi::SamplerHandle mSamplerState{};

        renderer::rhi::ShaderModuleHandle mVertexShader{};
        renderer::rhi::ShaderModuleHandle mPixelShader{};

        renderer::rhi::PipelineHandle mPipeline{};
        renderer::rhi::BindingSetHandle mBindingSetHandle{};
        renderer::rhi::BindingLayoutHandle mBindlessLayout{};
        renderer::rhi::DescriptorTableHandle mDescriptorTable{};
        renderer::rhi::BindingLayoutHandle mBindingLayoutHandle{};
        renderer::rhi::PipelineLayoutHandle mPipelineLayoutHandle{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_VULKAN_CONTEXT_HH