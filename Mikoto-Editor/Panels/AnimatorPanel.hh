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

#ifndef MIKOTO_PANELS_ANIMATOR_PANEL_HH
#define MIKOTO_PANELS_ANIMATOR_PANEL_HH

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <ImGui/Sequencer.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {

    struct EditorState;

    struct AnimatorPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class AnimatorPanel final : public Panel {
    public:
        explicit AnimatorPanel( const AnimatorPanelCreateInfo& data );

        auto OnRender( float timeStep ) -> void override;

    private:
        auto StopPreview() -> void;
        auto DrawAnimation( scene::Entity& entity, scene::KeyframeAnimationComponent& animation ) -> void;

    private:
        EditorState* mEditorState{};
        scene::Scene* mBoundScene{};
        core::u64 mBoundEntity{};

        imgui::Sequencer mSequencer{};
    };

}// namespace mikoto::editor

#endif// MIKOTO_PANELS_ANIMATOR_PANEL_HH

