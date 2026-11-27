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

#ifndef MIKOTO_PANELS_PREFERENCES_PANEL_HH
#define MIKOTO_PANELS_PREFERENCES_PANEL_HH

#include <imgui.h>

#include <EASTL/optional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

#include <Platform/Utility.hh>
#include <Platform/ActionManager.hh>

#include <Panels/Panel.hh>

namespace mikoto::platform {
    class ActionManager;
}

namespace mikoto::editor {
    struct EditorState;
    class ViewportPanel;
    class SettingsPanel;

    struct PreferencesPanelCreateInfo {
        EditorState* mState{};
        ViewportPanel* mViewportPanel{};
        SettingsPanel* mCameraPanel{};
        const platform::ActionManager* mActions{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class PreferencesPanel final : public Panel {
    public:
        explicit PreferencesPanel( const PreferencesPanelCreateInfo& info );
        auto OnRender( float timeStep ) -> void override;
        auto ApplyPending() -> void;
        auto OpenPlatformSettings() -> void;

    private:
        auto Load() -> void;
        auto Save() -> void;
        auto DrawShortcuts() const -> void;
        auto DrawPlatformSettings() -> void;
        auto DrawFontSelector( core::usize &index, const char *labelKey ) -> bool;
        auto LoadSystemFonts() -> void;

        static constexpr core::i32 kPlatformCategory{ 5 };
        static constexpr core::f32 kDefaultTextScale{ 1.20f };
        static constexpr bool kDefaultCompactSpacing{ true };

        EditorState* mState{};
        ViewportPanel* mViewportPanel{};
        SettingsPanel* mCameraPanel{};
        const platform::ActionManager* mActionManager{};

        ImGuiStyle mOriginalStyle{};
        filesystem::Path mPath{};

        eastl::vector<platform::InstalledFontFamily> mFontChoices{};
        core::usize mBundledFontCount{};
        bool mSystemFontsLoaded{};
        core::usize mFontIndex{};
        core::usize mConsoleFontIndex{};
        eastl::optional<core::usize> mAppliedConsoleFontIndex{};
        core::f32 mConsoleTextScale{ 0.95f };
        bool mConsoleUseEditorFont{ true };
        eastl::optional<core::usize> mAppliedFontIndex{};
        ImGuiTextFilter mFontFilter{};

        core::i32 mTheme{};
        core::f32 mTextScale{ kDefaultTextScale };
        core::f32 mTooltipDelay{ 0.4f };
        bool mCompact{ kDefaultCompactSpacing };

        bool mVsync{ true };
        bool mPerformanceOverlay{};
        bool mUtilityOverlay{ true };

        bool mPending{ true };
        bool mDirty{};

        core::i32 mCategory{};
        core::i32 mPlatformPreview{};
        eastl::string mStatus{};
        ImGuiTextFilter mSearchFilter{};
    };
}

#endif// MIKOTO_PANELS_PREFERENCES_PANEL_HH
