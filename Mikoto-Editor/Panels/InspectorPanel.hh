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

#ifndef MIKOTO_INSPECTOR_PANEL_HH
#define MIKOTO_INSPECTOR_PANEL_HH

#include <ankerl/unordered_dense.h>

#include <Panels/Panel.hh>

#include <Scene/Entity.hh>

namespace mikoto::editor {
    struct EditorState;

    struct InspectorPanelCreateInfo {
        EditorState* mState{};
    };

    class InspectorPanel final : public Panel {
    public:
        using ComponentUIFunc = void (InspectorPanel::*)(scene::Entity&);

        explicit InspectorPanel( const InspectorPanelCreateInfo& createInfo );

        auto OnUpdate( float timeStep ) -> void override;

        ~InspectorPanel() override = default;

    private:
        auto DrawComponents( scene::Entity* entity ) -> void;

        auto DrawTransformComponentTab( scene::Entity& entity ) -> void;
        auto DrawScriptingComponentTab( scene::Entity& entity ) -> void;
        auto DrawAnimatorComponentTab( scene::Entity& entity ) -> void;
        auto DrawSkinMeshComponentTab( scene::Entity& entity ) -> void;
        auto DrawMaterialComponentTab( scene::Entity& entity ) -> void;
        auto DrawPhysicsComponentTab( scene::Entity& entity ) -> void;
        auto DrawRenderComponentTab( scene::Entity& entity ) -> void;
        auto DrawLightComponentTab( scene::Entity& entity ) -> void;
        auto DrawTextComponentTab( scene::Entity& entity ) -> void;
        auto DrawAudioListenerComponentTab( scene::Entity& entity ) -> void;
        auto DrawAudioComponentTab( scene::Entity& entity ) -> void;
        auto DrawCameraComponentTab( scene::Entity& entity ) -> void;
        auto DrawSkyboxComponentTab( scene::Entity& entity ) -> void;
        auto DrawMeshColliderComponentTab( scene::Entity& entity ) -> void;
        auto DrawBoxColliderComponentTab( scene::Entity& entity ) -> void;
        auto DrawSphereColliderComponentTab( scene::Entity& entity ) -> void;
        auto DrawCapsuleColliderComponentTab( scene::Entity& entity ) -> void;

    private:
        EditorState* mState{};

        struct ComponentUiInfo {
            eastl::string mLabel{};
            ComponentUIFunc mCallback{}; };
        ankerl::unordered_dense::map<entt::id_type, ComponentUiInfo> mComponentUis{};
    };
}// namespace Mikoto

#endif// MIKOTO_INSPECTOR_PANEL_HH
