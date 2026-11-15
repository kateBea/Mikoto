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

#ifndef MIKOTO_PANELS_ANIMATION_GRAPH_PANEL_HH
#define MIKOTO_PANELS_ANIMATION_GRAPH_PANEL_HH

#include <filesystem>

#include <EASTL/string.h>

#include <ankerl/unordered_dense.h>

#include <Animation/Animator.hh>
#include <Animation/AnimationController.hh>

#include <ImGui/GraphEditor.hh>

#include <Panels/Panel.hh>


namespace mikoto::editor {
    struct EditorState;

    struct AnimationGraphPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class AnimationGraphPanel final : public Panel {
    public:
        explicit AnimationGraphPanel( const AnimationGraphPanelCreateInfo& info );
        auto OnRender( core::f32 timeStep ) -> void override;

    private:
        struct DocumentView {
            std::filesystem::path mPath{};
            bool mDirty{};
        };

    private:
        auto GetSelectedAnimator( core::u64& id ) const -> animation::Animator*;
        auto DrawParameters( animation::AnimationController& controller, DocumentView& document ) -> void;
        auto DrawGraph( animation::AnimationController& controller, DocumentView& document, animation::Animator* animator ) -> void;
        auto DrawInspector( animation::AnimationController& controller, DocumentView& document, animation::Animator* animator ) -> void;
        auto DrawToolbar( animation::AnimationController& controller, DocumentView& document, animation::Animator* animator ) -> void;
        auto Save( animation::AnimationController& controller, DocumentView& document, bool saveAs ) -> void;
        auto Open( animation::AnimationController& controller, DocumentView& document, animation::Animator* animator ) -> void;
        auto Changed( DocumentView& document, animation::Animator* animator ) -> void;

    private:
        EditorState* mEditorState{};
        imgui::NodeEditorContext mContext{};

        animation::AnimationController mScratch{};
        ankerl::unordered_dense::map<core::u64, DocumentView> mDocuments{};
        core::u64 mLastAnimatorID{ ~core::u64{} };

        animation::ControllerID mSelectedState{};
        animation::ControllerID mSelectedTransition{};
        core::i32 mNewParameterType{};

        bool mRestorePositions{ true };
        bool mFitRequested{ true };
        core::i32 mFitDelay{ 2 };

        eastl::string mMessage{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_ANIMATION_GRAPH_PANEL_HH

