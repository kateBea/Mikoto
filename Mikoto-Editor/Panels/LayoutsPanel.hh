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

#ifndef MIKOTO_PANELS_LAYOUTS_PANEL_HH
#define MIKOTO_PANELS_LAYOUTS_PANEL_HH

#include <EASTL/utility.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/functional.h>

#include <Filesystem/Path.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {
    using LayoutPanelVisibility = eastl::vector<eastl::pair<eastl::string, bool>>;

    /** @brief Connects saved docking layouts to the editor's panel visibility and reset actions. */
    struct LayoutsPanelCreateInfo {
        eastl::function<LayoutPanelVisibility()> mCaptureVisibility{};
        eastl::function<void( LayoutPanelVisibility )> mRestoreVisibility{};
        eastl::function<void()> mResetLayout{};
        eastl::string mGroup{ "Settings" };
        bool mIsToggleable{ true };
    };

    /** @brief Saves and restores named editor workspaces through ImGui settings and YAML metadata. */
    class LayoutsPanel final : public Panel {
    public:
        /** @brief Creates the layouts browser.
         * @param info Callbacks for capturing visibility, restoring it and resetting the workspace.
         */
        explicit LayoutsPanel( const LayoutsPanelCreateInfo& info );
        auto OnRender( core::f32 timeStep ) -> void override;

    private:
        /** @brief Refreshes the saved layout files from the editor's ImGui directory. */
        auto RefreshLayouts() -> void;
        /** @brief Saves docking settings and panel visibility under the entered layout name. */
        auto SaveLayout() -> void;
        /** @brief Validates a saved layout and queues its restoration between frames.
         * @param path Layout document to read.
         */
        auto LoadLayout( const filesystem::Path& path ) -> void;

        LayoutsPanelCreateInfo mCallbacks{};
        filesystem::Path mDirectory{};
        eastl::vector<filesystem::Path> mLayouts{};
        eastl::array<char, 96> mName{};
        eastl::string mError{};
        eastl::string mActiveLayout{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_LAYOUTS_PANEL_HH
