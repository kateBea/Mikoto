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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_CONTEXT_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_CONTEXT_HH

#include <mutex>

#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <vk_mem_alloc.h>
#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/RenderContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanInstance.hh>
#include <Renderer/Rhi/Vulkan/VulkanSwapchain.hh>

namespace mikoto::renderer::vulkan {

    // Reference: https://github.com/nvpro-samples/vk_minimal_latest
    class Context final : public RenderContext, public core::Singleton<Context> {
    public:
        explicit Context( const RenderContextCreateInfo& createInfo );

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto SetRefreshRate( rhi::RefreshRate rate ) -> void override;
        auto SetPresentTarget( rhi::TextureHandle texture ) -> void override;

        auto BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void override;

        auto Present() -> void override;
        auto SubmitFrame() -> void override;
        auto PrepareFrame() -> void override;

        auto Update() -> void override;

        // Vulkan Specifics
        MKT_NODISCARD auto GetMaxFramesInFlight() const -> core::u32;
        MKT_NODISCARD auto GetCurrentImageIndex() const -> core::u32;
        MKT_NODISCARD auto GetCurrentFrameIndex() const -> core::u32;

        MKT_NODISCARD auto GetInstance() -> Instance&;
        MKT_NODISCARD auto GetInstance() const -> const Instance&;

        MKT_NODISCARD auto GetSwapChain() -> VulkanSwapChainHandle;

        MKT_NODISCARD auto GetApiVersion() const -> core::u32;

    private:
        auto InitPresentation() -> void;

        auto InitRenderingObjects() -> void;
        auto InitSynchronization() -> void;

    private:
        static constexpr core::u32 kVersionMajor{ 1 };
        static constexpr core::u32 kVersionMinor{ 3 };
        static constexpr core::u32 kVersionPatch{ 0 };

        static constexpr core::u32 kMaxFramesInFlight{ 2 };

        rhi::TextureHandle mPresentTarget{};

        VulkanSwapChainHandle mSwapchain{};

        // Current frame
        core::u32 mCurrentFrameIndex{};
        core::u32 mCurrentImageIndex{};
        core::u32 mMaxFramesInFlight{};

        eastl::unique_ptr<Instance> mInstance{};

        struct FrameContext {
            core::u64 mFenceValue{};
            rhi::FenceHandle mFence{};

            BinarySemaphoreHandle mImageAvailableSemaphore{};

            // Reuse this descriptor only after the frame fence has completed.
            rhi::TextureHandle mBlitTarget{};
            rhi::DescriptorTableIndex mTextureDescriptorIndex{ rhi::kInvalidBindingItemIndex };
        };

        Queue* mGraphicsQueue{};
        Queue* mVideoDecodeQueue{};
        Queue* mVideoEnCodeQueue{};

        renderer::rhi::CommandListHandle mCommandList{};

        eastl::fixed_vector<FrameContext, kMaxFramesInFlight> mFrameContexts{};

        std::mutex mBatchedSubmissionMutex{};
        // Keep dependency boundaries and push order when logical queues alias.
        eastl::vector<eastl::pair<rhi::QueueType, rhi::SubmitInfo>> mBatchedSubmissions{};

        // SwapChain blit objects
        rhi::SamplerHandle mSamplerState{};

        rhi::ShaderModuleHandle mVertexShader{};
        rhi::ShaderModuleHandle mPixelShader{};
        rhi::PipelineHandle mPipeline{};
        rhi::BindingTableHandle mBindingSetHandle{};
        rhi::BindingLayoutHandle mBindlessLayout{};
        rhi::DescriptorTableHandle mDescriptorTable{};
        rhi::BindingLayoutHandle mBindingLayoutHandle{};
        rhi::PipelineLayoutHandle mPipelineLayoutHandle{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_CONTEXT_HH

