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

#include <Core/LocalizationService.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {
    Panel::Panel( eastl::string_view name, eastl::string_view headerName, eastl::string_view group, bool isToggleable )
        : mPanelName{ name },
          mPanelHeaderName{ headerName },
          mGroup{ group },
          mPanelIsVisible{ true },
          mIsToggleable{ isToggleable } {
    }


    auto Panel::SetVisible( const bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mPanelIsVisible = value;
    }


    auto Panel::SetGroup( eastl::string_view group ) -> void {
        MKT_BEGIN_PROFILER();
        mGroup = group;
    }

    auto Panel::GetGroup() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mGroup;
    }

    auto Panel::SetToggleable( bool toggleable ) -> void {
        MKT_BEGIN_PROFILER();
        mIsToggleable = toggleable;
    }

    auto Panel::IsToggleable() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsToggleable;
    }

    auto Panel::GetViewport() const -> const ViewportInfo& {
        MKT_BEGIN_PROFILER();
        return mViewport;
    }


    auto Panel::IsHovered() const -> bool {
        MKT_BEGIN_PROFILER();
        return mPanelIsHovered;
    }


    auto Panel::IsFocused() const -> bool {
        MKT_BEGIN_PROFILER();
        return mPanelIsFocused;
    }


    auto Panel::IsVisible() const -> bool {
        MKT_BEGIN_PROFILER();
        return mPanelIsVisible;
    }


    auto Panel::GetName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mPanelName;
    }


    auto Panel::GetHeaderName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        // Keep the icon prefix, but give every language the same docking ID.
        const auto nameOffset{ mPanelHeaderName.find( mPanelName ) };
        mLocalizedHeaderName = nameOffset == eastl::string::npos
                                   ? eastl::string{}
                                   : mPanelHeaderName.substr( 0, nameOffset );
        const auto displayName{ GetDisplayName() };
        mLocalizedHeaderName.append( displayName.data(), displayName.size() );
        mLocalizedHeaderName += "###";
        mLocalizedHeaderName += mPanelHeaderName;
        return mLocalizedHeaderName;
    }

    auto Panel::GetDisplayName() const -> eastl::string_view {
        struct Title {
            core::cstr mName{};
            core::cstr mKey{};
        };

        static constexpr Title titles[]{
            { "Explorer", "content_browser_title" },
            { "Hierarchy", "hierarchy_title" },
            { "Inspector", "inspector_title" },
            { "Scene", "viewport_title" },
            { "Game", "game_preview_title" },
            { "Settings", "settings_title" },
            { "Renderer", "renderer_title" },
            { "Lighting", "lighting_title" },
            { "Stats", "stats_title" },
            { "GPU Stats", "gpu_stats_title" },
            { "Instrumentation", "instrumentation_title" },
            { "Compiler Status", "compiler_status_title" },
            { "Console", "runtime_console_title" },
            { "Material Editor", "material_editor_title" },
            { "Particle Simulation", "particle_simulation_title" },
            { "Animator", "animator_title" },
            { "Animator Graph", "animation_graph_title" },
            { "Script Graph", "script_graph_title" },
            { "Pass Visualizer", "pass_visualizer_title" },
            { "Simulation Settings", "simulation_settings_title" },
            { "Audio Mixer", "audio_mixer_title" },
            { "Audio", "audio_panel_title" },
            { "Audio Profiler", "audio_profiler_title" },
            { "About Mikoto", "about_title" },
            { "Materials Browser", "materials_browser_title" },
            { "Animations Browser", "animations_browser_title" },
            { "Prefabs Browser", "prefabs_browser_title" },
            { "Preferences", "preferences_title" },
            { "Layouts", "layouts_title" },
        };

        for ( const auto& title: titles ) {
            if ( mPanelName == title.mName ) {
                return MKT_LOC( title.mKey );
            }
        }

        return mPanelName;
    }
}
