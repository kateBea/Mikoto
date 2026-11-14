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

#ifndef MIKOTO_EDITOR_ASSETS_BROWSER_HH
#define MIKOTO_EDITOR_ASSETS_BROWSER_HH

#include <imgui.h>

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Application/ThumbnailCache.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {

    struct EditorState;

    struct MaterialsBrowserPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class MaterialsBrowserPanel final : public Panel {
    public:
        explicit MaterialsBrowserPanel( const MaterialsBrowserPanelCreateInfo& info );

        auto OnRender( core::f32 timeStep ) -> void override;

    private:
        EditorState* mEditorState{};
    };

    struct AnimationsBrowserPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class AnimationsBrowserPanel final : public Panel {
    public:
        explicit AnimationsBrowserPanel( const AnimationsBrowserPanelCreateInfo& info );

        auto OnRender( core::f32 timeStep ) -> void override;

    private:
        EditorState* mEditorState{};
    };

    struct PrefabsBrowserPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class PrefabsBrowserPanel final : public Panel {
    public:
        explicit PrefabsBrowserPanel( const PrefabsBrowserPanelCreateInfo& info );

        auto OnRender( core::f32 timeStep ) -> void override;

    private:
        EditorState* mEditorState{};
        eastl::unique_ptr<ThumbnailCache> mThumbnailCache{};

        ImGuiTextFilter mFilter{};
        core::f32 mThumbnailSize{ 112.0f };
        core::i32 mSelectedPrefab{ -1 };
    };
}

#endif//MIKOTO_EDITOR_ASSETS_BROWSER_HH

