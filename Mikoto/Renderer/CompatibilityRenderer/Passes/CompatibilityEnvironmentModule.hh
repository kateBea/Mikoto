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


#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_ENVIRONMENT_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_ENVIRONMENT_MODULE_HH

#include <EASTL/array.h>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Descriptor.hh>
#include <Renderer/Rhi/CommandList.hh>

namespace mikoto::material {
    class SkyboxMaterial;
}

namespace mikoto::renderer {
    struct CompatibilityEnvironment {
        static constexpr core::usize kTextureCount{ 7 };

        eastl::array<rhi::TextureHandle, kTextureCount> mTextures{};
        rhi::TextureHandle mFilteredLighting{};
        rhi::TextureHandle mBrdfLut{};
        rhi::TextureHandle mFallback{};
        rhi::SamplerHandle mSampler{};

        // Strength, sky blur, last mip and source type, as packed in CompatibilityFrame.slang.
        core::float4 mParameters{};
        bool mHasSkybox{};

        static auto DeclareBindings( rhi::BindingLayoutDescription& layout ) -> void;
        auto AddBindings( rhi::BindingTableDescription& bindings ) const -> void;
        static auto DeclareLightingBindings( rhi::BindingLayoutDescription& layout ) -> void;
        auto AddLightingBindings( rhi::BindingTableDescription& bindings ) const -> void;
    };

    /** Resolves environment textures shared by the skybox and PBR passes. */
    class CompatibilityEnvironmentModule {
    public:
        auto Initialize( rhi::IGpuDevice* device ) -> void;
        MKT_NODISCARD auto Prepare(
            rhi::ICommandList* commands,
            const material::SkyboxMaterial* skybox,
            core::f32 strength, core::f32 blur ) -> CompatibilityEnvironment;
        auto RecordSkybox(
            rhi::ICommandList* commands,
            const CompatibilityEnvironment& environment,
            rhi::IBuffer& frameUniform ) -> void;

    private:
        auto BakeLighting( rhi::ICommandList* commands, const CompatibilityEnvironment& environment ) -> void;

        rhi::IGpuDevice* mDevice{};
        rhi::TextureHandle mFallback{};
        rhi::SamplerHandle mSampler{};
        rhi::BindingLayoutHandle mBindings{};
        rhi::PipelineLayoutHandle mLayout{};
        rhi::PipelineHandle mPipeline{};
        rhi::PipelineHandle mBakePipeline{};
        rhi::BufferHandle mBakeUniform{};
        rhi::PipelineLayoutHandle mBrdfLayout{};
        rhi::PipelineHandle mBrdfPipeline{};

        rhi::TextureHandle mFilteredLighting{};
        rhi::TextureHandle mBrdfLut{};
        eastl::array<rhi::TextureHandle, CompatibilityEnvironment::kTextureCount> mBakedSources{};
        bool mFallbackUploaded{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_ENVIRONMENT_MODULE_HH
