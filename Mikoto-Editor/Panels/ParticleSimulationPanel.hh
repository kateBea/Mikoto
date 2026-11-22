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

#ifndef MIKOTO_PANELS_PARTICLE_SIMULATION_PANEL_HH
#define MIKOTO_PANELS_PARTICLE_SIMULATION_PANEL_HH

#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/Particle/ParticleGraph.hh>

#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

#include <ImGui/GraphEditor.hh>

#include <Application/PreviewEnvironment.hh>

#include <Panels/Panel.hh>


namespace mikoto::editor {
    struct EditorState;

    struct ParticleSimulationPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    /** Independent emitter authoring scene. Nothing is applied to the project until requested. */
    class ParticleSimulationPanel final : public Panel {
    public:
        explicit ParticleSimulationPanel( const ParticleSimulationPanelCreateInfo& info );
        ~ParticleSimulationPanel() override;
        auto OnRender( core::f32 timeStep ) -> void override;
        /** Opens a saved recipe without retaining pointers into the active scene. */
        auto OpenAsset( const filesystem::Path& path ) -> void;
        auto EditEffect( const asset::ParticleEffectHandle& effect ) -> void;

    private:
        auto InitializePreview() -> void;
        auto DrawToolbar() -> void;
        auto DrawEmitterPicker() -> void;
        auto DrawEmitterMaterial() -> void;
        auto DrawPreview( core::f32 timeStep ) -> void;
        auto DrawSettings() -> void;
        auto SetRecipe( renderer::ParticleEmitterSettings settings ) -> void;
        auto ReplaceEmitter( const renderer::ParticleEmitterSettings& settings, const material::ParticleMaterialHandle& material ) -> void;
        auto SelectEmitter( core::usize index ) -> void;
        auto RefreshPreview() -> void;
        auto SaveAsset() -> void;
        auto DrawGraph() -> void;

        EditorState* mEditorState{};

        eastl::unique_ptr<scene::Scene> mScene{};
        eastl::unique_ptr<renderer::ISceneRenderer> mRenderer{};
        scene::SceneCamera mCamera{};
        entt::entity mSimulator{ entt::null };
        entt::entity mGround{ entt::null };

        eastl::vector<renderer::ParticleEmitterHandle> mEmitters{};
        core::usize mSelectedEmitter{};
        renderer::ParticleEmitterSettings mSettings{};
        renderer::ParticleSimulationMode mMode{ renderer::ParticleSimulationMode::eCpu };

        PreviewEnvironment mEnvironment{};
        asset::AssetHandle<asset::Texture> mEnvironmentTexture{};

        filesystem::Path mPath{};
        eastl::string mError{};

        core::f32 mPreviewHeight{ 340.0f };
        core::f32 mYaw{ 35 };
        core::f32 mPitch{ 18 };
        core::f32 mDistance{ 8 };
        core::f32 mTargetHeight{ 1.5f };

        core::f32 mSpeed{ 1 };
        core::f32 mElapsed{};

        core::f32 mBlur{ 1.5f };
        core::f32 mExposure{ 1 };

        bool mShowGround{ true };
        bool mShowGrid{ true };
        bool mStep{};
        bool mUnsaved{};

        renderer::ParticleGraph mGraph{};
        imgui::NodeEditorContext mGraphContext{};
        eastl::string mGraphMessage{};
        bool mGraphRestore{true};
        bool mGraphFit{true};
    };
}// namespace mikoto::editor
#endif

