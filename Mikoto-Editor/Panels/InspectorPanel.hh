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

#ifndef MIKOTO_PANELS_INSPECTOR_PANEL_HH
#define MIKOTO_PANELS_INSPECTOR_PANEL_HH

#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#include <Filesystem/Path.hh>

#include <Assets/AssetTexture.hh>

#include <Renderer/Core/SceneRenderer.hh>

#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <ImGui/TextEditor.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;

    struct InspectorPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class InspectorPanel final : public Panel {
    public:
        using ComponentUIFunc = void ( InspectorPanel::* )( scene::Entity& );

        explicit InspectorPanel( const InspectorPanelCreateInfo& createInfo );

        auto OnRender( float timeStep ) -> void override;

        ~InspectorPanel() override = default;

    private:
        template<typename ComponentType>
        auto RegisterComponent( eastl::string label, ComponentUIFunc callback, core::cstr group ) -> void;

        auto DrawComponents( scene::Entity* entity ) -> void;
        auto DrawNameTextInput( scene::Entity* entity ) -> void;

        auto DrawTransformComponentTab( scene::Entity& entity ) -> void;
        auto DrawScriptingComponentTab( scene::Entity& entity ) -> void;
        auto DrawAnimatorComponentTab( scene::Entity& entity ) -> void;
        auto DrawSkinMeshComponentTab( scene::Entity& entity ) -> void;
        auto DrawCanvasComponentTab( scene::Entity& entity ) -> void;
        auto DrawMaterialComponentTab( scene::Entity& entity ) -> void;
        auto DrawVehicleComponentTab( scene::Entity& entity ) -> void;
        template<typename Component>
        auto DrawExtendedPhysicsComponentTab( scene::Entity& entity ) -> void;
        auto DrawWaterComponentTab( scene::Entity& entity ) -> void;
        auto DrawLandscapeComponentTab( scene::Entity& entity ) -> void;
        auto DrawWaterSimulationComponentTab( scene::Entity& entity ) -> void;
        auto DrawVehicleWheelComponentTab( scene::Entity& entity ) -> void;
        auto AssignScriptPayload( scene::Entity& entity, const ImGuiPayload& payload ) -> void;

        auto DrawPhysicsComponentTab( scene::Entity& entity ) -> void;
        auto DrawRenderComponentTab( scene::Entity& entity ) -> void;
        auto DrawMeshInfo( scene::MeshComponent& component ) -> void;
        auto DrawLightComponentTab( scene::Entity& entity ) -> void;
        auto DrawAreaLightOptions( scene::LightComponent& component ) -> void;
        auto DrawTextComponentTab( scene::Entity& entity ) -> void;
        auto DrawAudioListenerComponentTab( scene::Entity& entity ) -> void;
        auto DrawAudioComponentTab( scene::Entity& entity ) -> void;
        auto DrawVideoComponentTab( scene::Entity& entity ) -> void;
        auto DrawAudioMixerComponentTab( scene::Entity& entity ) -> void;
        auto DrawAudioMixerGroupComponentTab( scene::Entity& entity ) -> void;
        auto DrawParticleComponentTab( scene::Entity& entity ) -> void;
        template<typename Component>
        auto DrawVolumeComponentTab( scene::Entity& entity ) -> void;
        auto DrawCameraComponentTab( scene::Entity& entity ) -> void;
        auto DrawTextureRenderComponentTab( scene::Entity& entity ) -> void;
        auto DrawSkyboxComponentTab( scene::Entity& entity ) -> void;
        auto DrawPostProcessComponentTab( scene::Entity& entity ) -> void;
        auto DrawMeshColliderComponentTab( scene::Entity& entity ) -> void;
        auto DrawBoxColliderComponentTab( scene::Entity& entity ) -> void;
        auto DrawSphereColliderComponentTab( scene::Entity& entity ) -> void;
        auto DrawCapsuleColliderComponentTab( scene::Entity& entity ) -> void;
        auto DrawAudioRoutingField( const char* label, core::u64& output, core::u64 mixer = 0, core::u64 self = 0, bool mixerOnly = false ) -> bool;

    private:
        EditorState* mState{};

        struct MeshTexturePreview {
            filesystem::Path mPath{};
            asset::AssetHandle<asset::Texture> mRequest{};
        };
        eastl::vector<MeshTexturePreview> mMeshTexturePreviews{};
        bool mMeshInfoDrawn{};

        struct TextureRenderOptions {
            core::u64 mEntity{};
            renderer::SceneRenderSettings mSettings{};
        };

        const scene::Scene* mTextureRenderScene{};
        eastl::vector<TextureRenderOptions> mTextureRenderOptions{};

        struct ComponentUiInfo {
            eastl::string mLabel{};
            core::cstr mGroup{};
            bool ( *mPresent )( const scene::Entity& ){};
            ComponentUIFunc mCallback{};
            void ( *mDraw )( InspectorPanel&, scene::Entity&, eastl::string_view, ComponentUIFunc ){};
        };
        eastl::fixed_vector<ComponentUiInfo, 20> mComponentUis{};
        eastl::vector<char> mNameBuffer{};
        eastl::string mNameSource{};
        core::u64 mNameEntity{};
        core::float3 mRotationCenter{};
        core::u64 mRotationEntity{};
        bool mRotating{};
        bool mScriptEditorLaunchFailed{};
        const void* mNameScene{};
        // Selection belongs to the canvas, never to a vector position or an ECS widget.
        const void* mCanvasScene{};
        core::u64 mCanvasEntity{};
        core::u64 mCanvasElement{};
        eastl::vector<char> mCanvasTextBuffer{};
        eastl::vector<char> mCanvasNameBuffer{};


        bool mDisplayScriptPreview{};
        filesystem::Path mScriptPreviewPath{};
        eastl::string mScriptPreviewText{};
        imgui::TextEditor mTextEditor{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_INSPECTOR_PANEL_HH

