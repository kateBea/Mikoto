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


#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_MATERIAL_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_MATERIAL_MODULE_HH

#include <EASTL/hash_map.h>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Descriptor.hh>

namespace mikoto::renderer::rhi {
    class ICommandList;
}

namespace mikoto::material {
    class SurfaceMaterial;
}

namespace mikoto::renderer {
    struct CompatibilityEnvironment;

    /** Builds fixed material bindings and uploads per-draw surface parameters. */
    class CompatibilityMaterialModule {
    public:
        auto Initialize( rhi::IGpuDevice& device ) -> void;
        auto BeginFrame() -> void;
        auto Bind( rhi::ICommandList& commands, const material::SurfaceMaterial* material, const core::float4x4& world,
            const core::float4x4& normalMatrix, const CompatibilityEnvironment& environment, rhi::IBuffer& frameUniform,
            core::u32 id, bool highlighted, const rhi::TextureHandle& shadowMap, bool transparentPass ) -> void;
        MKT_NODISCARD auto GetLayout() const -> const rhi::PipelineLayoutHandle&;

    private:
        MKT_NODISCARD auto CreateBindings( const material::SurfaceMaterial* material, const CompatibilityEnvironment& environment,
            rhi::IBuffer& frameUniform, rhi::IBuffer& drawUniform, const rhi::TextureHandle& shadowMap ) -> rhi::BindingTableHandle;

        rhi::IGpuDevice* mDevice{};
        rhi::SamplerHandle mSampler{};
        rhi::BindingLayoutHandle mBindings{};
        rhi::PipelineLayoutHandle mLayout{};
        rhi::BufferHandle mUniform{};
        eastl::hash_map<const material::SurfaceMaterial*, rhi::BindingTableHandle> mFrameTables{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_MATERIAL_MODULE_HH
