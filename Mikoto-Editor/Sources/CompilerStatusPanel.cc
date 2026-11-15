
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


#include <slang.h>

#include <Core/LocalizationService.hh>

#include <Filesystem/FileSystem.hh>

#include <Scene/Scene.hh>

#include <Scripting/ScriptingService.hh>
#include <Scripting/Dotnet/DotnetScriptingService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Theme/PopupStyle.hh>

#include <Panels/CompilerStatusPanel.hh>

#include <Layers/EditorLayer.hh>


namespace mikoto::editor {
    CompilerStatusPanel::CompilerStatusPanel( const CompilerStatusPanelCreateInfo& info )
        : Panel{ "Compiler Status", "Compiler Status", info.mGroup, info.mIsToggleable },
          mState{ info.mState } {
        mPanelIsVisible = false;
    }

    auto CompilerStatusPanel::OnRender( core::f32 ) -> void {
        DrawBuildPopup();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 720, 500 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        mPanelIsFocused = ImGui::IsWindowFocused();
        mPanelIsHovered = ImGui::IsWindowHovered();
        ImGui::SeparatorText( "C# / .NET" );
        ImGui::Text( MKT_LOC( "compiler_status_build_support" ).c_str(), scripting::DotnetScriptingService::IsSupported() ? MKT_LOC( "compiler_status_enabled" ).c_str() : MKT_LOC( "compiler_status_disabled_mikoto_enable_dotnet_scripting" ).c_str() );
        const filesystem::Path bridge{ "Resources/Scripting/Mikoto.ScriptCore.dll" };
        const filesystem::Path config{ "Resources/Scripting/Mikoto.ScriptCore.runtimeconfig.json" };
        ImGui::Text( MKT_LOC( "compiler_status_managed_bridge_runtime_configuration" ).c_str(), bridge.Exists() ? MKT_LOC( "compiler_status_found" ).c_str() : MKT_LOC( "compiler_status_missing" ).c_str(), config.Exists() ? MKT_LOC( "compiler_status_found" ).c_str() : MKT_LOC( "compiler_status_missing" ).c_str() );
        ImGui::TextWrapped( MKT_LOC( "compiler_status_saving_c_files_triggers_a_background_build_after_a_short" ).c_str() );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "compiler_status_clear_build_history", "Clear build history" ).c_str() ) ) {
            scripting::DotnetCompilerStatus::Clear();
        }

        ImGui::SameLine();
        imgui::CheckBox( imgui::widget::LocalizedLabel( "compiler_status_failures_only", "Failures only" ).c_str(), mErrorsOnly );
        mFilter.Draw( MKT_LOC( "compiler_status_filter_source_or_diagnostics" ).c_str(), -1.0f );
        const auto records{ scripting::DotnetCompilerStatus::Snapshot() };

        if ( records.empty() ) {
            ImGui::TextDisabled( MKT_LOC( "compiler_status_no_c_compilations_recorded_play_a_scene_containing_an_active" ).c_str() );
        }

        if ( ImGui::BeginChild( "Build history", { 0, -80 }, ImGuiChildFlags_Borders ) ) {
            for ( core::usize index{ records.size() }; index > 0; --index ) {
                const auto& record{ records[index - 1] };

                if ( ( mErrorsOnly && record.mStatus != 3 ) ||
                     ( !mFilter.PassFilter( record.mSource.c_str() ) && !mFilter.PassFilter( record.mDiagnostics.c_str() ) ) ) {
                    continue;
                }

                ImGui::PushID( static_cast<int>( index ) );
                const char* status{ record.mStatus == 0 ? MKT_LOC( "compiler_status_compiling" ).c_str() : record.mStatus == 1 ? MKT_LOC( "compiler_status_succeeded" ).c_str()
                                                                                                       : record.mStatus == 2   ? MKT_LOC( "compiler_status_cache_hit" ).c_str()
                                                                                                                               : MKT_LOC( "compiler_status_failed" ).c_str() };

                if ( ImGui::TreeNodeEx( record.mSource.c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s — %s", status, record.mSource.c_str() ) ) {
                    ImGui::TextUnformatted( record.mRuntime.c_str() );
                    ImGui::Text( MKT_LOC( "compiler_status_ms_files_errors_warnings" ).c_str(), record.mMilliseconds, record.mFiles, record.mErrors, record.mWarnings );
                    ImGui::TextWrapped( MKT_LOC( "compiler_status_output" ).c_str(), record.mOutput.empty() ? MKT_LOC( "compiler_status_none" ).c_str() : record.mOutput.c_str() );

                    if ( ImGui::Button( imgui::widget::LocalizedLabel( "compiler_status_edit_source", "Edit source" ).c_str() ) ) {
                        mOpenFailed = !filesystem::OpenInTextEditor( filesystem::Path{ record.mSource.c_str() }, mState->mExternalEditor );
                    }

                    ImGui::SameLine();

                    if ( ImGui::Button( imgui::widget::LocalizedLabel( "compiler_status_copy_diagnostics", "Copy diagnostics" ).c_str() ) ) {
                        ImGui::SetClipboardText( record.mDiagnostics.c_str() );
                    }

                    ImGui::TextWrapped( "%s", record.mDiagnostics.empty() ? MKT_LOC( "compiler_status_no_diagnostics" ).c_str() : record.mDiagnostics.c_str() );
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        if ( mOpenFailed ) {
            ImGui::TextWrapped( MKT_LOC( "compiler_status_could_not_open_vs_code_check_that_the_editor_is" ).c_str() );
        }

        ImGui::SeparatorText( MKT_LOC( "compiler_status_shader_compiler" ).c_str() );
        ImGui::Text( "Slang: %s", spGetBuildTagString() );
        ImGui::TextWrapped( MKT_LOC( "compiler_status_runtime_shader_compilation_uses_slang_spir_v_reflection_uses_spirv" ).c_str() );
        ImGui::End();
    }

    auto CompilerStatusPanel::UpdateBuilds( const filesystem::Path& assets, const filesystem::Path& project ) -> void {
        mBuildTracker.Update( mState->mActiveScene, assets, project );

        const auto revision{ scripting::DotnetScriptingService::GetCompilationRevision() };

        if ( mBuildTracker.IsBuilding() || !mState->mActiveScene ) {
            return;
        }

        const bool recompiled{ revision != mScriptCompilationRevision };
        mScriptCompilationRevision = revision;

        // Build workers never touch scene components. Refresh every assigned C#
        // script here because a changed source can affect other classes in its assembly.
        auto& scene{ *mState->mActiveScene };
        const auto view{ scene.GetRegistry().view<scene::ScriptComponent>() };

        for ( const auto handle: view ) {
            auto& component{ view.get<scene::ScriptComponent>( handle ) };

            if ( component.GetLanguage() != scripting::ScriptLanguage::eCSharp || component.GetFilePath().IsEmpty() ) {
                continue;
            }

            auto script{ component.GetHandle() };

            if ( !script ) {
                script = scripting::ScriptingService::Get()->LoadScript(
                    component.GetLanguage(), component.GetFilePath(), scene.FindByHandle( handle ) );
                component.SetScript( script );
            }
            else if ( recompiled ) {
                script->ReloadScript();
            }
        }
    }

    auto CompilerStatusPanel::DrawBuildPopup() -> void {
        const auto records{ scripting::DotnetCompilerStatus::Snapshot() };

        if ( records.empty() ) {
            return;
        }

        const auto& record{ records.back() };
        // Completion is recorded in build history; a successful build should
        // not leave a modal-looking toast over the scene when Play starts.
        if ( record.mStatus == 1 || record.mStatus == 2 ) {
            return;
        }

        const bool building{ record.mStatus == 0 };

        if ( mBuildSequence != record.mSequence ) {
            mBuildSequence = record.mSequence;
            mBuildVisibleUntil = ImGui::GetTime() + 8.0;
        }

        if ( !building && ImGui::GetTime() > mBuildVisibleUntil ) {
            return;
        }

        const auto* viewport{ ImGui::GetMainViewport() };
        ImGui::SetNextWindowViewport( viewport->ID );
        ImGui::SetNextWindowPos( viewport->GetWorkCenter(), ImGuiCond_Always, { 0.5f, 0.5f } );
        ImGui::SetNextWindowSize( { 440.0f, 0.0f } );
        const PopupStyle popupStyle{};

        if ( imgui::BeginWindow( imgui::widget::LocalizedLabel( "compiler_status_c_build_progress_toast", "C# build progress##Toast" ).c_str(), nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing ) ) {
            ImGui::TextUnformatted( MKT_LOC(
                building ? "compiler_status_building_c_scripts" : "compiler_status_c_build_failed" )
                .c_str() );
            const filesystem::Path source{ record.mSource.c_str() };
            ImGui::TextWrapped( "%s", source.GetFilename().data() );
            const char* stage{ record.mStage == 1 ? MKT_LOC( "compiler_status_reading_source_files" ).c_str() : record.mStage == 2 ? MKT_LOC( "compiler_status_resolving_references" ).c_str()
                                                                                                            : record.mStage == 3   ? MKT_LOC( "compiler_status_compiling_assembly" ).c_str()
                                                                                                            : record.mStage == 4   ? MKT_LOC( "compiler_status_writing_dll_and_symbols" ).c_str()
                                                                                                                                   : MKT_LOC( "compiler_status_preparing_compiler" ).c_str() };

            if ( building ) {
                ImGui::TextUnformatted( stage );
            }
            else {
                ImGui::Text( MKT_LOC( "compiler_status_files_errors_warnings_ms" ).c_str(), record.mFiles, record.mErrors, record.mWarnings, record.mMilliseconds );
            }

            const auto position{ ImGui::GetCursorScreenPos() };
            const float width{ ImGui::GetContentRegionAvail().x };
            auto* draw{ ImGui::GetWindowDrawList() };
            draw->AddRectFilled( position, { position.x + width, position.y + 7.0f }, ImGui::GetColorU32( ImGuiCol_FrameBg ), 3.5f );

            if ( building ) {
                // The build process does not report percentage completion. Animate a
                // bounded segment rather than inventing a progress percentage.
                const float phase{ static_cast<float>( ImGui::GetTime() - static_cast<int>( ImGui::GetTime() ) ) };
                const float offset{ phase * width * 0.72f };
                draw->AddRectFilled( { position.x + offset, position.y },
                    { position.x + offset + width * 0.28f, position.y + 7.0f }, ImGui::GetColorU32( ImGuiCol_PlotHistogram ), 3.5f );
            }
            else {
                draw->AddRectFilled( position, { position.x + width, position.y + 7.0f },
                    IM_COL32( 204, 62, 62, 255 ), 3.5f );
            }

            ImGui::Dummy( { width, 13.0f } );
        }

        ImGui::End();
    }
}
