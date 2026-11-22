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

#ifndef MIKOTO_APPLICATION_PARTICLE_MATERIAL_PREVIEW_HH
#define MIKOTO_APPLICATION_PARTICLE_MATERIAL_PREVIEW_HH

#include <Material/PostProcessMaterial.hh>
#include <Material/SkyboxMaterial.hh>

#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/Core/ThumbnailRenderer.hh>

#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

namespace mikoto::editor {

    // Isolate preview emitters and playback from the scene being edited.
    class ParticleMaterialPreview final {
    public:
        ParticleMaterialPreview( renderer::rhi::IGpuDevice* device, const asset::ModelHandle& ground );

        MKT_NODISCARD auto Render(
            const material::ParticleMaterialHandle& material,
            const renderer::ThumbnailPreviewSettings& settings,
            const asset::AssetHandle<asset::Texture>& environment,
            core::f32 timeStep,
            core::u32 width,
            core::u32 height ) -> renderer::rhi::TextureHandle;

        ~ParticleMaterialPreview();

    private:
        eastl::unique_ptr<scene::Scene> mScene{};
        eastl::unique_ptr<renderer::ISceneRenderer> mRenderer{};

        scene::SceneCamera mCamera{};

        material::ParticleMaterialHandle mMaterial{};
        renderer::rhi::TextureHandle mEnvironment{};
        core::Ref<material::SkyboxMaterial> mSkybox{};
        core::Ref<material::PostProcessMaterial> mPostProcess{};

        entt::entity mSimulator{ entt::null };
        entt::entity mGround{ entt::null };
        entt::entity mLight{ entt::null };
    };

}// namespace mikoto::editor

#endif// MIKOTO_APPLICATION_PARTICLE_MATERIAL_PREVIEW_HH
