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

#ifndef MIKOTO_PANELS_MATERIAL_EDITOR_PANEL_HH
#define MIKOTO_PANELS_MATERIAL_EDITOR_PANEL_HH

#include <EASTL/shared_ptr.h>
#include <EASTL/unique_ptr.h>

#include <Renderer/Core/ThumbnailRenderer.hh>

#include <Application/PreviewEnvironment.hh>
#include <Application/ParticleMaterialPreview.hh>
#include <Application/MaterialGraphEditor.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {

    struct EditorState;

    struct MaterialEditorPanelDescription {
        EditorState* mState{};
        renderer::rhi::IGpuDevice* mDevice{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    /** @brief Material workspace with a preview/details column and a reserved graph canvas. */
    class MaterialEditorPanel final : public Panel {
    public:
        explicit MaterialEditorPanel( const MaterialEditorPanelDescription& description );

        auto OnRender( float timeStep ) -> void override;
        ~MaterialEditorPanel() override = default;

    private:
        struct ModelLoad;

    private:
        auto DrawGraph() -> void;
        auto DrawToolbar() -> void;
        auto DrawMaterial() -> void;
        auto DrawLighting() -> void;
        auto DrawOtherMaterial() -> void;
        auto RequestPreviewModel() -> void;
        auto DrawMaterialDomain() -> void;
        auto DrawPreview( core::f32 timeStep ) -> void;

        auto LoadMaterial() -> void;

        auto SaveMaterial( bool choosePath ) -> void;

        auto DrawTexture( material::MapType type, core::cstr label ) -> void;

        auto SetAsset( material::MaterialHandle material ) -> void;
        auto SetMaterial( core::Ref<material::SurfaceMaterial> material ) -> void;

    private:
        EditorState* mEditorState{};
        renderer::rhi::IGpuDevice* mDevice{};
        eastl::unique_ptr<renderer::ThumbnailRenderer> mRenderer{};
        eastl::unique_ptr<ParticleMaterialPreview> mParticlePreview{};
        eastl::shared_ptr<ModelLoad> mModelLoad{};
        core::Ref<material::SurfaceMaterial> mMaterial{};

        asset::ModelHandle mModel{};
        renderer::rhi::TextureHandle mPreviewTexture{};
        // Resource view of EditorState::mEditMaterial, also used to detect asset switches.
        material::MaterialHandle mMaterialResource{};

        // Retain domain-specific edits while switching within the open document.
        eastl::array<material::MaterialHandle, 6> mDomainMaterials{};

        renderer::ThumbnailPreviewSettings mPreview{};

        PreviewEnvironment mEnvironment{};
        asset::AssetHandle<asset::Texture> mPreviewEnvironmentAsset{};
        PreviewEnvironment mPostProcessImageLoad{};
        renderer::rhi::TextureHandle mPostProcessImage{};
        eastl::array<char, 256> mName{};

        eastl::string mError{};

        MaterialGraphEditor mGraph{};
        core::f32 mPreviewHeight{ 300.0f };

        core::u32 mPreviewWidth{};
        core::u32 mPreviewPixelHeight{};

        bool mUnsaved{};
        bool mAutoRotate{};
        bool mPreviewPending{};
        bool mPreviewDirty{ true };
        const scene::Scene* mParticleTargetScene{};
        core::u64 mParticleTargetEntity{};
        core::usize mParticleTargetEmitter{};
    };

}// namespace mikoto::editor

#endif// MIKOTO_PANELS_MATERIAL_EDITOR_PANEL_HH

