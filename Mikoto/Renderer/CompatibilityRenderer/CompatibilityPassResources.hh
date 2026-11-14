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


#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_PASS_RESOURCES_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_PASS_RESOURCES_HH

#include <EASTL/string_view.h>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/CommandList.hh>

namespace mikoto::renderer {
    inline constexpr rhi::Format kCompatibilitySceneFormat{ rhi::Format::eRGBA16_FLOAT };
    inline constexpr rhi::Format kCompatibilityDepthFormat{ rhi::Format::eD32 };
    inline constexpr rhi::Format kCompatibilityOutputFormat{ rhi::Format::eBGRA8_UNORM };
    inline constexpr core::u32 kCompatibilityFrameSlot{};
    inline constexpr core::u32 kFullscreenTriangleVertexCount{ 3 };

    MKT_NODISCARD auto CreateReusableCompatibilityUniform( rhi::IGpuDevice* device, core::usize size ) -> rhi::BufferHandle;
    MKT_NODISCARD auto WriteCompatibilityUniform(
        rhi::IGpuDevice* device,
        rhi::ICommandList* commands,
        rhi::BufferHandle reusable,
        const void* data, core::usize size ) -> rhi::BufferHandle;

    auto ResizeCompatibilityTarget(
        rhi::IGpuDevice* device,
        rhi::TextureHandle& target,
        rhi::Extent2D size, rhi::Format format,
        rhi::TextureUsageFlags usage, eastl::string_view name ) -> void;

    auto SetCompatibilityViewport( rhi::ICommandList* commands, rhi::Extent2D size ) -> void;

    /** Records a fullscreen pass with a sampled input and fixed uniforms. */
    class CompatibilityFullscreenPass {
    public:
        auto Initialize(
            rhi::IGpuDevice* device,
            eastl::string_view fragmentShader,
            core::usize uniformSize, bool extraSource = false ) -> void;
        auto Record(
            rhi::ICommandList* commands,
            eastl::string_view name,
            rhi::TextureHandle source, rhi::TextureHandle output,
            const void* parameters, core::usize parameterSize,
            const rhi::TextureHandle& extraSource = {} ) -> void;

    private:
        rhi::IGpuDevice* mDevice{};
        rhi::BindingLayoutHandle mBindings{};
        rhi::PipelineLayoutHandle mLayout{};
        rhi::PipelineHandle mPipeline{};
        rhi::BufferHandle mUniform{};
        rhi::SamplerHandle mSampler{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_PASS_RESOURCES_HH
