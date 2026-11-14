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

#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_SHADOW_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_SHADOW_MODULE_HH

#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>

namespace mikoto::scene {
    class Camera;
    class Scene;
}

namespace mikoto::renderer {
    class CompatibilityGeometryModule;
    struct SceneRenderSettings;
    struct CompatibilityEnvironment;

    /** Records a fixed-binding directional shadow map for the compatibility renderer. */
    class CompatibilityShadowModule {
    public:
        auto Record( rhi::IGpuDevice& device, rhi::ICommandList& commands, const scene::Scene& scene, const scene::Camera& camera,
            CompatibilityGeometryModule& geometry, const CompatibilityEnvironment& environment, const SceneRenderSettings& settings ) -> void;

        MKT_NODISCARD auto GetMap() const -> rhi::TextureHandle;
        MKT_NODISCARD auto GetMatrix() const -> const core::float4x4& { return mMatrix; }
        MKT_NODISCARD auto GetLightIndex() const -> core::u32 { return mLightIndex; }
        MKT_NODISCARD auto GetDiagnostics() const -> const ShadowDiagnostics& { return mDiagnostics; }

    private:
        rhi::TextureHandle mColor{};
        rhi::TextureHandle mDepth{};
        rhi::BufferHandle mUniform{};
        core::float4x4 mMatrix{ 1.0f };
        core::u32 mLightIndex{};
        ShadowDiagnostics mDiagnostics{};
    };
}

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_SHADOW_MODULE_HH
