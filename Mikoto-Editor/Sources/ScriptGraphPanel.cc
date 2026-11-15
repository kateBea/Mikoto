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

#include <cstring>
#include <fstream>
#include <filesystem>

#include <EASTL/array.h>
#include <EASTL/algorithm.h>

#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/AssetRegistryService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>

#include <Panels/ScriptGraphPanel.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace scripting;
    namespace ed = ax::NodeEditor;

    struct ScriptGraphTarget {
        ScriptLanguage mLanguage{};
        core::cstr mExtension{};
        core::cstr mDefaultFileName{};
    };

    static constexpr ScriptGraphTarget kScriptGraphTargets[]{
        { .mLanguage = ScriptLanguage::eCSharp, .mExtension = "cs", .mDefaultFileName = "GraphScript.cs" },
        { .mLanguage = ScriptLanguage::eLua, .mExtension = "lua", .mDefaultFileName = "GraphScript.lua" },
        { .mLanguage = ScriptLanguage::eNative, .mExtension = "cpp", .mDefaultFileName = "GraphScript.cpp" }
    };

    static auto Color( scripting::ScriptGraphValueType type ) -> ImVec4 {
        switch ( type ) {
            case scripting::ScriptGraphValueType::eFlow:
                return { 1, 1, 1, 1 };
            case scripting::ScriptGraphValueType::eFloat:
                return { .55f, .86f, .2f, 1 };
            case scripting::ScriptGraphValueType::eBool:
                return { .9f, .16f, .18f, 1 };
            case scripting::ScriptGraphValueType::eKeyCode:
                return { .3f, .7f, .8f, 1 };
            case scripting::ScriptGraphValueType::eVector3:
                return { .95f, .72f, .2f, 1 };
            case scripting::ScriptGraphValueType::eString:
                return { .85f, .3f, .75f, 1 };
        }

        return { 1, 1, 1, 1 };
    }

    static auto EditText( const char* label, eastl::string& value ) -> bool {
        eastl::array<char, 2048> buffer{};
        std::memcpy( buffer.data(), value.data(), eastl::min( value.size(), buffer.size() - 1 ) );

        if ( ImGui::InputText( label, buffer.data(), buffer.size() ) ) {
            value = buffer.data();
            return true;
        }

        return false;
    }

    ScriptGraphPanel::ScriptGraphPanel( const ScriptGraphPanelCreateInfo& info )
        : Panel{ "Script Graph", "Script Graph", info.mGroup, info.mIsToggleable } {
        mPanelIsVisible = false;
        mGraph.InitializeDemo();
    }

    auto ScriptGraphPanel::Open() -> void {
        const auto path{ filesystem::OpenFileDialog( { { MKT_LOC( "script_graph_mikoto_script_graph" ).c_str(), "mkscriptgraph" } } ) };

        if ( path.IsEmpty() ) {
            return;
        }

        try {
            std::ifstream file{ std::filesystem::u8path( path.GetC_Str() ) };

            if ( !file ) {
                MKT_THROW_RUNTIME_ERROR( "Could not open script graph." );
            }

            if ( mGraph.Deserialize( YAML::Load( file ), mMessage ) ) {
                mContext.Reset();
                mRestore = mFit = true;
                mDirty = false;
                mResult = {};
            }
        } catch ( const std::exception& e ) {
            mMessage = e.what();
        }
    }

    auto ScriptGraphPanel::Save() -> void {
        const auto selected{ filesystem::SaveFileDialog( "Script.mkscriptgraph", { { MKT_LOC( "script_graph_mikoto_script_graph" ).c_str(), "mkscriptgraph" } } ) };

        if ( selected.IsEmpty() ) {
            return;
        }

        try {
            auto path{ std::filesystem::u8path( selected.GetC_Str() ) };

            if ( path.extension().empty() ) {
                path += ".mkscriptgraph";
            }

            const auto source{ YAML::Dump( mGraph.Serialize() ) };
            std::ofstream file{ path };
            file << source;
            file.flush();

            if ( !file ) {
                MKT_THROW_RUNTIME_ERROR( "Could not save script graph." );
            }

            mDirty = false;
            mMessage = MKT_LOC( "script_graph_graph_saved" ).c_str();
        } catch ( const std::exception& e ) {
            mMessage = e.what();
        }
    }

    auto ScriptGraphPanel::GenerateScript( eastl::string_view name ) -> bool {
        mResult = mCompiler.Compile( mGraph, kScriptGraphTargets[mLanguageIndex].mLanguage, name );
        mMessage = mResult.mSuccess ? MKT_LOC( "script_graph_source_generated" ).c_str() :
            MKT_LOC( "script_graph_resolve_the_highlighted_graph_errors" ).c_str();
        return mResult.mSuccess;
    }

    auto ScriptGraphPanel::ExportScript() -> void {
        const auto& target{ kScriptGraphTargets[mLanguageIndex] };
        const auto selected{ filesystem::SaveFileDialog( target.mDefaultFileName,
            { { MKT_LOC( "script_graph_script_source" ).c_str(), target.mExtension } } ) };

        if ( selected.IsEmpty() ) {
            return;
        }

        try {
            auto path{ selected.GetPathTyped<std::filesystem::path>() };
            if ( path.extension().empty() ) {
                path += ".";
                path += target.mExtension;
            }

            const filesystem::Path scriptPath{ path };
            if ( !scriptPath.IsExtension( target.mExtension ) ) {
                mMessage = MKT_LOC( "script_graph_script_extension_mismatch" ).c_str();
                return;
            }

            if ( !GenerateScript( scriptPath.GetStem() ) ) {
                return;
            }

            std::ofstream file{ path, std::ios::binary };
            file.write( mResult.mSource.data(), static_cast<std::streamsize>( mResult.mSource.size() ) );
            file.close();

            if ( !file ) {
                mMessage = MKT_LOC( "script_graph_script_save_failed" ).c_str();
                return;
            }

            mMessage = MKT_LOC( "script_graph_script_saved" ).c_str();
        } catch ( const std::exception& error ) {
            mMessage = error.what();
        }
    }

    auto ScriptGraphPanel::OnRender( core::f32 ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            mPanelIsHovered = ImGui::IsWindowHovered();
            mPanelIsFocused = ImGui::IsWindowFocused();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_open", "Open..." ).c_str() ) ) {
                if ( mDirty ) {
                    ImGui::OpenPopup( imgui::widget::LocalizedLabel( "script_graph_discard_changes", "Discard changes?" ).c_str() );
                }
                else {
                    Open();
                }
            }

            if ( ImGui::BeginPopupModal( imgui::widget::LocalizedLabel( "script_graph_discard_changes", "Discard changes?" ).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
                ImGui::TextUnformatted( MKT_LOC( "script_graph_open_another_graph_and_discard_unsaved_edits" ).c_str() );

                if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_open_graph", "Open graph" ).c_str() ) ) {
                    ImGui::CloseCurrentPopup();
                    Open();
                }

                ImGui::SameLine();

                if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_cancel", "Cancel" ).c_str() ) ) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_save_as", "Save as..." ).c_str() ) ) {
                Save();
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_add_node", "Add node" ).c_str() ) ) {
                ImGui::OpenPopup( "Script nodes" );
            }

            if ( ImGui::BeginPopup( "Script nodes" ) ) {
                const char* names[]{
                    "EntityScript / OnCreate",
                    "Float",
                    MKT_LOC( "script_graph_add" ).c_str(),
                    MKT_LOC( "script_graph_greater_than" ).c_str(),
                    MKT_LOC( "script_graph_branch" ).c_str(),
                    "Log / Info",
                    MKT_LOC( "script_graph_comment" ).c_str(),
                    "EntityScript / OnUpdate",
                    MKT_LOC( "script_graph_transform_get_position" ).c_str(),
                    MKT_LOC( "script_graph_transform_set_position" ).c_str(),
                    "Input / IsKeyPressed",
                    MKT_LOC( "script_graph_transform_get_rotation" ).c_str(),
                    MKT_LOC( "script_graph_transform_set_rotation" ).c_str(),
                    MKT_LOC( "script_graph_transform_get_scale" ).c_str(),
                    MKT_LOC( "script_graph_transform_set_scale" ).c_str(),
                    "Vector3",
                    MKT_LOC( "script_graph_add_vector3" ).c_str()
                };

                for ( int i{}; i < 17; ++i ) {
                    if ( ImGui::MenuItem( names[i] ) ) {
                        mGraph.AddNode( static_cast<scripting::ScriptNodeType>( i ), { 200, 100 } );
                        mRestore = true;
                        mDirty = true;
                        mResult = {};
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_fit_graph", "Fit graph" ).c_str() ) ) {
                mFit = true;
            }

            ImGui::SameLine();

            ImGui::TextDisabled( "%s", mDirty ? MKT_LOC( "script_graph_unsaved" ).c_str() : "" );

            constexpr const char* languages[]{ "C# (.NET)", "Lua", "C++" };
            ImGui::SetNextItemWidth( ImGui::GetFontSize() * 7.0f );
            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "inspector_language" ).c_str(), &mLanguageIndex, languages, IM_ARRAYSIZE( languages ) ) ) {
                mResult = {};
                mMessage.clear();
            }

            ImGui::SameLine();
            if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_generate_source" ).c_str() ) ) {
                GenerateScript( "GraphScript" );
            }

            ImGui::SameLine();
            if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_export_script" ).c_str() ) ) {
                ExportScript();
            }

            ImGui::TextDisabled( MKT_LOC( "script_graph_entityscript_api_authoring_transforms_target_the_owning_entity_local_space" ).c_str() );

            if ( !mMessage.empty() ) {
                ImGui::TextWrapped( "%s", mMessage.c_str() );
            }

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "script_graph_compiler_output", "Compiler output" ).c_str() ) ) {
                for ( const auto& diagnostic: mResult.mDiagnostics ) {
                    ImGui::TextWrapped( MKT_LOC( "script_graph_node" ).c_str(), static_cast<unsigned long long>( diagnostic.mNode ), diagnostic.mMessage.c_str() );
                }

                ImGui::BeginChild( "GeneratedScript", { 0, 140 }, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar );

                if ( !mResult.mSource.empty() ) {
                    if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "script_graph_copy_source" ).c_str() ) ) {
                        ImGui::SetClipboardText( mResult.mSource.c_str() );
                    }

                    ImGui::TextUnformatted( mResult.mSource.c_str() );
                }

                ImGui::EndChild();
            }

            const auto paneFlags{ ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp };

            if ( ImGui::BeginTable( "ScriptGraphPanes", mShowScriptPreview ? 3 : 2, paneFlags ) ) {
                ImGui::TableSetupColumn( MKT_LOC( "script_graph_script_api" ).c_str(), ImGuiTableColumnFlags_WidthFixed, 230.0f );
                ImGui::TableSetupColumn( MKT_LOC( "script_graph_graph" ).c_str(), ImGuiTableColumnFlags_WidthStretch, 2.0f );

                if ( mShowScriptPreview ) {
                    ImGui::TableSetupColumn( MKT_LOC( "script_graph_script_preview" ).c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::BeginChild( "ScriptClassPalette", { 0, 0 }, ImGuiChildFlags_Borders );
                // Apply visibility next frame so this table keeps its original column count.
                bool showPreview{ mShowScriptPreview };
                imgui::CheckBox( imgui::widget::LocalizedLabel( "script_graph_script_preview", "Script preview" ).c_str(), showPreview );
                ImGui::SeparatorText( MKT_LOC( "script_graph_script_api" ).c_str() );
                ImGui::TextWrapped( MKT_LOC( "script_graph_members_exposed_by_mikoto_s_entityscript_api_click_to_add" ).c_str() );
                auto member{ [&]( const char* label, scripting::ScriptNodeType type ) {
                    if ( ImGui::Selectable( label, false ) ) {
                        mGraph.AddNode( type, { 200, 100 } );
                        mRestore = true;
                        mDirty = true;
                        mResult = {};
                        mMessage.clear();
                    }
                } };

                if ( ImGui::TreeNodeEx( "EntityScript", ImGuiTreeNodeFlags_DefaultOpen ) ) {
                    member( "OnCreate()", scripting::ScriptNodeType::eBeginPlay );
                    member( "OnUpdate(float timeStep)", scripting::ScriptNodeType::eOnUpdate );

                    if ( ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "script_graph_transform_properties", "Transform properties" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                        member( MKT_LOC( "script_graph_get_position_vector3" ).c_str(), scripting::ScriptNodeType::eGetPosition );
                        member( MKT_LOC( "script_graph_set_position" ).c_str(), scripting::ScriptNodeType::eSetPosition );
                        member( MKT_LOC( "script_graph_get_rotation_vector3" ).c_str(), scripting::ScriptNodeType::eGetRotation );
                        member( MKT_LOC( "script_graph_set_rotation" ).c_str(), scripting::ScriptNodeType::eSetRotation );
                        member( MKT_LOC( "script_graph_get_scale_vector3" ).c_str(), scripting::ScriptNodeType::eGetScale );
                        member( MKT_LOC( "script_graph_set_scale" ).c_str(), scripting::ScriptNodeType::eSetScale );
                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }

                if ( ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "script_graph_input", "Input" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                    member( "IsKeyPressed(KeyCode)", scripting::ScriptNodeType::eKeyPressed );
                    ImGui::TreePop();
                }

                if ( ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "script_graph_log", "Log" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                    member( "Info(string message)", scripting::ScriptNodeType::ePrint );
                    ImGui::TreePop();
                }

                ImGui::EndChild();
                ImGui::TableSetColumnIndex( 1 );
                ImGui::BeginChild( "ScriptGraphView", { 0, 0 } );
                DrawGraph();
                ImGui::EndChild();

                if ( mShowScriptPreview ) {
                    ImGui::TableSetColumnIndex( 2 );
                    ImGui::BeginChild( "ScriptSourcePreview", { 0, 0 }, ImGuiChildFlags_Borders );
                    DrawScriptPreview();
                    ImGui::EndChild();
                }

                ImGui::EndTable();
                mShowScriptPreview = showPreview;
            }
        }

        ImGui::End();
    }

    auto ScriptGraphPanel::DrawScriptPreview() -> void {
        ImGui::SeparatorText( MKT_LOC( "script_graph_script_preview" ).c_str() );
        const auto selectSource{ [&]( const filesystem::Path& path ) {
            try {
                mPreviewRequest = asset::AssetRegistryService::Get()->Request<asset::ScriptSource>( asset::ScriptLoadDescription{ .mPath = path } );
                mPreviewPath = path;
                mPreviewLoaded = false;
                mPreviewError.clear();
            } catch ( const std::exception& error ) {
                mPreviewError = error.what();
            }
        } };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_load_script", "Load script..." ).c_str() ) ) {
            const auto path{ filesystem::OpenFileDialog( { { MKT_LOC( "script_graph_script_source" ).c_str(), "lua,cs,cpp" } } ) };

            if ( !path.IsEmpty() ) {
                selectSource( path );
            }
        }

        ImGui::SetNextItemWidth( -1 );

        if ( ImGui::BeginCombo( "##PreviewScript", mPreviewPath.IsEmpty() ? MKT_LOC( "script_graph_select_loaded_script" ).c_str() : mPreviewPath.GetC_Str() ) ) {
            for ( const auto& metadata: asset::AssetRegistryService::Get()->GetRegisteredScripts() ) {
                if ( ImGui::Selectable( metadata.GetPath().GetC_Str(), metadata.GetPath() == mPreviewPath ) ) {
                    selectSource( metadata.GetPath() );
                }
            }

            ImGui::EndCombo();
        }

        if ( ImGui::BeginDragDropTarget() ) {
            if ( const auto* payload{ ImGui::AcceptDragDropPayload( "MIKOTO_SCRIPT_SOURCE" ) } ) {
                if ( payload->DataSize > 1 && static_cast<const char*>( payload->Data )[payload->DataSize - 1] == '\0' ) {
                    selectSource( filesystem::Path{ static_cast<const char*>( payload->Data ) } );
                }
            }

            ImGui::EndDragDropTarget();
        }

        if ( !mPreviewError.empty() ) {
            ImGui::TextWrapped( "%s", mPreviewError.c_str() );
        }

        if ( mPreviewPath.IsEmpty() ) {
            ImGui::TextWrapped( MKT_LOC( "script_graph_load_a_source_file_or_select_a_loaded_script_to" ).c_str() );
            return;
        }

        if ( mPreviewRequest.GetState() == asset::AssetState::eUnloaded ) {
            selectSource( mPreviewPath );
        }

        if ( mPreviewRequest.GetState() == asset::AssetState::eFailed ) {
            ImGui::TextWrapped( "%s", mPreviewRequest.GetError().c_str() );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "script_graph_retry", "Retry" ).c_str() ) ) {
                asset::AssetRegistryService::Get()->ReloadScriptSource( mPreviewPath );
                selectSource( mPreviewPath );
            }

            return;
        }

        if ( !mPreviewLoaded ) {
            const auto source{ mPreviewRequest.GetIfReady() };

            if ( !source ) {
                ImGui::TextDisabled( MKT_LOC( "script_graph_loading" ).c_str() );
                return;
            }

            mScriptPreview.SetLanguage( mPreviewPath.GetNormalizedExtension() == "lua" ? imgui::TextLanguage::eLua : mPreviewPath.GetNormalizedExtension() == "cpp" ? imgui::TextLanguage::eCpp
                                                                                                                                                                    : imgui::TextLanguage::eCSharp );
            mScriptPreview.SetText( source->GetFile()->GetContentsString() );
            mScriptPreview.SetReadOnly( true );
            mScriptPreview.SetShowWhitespaces( false );
            mPreviewLoaded = true;
        }

        imgui::ImGuiScopedTextFont codeFont{ imgui::ImGuiService::Get()->PushFont( "./Resources/Fonts/Google_Sans_Code/static/GoogleSansCode-Light.ttf" ) };
        const auto cursor{ mScriptPreview.GetCursorPosition() };
        ImGui::Text( MKT_LOC( "script_graph_lines" ).c_str(), cursor.mLine + 1, cursor.mColumn + 1,
            mScriptPreview.GetTotalLines(), imgui::TextEditor::GetLanguageName( mScriptPreview.GetLanguage() ) );
        ImGui::Separator();
        mScriptPreview.Draw( "##ScriptGraphSource", ImGui::GetContentRegionAvail() );
    }

    auto ScriptGraphPanel::DrawGraph() -> void {
        imgui::NodeEditorScope scope{ mContext };
        const auto previous{ ed::GetStyle() };
        imgui::ApplyBlueprintStyle();
        bool edited{};
        ed::Begin( "ScriptGraphCanvas", { 0, 0 } );

        for ( auto& node: mGraph.GetNodes() ) {
            const ed::NodeId id{ node.mID * 16 };

            if ( mRestore ) {
                ed::SetNodePosition( id, { node.mPosition.x, node.mPosition.y } );
            }

            const bool error{ eastl::any_of( mResult.mDiagnostics.begin(), mResult.mDiagnostics.end(), [&]( const auto& d ) {
                return d.mNode == node.mID;
            } ) };
            ed::BeginNode( id );
            ImGui::PushID( static_cast<int>( node.mID ) );
            const auto header{ ImGui::GetCursorScreenPos() };
            ImGui::TextUnformatted( node.mName.c_str() );

            if ( !node.mBinding.empty() && ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( "%s", node.mBinding.c_str() );
            }

            ImGui::Dummy( { 240, 7 } );

            if ( node.mType == scripting::ScriptNodeType::eComment ) {
                ImGui::SetNextItemWidth( 260 );
                edited |= EditText( "##Comment", node.mName );
                ed::Group( { 300, 160 } );
            }
            else {
                const float left{ ImGui::GetCursorPosX() };
                ImGui::BeginGroup();

                for ( auto& pin: node.mPins ) {
                    const bool connected{ eastl::any_of( mGraph.GetLinks().begin(), mGraph.GetLinks().end(), [&]( const auto& l ) {
                        return l.mSource == pin.mID || l.mDestination == pin.mID;
                    } ) };
                    ed::BeginPin( pin.mID, pin.mInput ? ed::PinKind::Input : ed::PinKind::Output );

                    if ( !pin.mInput ) {
                        ImGui::SetCursorPosX( left + 340 );
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted( pin.mName.c_str() );
                        ImGui::SameLine();
                    }

                    imgui::DrawBlueprintPinIcon( pin.mType == scripting::ScriptGraphValueType::eFlow ? imgui::BlueprintPinShape::eFlow : imgui::BlueprintPinShape::eData, connected, Color( pin.mType ) );
                    const auto a{ ImGui::GetItemRectMin() };
                    const auto b{ ImGui::GetItemRectMax() };
                    const ImVec2 pivot{ ( a.x + b.x ) * .5f, ( a.y + b.y ) * .5f };
                    ed::PinPivotRect( pivot, pivot );

                    if ( pin.mInput ) {
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted( pin.mName.c_str() );
                    }

                    ed::EndPin();

                    if ( ( pin.mInput && !connected ) || ( node.mType == scripting::ScriptNodeType::eFloat || node.mType == scripting::ScriptNodeType::eVector3 ) ) {
                        ImGui::PushID( static_cast<int>( pin.mID ) );

                        if ( node.mType != scripting::ScriptNodeType::eFloat && node.mType != scripting::ScriptNodeType::eVector3 ) {
                            ImGui::SameLine( left + 110 );
                        }

                        ImGui::PushItemWidth( pin.mType == scripting::ScriptGraphValueType::eVector3 ? 210.0f : 200.0f );

                        if ( pin.mType == scripting::ScriptGraphValueType::eFloat ) {
                            edited |= ImGui::DragFloat( "##Value", &pin.mNumber, .01f );
                        }

                        if ( pin.mType == scripting::ScriptGraphValueType::eVector3 ) {
                            edited |= ImGui::DragFloat3( "##Value", &pin.mVector.x, .01f );
                        }

                        if ( pin.mType == scripting::ScriptGraphValueType::eKeyCode ) {
                            int key{ static_cast<int>( pin.mNumber ) };

                            if ( ImGui::InputInt( "##Key", &key ) ) {
                                pin.mNumber = static_cast<float>( glm::clamp( key, 0, 511 ) );
                                edited = true;
                            }

                            if ( ImGui::IsItemHovered() ) {
                                mikoto::imgui::widget::ItemTooltip( "Mikoto.KeyCode: I=73, J=74, K=75, L=76, Space=32" );
                            }
                        }

                        if ( pin.mType == scripting::ScriptGraphValueType::eBool ) {
                            edited |= imgui::CheckBox( "##Value", pin.mBoolean );
                        }

                        if ( pin.mType == scripting::ScriptGraphValueType::eString ) {
                            edited |= EditText( "##Value", pin.mText );
                        }

                        ImGui::PopItemWidth();
                        ImGui::PopID();
                    }
                }

                ImGui::EndGroup();
            }

            ImGui::PopID();
            ed::EndNode();

            if ( node.mType != scripting::ScriptNodeType::eComment ) {
                imgui::DrawBlueprintHeader( id, header, error ? ImVec4{ .6f, .15f, .15f, 1 } : node.mType == scripting::ScriptNodeType::eBeginPlay ? ImVec4{ .5f, .15f, .18f, 1 }
                                                                                                                                                   : ImVec4{ .18f, .3f, .42f, 1 } );
            }

            const auto position{ ed::GetNodePosition( id ) };

            if ( !mRestore && ( position.x != node.mPosition.x || position.y != node.mPosition.y ) ) {
                mDirty = true;
            }

            node.mPosition = { position.x, position.y };
        }

        for ( const auto& link: mGraph.GetLinks() ) {
            ed::Link( link.mID * 16 + 15, link.mSource, link.mDestination, Color( mGraph.FindPin( link.mSource )->mType ), 2 );
        }

        if ( ed::BeginCreate() ) {
            ed::PinId a{};
            ed::PinId b{};

            if ( ed::QueryNewLink( &a, &b ) && a && b ) {
                core::u64 source{ a.Get() };
                core::u64 destination{ b.Get() };

                if ( const auto* pin{ mGraph.FindPin( source ) }; pin && pin->mInput ) {
                    eastl::swap( source, destination );
                }

                if ( mGraph.CanConnect( source, destination ) ) {
                    if ( ed::AcceptNewItem() ) {
                        edited |= mGraph.Connect( source, destination );
                    }
                }
                else {
                    ed::RejectNewItem( { 1, .2f, .2f, 1 } );
                }
            }
        }

        ed::EndCreate();

        if ( ed::BeginDelete() ) {
            ed::LinkId link{};

            while ( ed::QueryDeletedLink( &link ) ) {
                if ( ed::AcceptDeletedItem() ) {
                    mGraph.Disconnect( link.Get() / 16 );
                    edited = true;
                }
            }

            ed::NodeId node{};

            while ( ed::QueryDeletedNode( &node ) ) {
                if ( ed::AcceptDeletedItem() ) {
                    mGraph.RemoveNode( node.Get() / 16 );
                    edited = true;
                }
            }
        }

        ed::EndDelete();
        ed::End();

        if ( mFit ) {
            ed::NavigateToContent();
            mFit = false;
        }

        mRestore = false;
        ed::GetStyle() = previous;

        if ( edited ) {
            mDirty = true;
            mResult = {};
            mMessage.clear();
        }
    }
}// namespace mikoto::editor
