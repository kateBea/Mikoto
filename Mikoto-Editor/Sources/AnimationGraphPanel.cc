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

#include <filesystem>

#include <EASTL/array.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Animation/AnimationSystem.hh>

#include <Scene/Component.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Panels/AnimationGraphPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::animation;

    namespace ed = ax::NodeEditor;

    /**
     * @brief Loads and validates a controller document for the animation graph.
     * @param path Controller JSON file to read; files larger than 4 MiB are rejected.
     * @param[out] controller Receives the validated controller on success.
     * @param[out] error Receives a diagnostic if loading fails.
     * @returns True when the controller is loaded successfully.
     */
    static auto LoadController( const std::filesystem::path& path, animation::AnimationController& controller, eastl::string& error ) -> bool {
        MKT_BEGIN_PROFILER();

        try {
            std::error_code status{};
            const auto size{ std::filesystem::file_size( path, status ) };

            if ( status || size > 4 * 1024 * 1024 ) {
                MKT_THROW_RUNTIME_ERROR( "Controller file is missing or exceeds 4 MiB." );
            }

            filesystem::File file{ filesystem::Path{ path } };
            const auto data{ nlohmann::json::parse( file.GetContentsString().c_str() ) };
            return controller.DecodeController( data, error );
        } catch ( const std::exception& exception ) {
            error = exception.what();
            return false;
        }
    }

    /**
     * @brief Validates and writes the animation graph's controller document.
     * @param path Destination controller JSON file.
     * @param controller Editable controller state to validate and save.
     * @param[out] error Receives a validation or file-writing diagnostic on failure.
     * @returns True after successfully writing the document.
     */
    static auto SaveController( const std::filesystem::path& path, const animation::AnimationController& controller, eastl::string& error ) -> bool {
        MKT_BEGIN_PROFILER();
        error = controller.Validate();

        if ( !error.empty() ) {
            return false;
        }

        try {
            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( filesystem::Path{ path } ) };

            if ( file.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot open animation controller file for writing." );
            }

            file->Write( controller.EncodeController().dump( 2 ).c_str() );
            return true;
        } catch ( const std::exception& exception ) {
            error = exception.what();
            return false;
        }
    }


    static auto DrawGraphSocket( ed::PinId id, ed::PinKind kind, const ImVec4& color ) -> void {
        ed::BeginPin( id, kind );
        imgui::DrawBlueprintPinIcon( imgui::BlueprintPinShape::eFlow, ed::HasAnyLinks( id ), color );
        const auto minimum{ ImGui::GetItemRectMin() };
        const auto maximum{ ImGui::GetItemRectMax() };
        const ImVec2 center{ ( minimum.x + maximum.x ) * .5f, ( minimum.y + maximum.y ) * .5f };
        ed::PinPivotRect( center, center );
        ed::EndPin();
    }

    static auto NodeID( animation::ControllerID id ) -> core::usize {
        MKT_BEGIN_PROFILER();
        return 100 + static_cast<core::usize>( id ) * 4;
    }

    static auto LinkID( animation::ControllerID id ) -> core::usize {
        MKT_BEGIN_PROFILER();
        return 103 + static_cast<core::usize>( id ) * 4;
    }

    static auto EditText( core::cstr label, eastl::string& text ) -> bool {
        MKT_BEGIN_PROFILER();
        eastl::array<char, 513> buffer{};
        std::snprintf( buffer.data(), buffer.size(), "%s", text.c_str() );

        if ( ImGui::InputText( label, buffer.data(), buffer.size() ) ) {
            text = buffer.data();
            return true;
        }

        return false;
    }

    static auto ExampleController() -> animation::AnimationController {
        MKT_BEGIN_PROFILER();
        animation::AnimationController result{};
        result.mName = "Locomotion";

        // Keep the example states arranged in the same order as their transitions.
        const auto grounded{ result.AddState( "Grounded" ).mID };
        const auto jumping{ result.AddState( "Jumping" ).mID };
        const auto hanging{ result.AddState( "Ledge Hang" ).mID };
        const auto falling{ result.AddState( "Falling" ).mID };
        const auto die{ result.AddState( "Die" ).mID };

        result.FindState( grounded )->mX = 300;
        result.FindState( grounded )->mY = 250;
        result.FindState( jumping )->mX = 300;
        result.FindState( jumping )->mY = 0;
        result.FindState( hanging )->mX = 640;
        result.FindState( hanging )->mY = 0;
        result.FindState( falling )->mX = 640;
        result.FindState( falling )->mY = 250;
        result.FindState( die )->mX = 300;
        result.FindState( die )->mY = 490;

        result.AddParameter( animation::ParameterType::eFloat ).mName = MKT_LOC( "animation_graph_forward" ).c_str();
        result.AddParameter( animation::ParameterType::eFloat ).mName = "Strafe";
        const auto fire{ result.AddParameter( animation::ParameterType::eBool ).mID };
        result.FindParameter( fire )->mName = "Fire";
        const auto dieTrigger{ result.AddParameter( animation::ParameterType::eTrigger ).mID };
        result.FindParameter( dieTrigger )->mName = "Die";

        // Death and jumping interrupt locomotion without waiting for an exit time.
        result.Connect( grounded, die );
        result.mTransitions.back().mHasExitTime = false;
        result.mTransitions.back().mConditions.push_back( { dieTrigger, animation::ConditionOperator::eEqual, 1 } );

        result.Connect( grounded, jumping );
        result.mTransitions.back().mHasExitTime = false;
        result.mTransitions.back().mConditions.push_back( { fire, animation::ConditionOperator::eEqual, 1 } );

        result.Connect( jumping, hanging );
        result.mTransitions.back().mConditions.push_back( { fire, animation::ConditionOperator::eEqual, 1 } );

        result.Connect( jumping, falling );
        result.mTransitions.back().mConditions.push_back( { fire, animation::ConditionOperator::eEqual, 0 } );

        result.Connect( hanging, falling );
        result.Connect( falling, grounded );

        return result;
    }


    AnimationGraphPanel::AnimationGraphPanel( const AnimationGraphPanelCreateInfo& info )
        : Panel{ "Animator Graph", "Animator Graph", info.mGroup, info.mIsToggleable },
          mEditorState{ info.mState },
          mScratch{ ExampleController() } {
        MKT_BEGIN_PROFILER();
        mPanelIsVisible = false;

        const eastl::string icon{ imgui::GetStringFromUnicode( 58896 ) };
        mPanelHeaderName = imgui::widget::MakeIconTitle( icon.c_str(), mPanelName );
    }

    auto AnimationGraphPanel::GetSelectedAnimator( core::u64& id ) const -> animation::Animator* {
        MKT_BEGIN_PROFILER();
        id = 0;
        auto* entity{ mEditorState ? mEditorState->mSelectedEntity : nullptr };

        if ( !entity || !entity->IsValid() || !entity->HasComponent<scene::AnimatorComponent>() ) {
            return nullptr;
        }

        id = entity->GetComponent<scene::AnimatorComponent>().GetAnimatorID();
        auto* animator{ animation::AnimationSystem::Get()->GetAnimator( id ) };

        if ( !animator ) {
            id = 0;
        }

        return animator;
    }

    auto AnimationGraphPanel::Changed( DocumentView& document, animation::Animator* animator ) -> void {
        MKT_BEGIN_PROFILER();
        document.mDirty = true;

        if ( animator ) {
            animator->StopController();
        }
    }

    auto AnimationGraphPanel::Save( animation::AnimationController& controller, DocumentView& document, bool saveAs ) -> void {
        MKT_BEGIN_PROFILER();
        auto path{ document.mPath };

        if ( saveAs || path.empty() ) {
            std::initializer_list<filesystem::FileDialogPair> filters{ { MKT_LOC( "animation_graph_mikoto_animator_controller" ).c_str(), "mkanim" } };
            const auto selected{ filesystem::SaveFileDialog( "Controller.mkanim", filters ) };

            if ( selected.IsEmpty() ) {
                return;
            }

            path = std::filesystem::u8path( selected.GetC_Str() );

            if ( path.extension().empty() ) {
                path += ".mkanim";
            }
        }

        if ( SaveController( path, controller, mMessage ) ) {
            document.mPath = path;
            document.mDirty = false;
            mMessage = MKT_LOC( "animation_graph_controller_saved" ).c_str();
        }
    }

    auto AnimationGraphPanel::Open( animation::AnimationController& controller, DocumentView& document, animation::Animator* animator ) -> void {
        MKT_BEGIN_PROFILER();
        const std::initializer_list<filesystem::FileDialogPair> filters{
            { .mDescription = MKT_LOC( "animation_graph_mikoto_animator_controller" ).c_str(), .mPattern = "mkanim" }
        };
        const auto selected{ filesystem::OpenFileDialog( filters ) };

        if ( selected.IsEmpty() ) {
            return;
        }

        animation::AnimationController candidate{};
        const auto path{ std::filesystem::u8path( selected.GetC_Str() ) };

        if ( !LoadController( path, candidate, mMessage ) ) {
            return;
        }

        if ( animator ) {
            animator->StopController();
        }

        controller = eastl::move( candidate );
        document.mPath = path;
        document.mDirty = false;
        mSelectedState = mSelectedTransition = 0;
        ed::ClearSelection();
        mRestorePositions = mFitRequested = true;
        mMessage = MKT_LOC( "animation_graph_controller_loaded" ).c_str();
    }

    auto AnimationGraphPanel::DrawToolbar( animation::AnimationController& controller, DocumentView& document, animation::Animator* animator ) -> void {
        MKT_BEGIN_PROFILER();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_open", "Open..." ).c_str() ) ) {
            if ( document.mDirty ) {
                ImGui::OpenPopup( imgui::widget::LocalizedLabel( "animation_graph_unsaved_controller", "Unsaved controller" ).c_str() );
            }
            else {
                Open( controller, document, animator );
            }
        }

        if ( ImGui::BeginPopupModal( imgui::widget::LocalizedLabel( "animation_graph_unsaved_controller", "Unsaved controller" ).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
            ImGui::TextUnformatted( MKT_LOC( "animation_graph_save_your_changes_before_opening_another_controller" ).c_str() );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_save_and_open", "Save and open" ).c_str() ) ) {
                Save( controller, document, false );

                if ( !document.mDirty ) {
                    ImGui::CloseCurrentPopup();
                    Open( controller, document, animator );
                }
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_discard_and_open", "Discard and open" ).c_str() ) ) {
                ImGui::CloseCurrentPopup();
                Open( controller, document, animator );
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_cancel", "Cancel" ).c_str() ) ) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_save", "Save" ).c_str() ) ) {
            Save( controller, document, false );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_save_as", "Save as..." ).c_str() ) ) {
            Save( controller, document, true );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_state", "+ State" ).c_str() ) ) {
            mSelectedState = controller.AddState().mID;
            mSelectedTransition = 0;
            mRestorePositions = true;
            Changed( document, animator );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_fit_graph", "Fit graph" ).c_str() ) ) {
            mFitRequested = true;
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_auto_layout", "Auto layout" ).c_str() ) ) {
            for ( core::usize i{}; i < controller.mStates.size(); ++i ) {
                controller.mStates[i].mX = 300.0f + ( i % 3 ) * 300.0f;
                controller.mStates[i].mY = ( i / 3 ) * 170.0f;
            }

            mRestorePositions = mFitRequested = true;
            document.mDirty = true;
        }

        ImGui::SameLine();
        const bool hasClips{ animator && !animator->GetAnimationList().empty() };
        ImGui::BeginDisabled( !hasClips );
        const bool playing{ animator && animator->GetControllerPlayback().IsPlaying() };

        if ( ImGui::Button( playing ? MKT_LOC( "animation_graph_stop_controller" ).c_str() : MKT_LOC( "animation_graph_play_controller" ).c_str() ) ) {
            if ( playing ) {
                animator->StopController();
            }
            else if ( !animator->PlayController() ) {
                mMessage = MKT_LOC( "animation_graph_assign_a_valid_clip_to_every_state_and_resolve_controller" ).c_str();
            }
            else {
                mMessage.clear();
            }
        }

        ImGui::EndDisabled();

        ImGui::SetNextItemWidth( 220 );

        if ( EditText( MKT_LOC( "animation_graph_controller" ).c_str(), controller.mName ) ) {
            document.mDirty = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled( MKT_LOC( "animation_graph_base_layer" ).c_str(), animator ? MKT_LOC( "animation_graph_selected_model" ).c_str() : MKT_LOC( "animation_graph_unbound_controller" ).c_str(), document.mDirty ? MKT_LOC( "animation_graph_unsaved" ).c_str() : "" );

        if ( !animator ) {
            ImGui::TextWrapped( MKT_LOC( "animation_graph_select_an_entity_with_an_animator_component_to_bind_clips" ).c_str() );
        }

        if ( animator && !hasClips ) {
            ImGui::TextWrapped( MKT_LOC( "animation_graph_this_model_has_no_animation_clips_pose_its_skeleton_using" ).c_str() );
        }

        if ( hasClips && controller.mStates.empty() ) {
            if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_create_states_from_animation_clips", "Create states from animation clips" ).c_str() ) ) {
                for ( const auto& clipEntry: animator->GetAnimationList() ) {
                    const auto& name{ clipEntry.first };

                    const eastl::string clipName{ name.c_str() };
                    controller.AddState( clipName ).mClip = clipName;
                }

                Changed( document, animator );
                mRestorePositions = mFitRequested = true;
            }
        }

        const auto error{ controller.Validate() };

        if ( !error.empty() ) {
            ImGui::TextColored( { 1, 0.55f, 0.25f, 1 }, "%s", error.c_str() );
        }

        if ( !mMessage.empty() ) {
            ImGui::TextWrapped( "%s", mMessage.c_str() );
        }

        ImGui::Separator();
    }

    auto AnimationGraphPanel::DrawParameters( animation::AnimationController& controller, DocumentView& document ) -> void {
        MKT_BEGIN_PROFILER();
        ImGui::SeparatorText( MKT_LOC( "animation_graph_layers" ).c_str() );
        ImGui::Selectable( imgui::widget::LocalizedLabel( "animation_graph_base_layer", "Base Layer" ).c_str(), true );
        ImGui::SeparatorText( MKT_LOC( "animation_graph_parameters" ).c_str() );
        ImGui::SetNextItemWidth( 120 );
        ImGui::Combo( "##ParameterType", &mNewParameterType, MKT_LOC( "animation_graph_float_integer_bool_trigger" ).c_str() );
        ImGui::SameLine();

        if ( ImGui::Button( "+" ) ) {
            controller.AddParameter( static_cast<animation::ParameterType>( mNewParameterType ) );
            document.mDirty = true;
        }

        animation::ControllerID removed{};

        for ( auto& parameter: controller.mParameters ) {
            ImGui::PushID( static_cast<core::i32>( parameter.mID ) );
            ImGui::SetNextItemWidth( -30 );

            if ( EditText( "##Name", parameter.mName ) ) {
                document.mDirty = true;
            }

            ImGui::SameLine();

            if ( ImGui::SmallButton( "x" ) ) {
                removed = parameter.mID;
            }

            ImGui::SetNextItemWidth( -1 );
            bool changed{};

            switch ( parameter.mType ) {
                case animation::ParameterType::eFloat:
                    changed = ImGui::DragFloat( "##Value", &parameter.mFloatValue, 0.01f );
                    break;
                case animation::ParameterType::eInteger:
                    changed = ImGui::InputInt( "##Value", &parameter.mIntValue );
                    break;
                case animation::ParameterType::eBool:
                    changed = imgui::CheckBox( imgui::widget::LocalizedLabel( "animation_graph_enabled", "Enabled" ).c_str(), parameter.mBoolValue );
                    break;
                case animation::ParameterType::eTrigger:
                    if ( ImGui::Button( parameter.mBoolValue ? MKT_LOC( "animation_graph_reset_trigger" ).c_str() : MKT_LOC( "animation_graph_fire_trigger" ).c_str() ) ) {
                        parameter.mBoolValue = !parameter.mBoolValue;
                    }

                    break;
            }

            document.mDirty |= changed;
            ImGui::Separator();
            ImGui::PopID();
        }

        if ( removed ) {
            controller.RemoveParameter( removed );
            document.mDirty = true;
        }

        ImGui::TextWrapped( MKT_LOC( "animation_graph_drag_an_output_pin_to_another_state_s_input_to" ).c_str() );
    }

    auto AnimationGraphPanel::DrawGraph( animation::AnimationController& controller, DocumentView& document, animation::Animator* animator ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRestorePositions ) {
            mFitDelay = 2;
        }

        const auto previousStyle{ ed::GetStyle() };

        imgui::ApplyBlueprintStyle();

        ImGui::PushStyleColor( ImGuiCol_Text, { 0.78f, 0.79f, 0.8f, 1 } );
        ImGui::PushStyleColor( ImGuiCol_TextDisabled, { 0.58f, 0.6f, 0.62f, 1 } );
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 8, 6 } );

        ed::Begin( "AnimatorCanvas" );
        ed::PushStyleColor( ed::StyleColor_NodeBg, { 0.13f, 0.14f, 0.13f, 1 } );
        ed::PushStyleColor( ed::StyleColor_NodeBorder, { 0.23f, 0.29f, 0.24f, 1 } );
        ed::BeginNode( 1 );

        const auto entryStart{ ImGui::GetCursorScreenPos() };
        ImGui::TextColored( { 0.62f, 0.78f, 0.62f, 1 }, MKT_LOC( "animation_graph_entry" ).c_str() );
        ImGui::Dummy( { 180, 0 } );
        ImGui::TextUnformatted( MKT_LOC( "animation_graph_default_state" ).c_str() );
        ImGui::SameLine();

        DrawGraphSocket( 2, ed::PinKind::Output, { 0.49f, 0.72f, 0.48f, 1 } );

        ed::EndNode();

        imgui::DrawBlueprintHeader( 1, entryStart, { 0.55f, 0.19f, 0.20f, 1 } );

        ed::PopStyleColor( 2 );

        if ( mRestorePositions ) {
            ed::SetNodePosition( 1, { 0, 250 } );
        }

        const auto active{ animator ? animator->GetControllerPlayback().GetState() : animation::ControllerID{} };

        for ( auto& state: controller.mStates ) {
            const bool isDefault{ state.mID == controller.mDefaultState };
            ed::PushStyleColor( ed::StyleColor_NodeBg, { .20f, .20f, .21f, 1 } );
            ed::PushStyleColor( ed::StyleColor_NodeBorder, active == state.mID ? ImVec4{ 0.43f, 0.72f, 0.51f, 1 } : ImVec4{ .45f, .45f, .47f, 1 } );
            ed::PushStyleVar( ed::StyleVar_NodeBorderWidth, active == state.mID ? 1.5f : 1.0f );
            ed::BeginNode( NodeID( state.mID ) );
            const auto headerStart{ ImGui::GetCursorScreenPos() };
            ImGui::TextColored( { 0.84f, 0.85f, 0.86f, 1 }, "%s", state.mName.c_str() );
            ImGui::Dummy( { 210, 0 } );
            ImGui::TextDisabled( "%s", state.mClip.empty() ? MKT_LOC( "animation_graph_no_animation_clip" ).c_str() : state.mClip.c_str() );

            DrawGraphSocket( NodeID( state.mID ) + 1, ed::PinKind::Input, { .92f, .92f, .92f, 1 } );

            ImGui::SameLine();
            ImGui::TextUnformatted( "In" );
            ImGui::SameLine( 160 );
            ImGui::TextUnformatted( MKT_LOC( "animation_graph_out" ).c_str() );
            ImGui::SameLine();

            DrawGraphSocket( NodeID( state.mID ) + 2, ed::PinKind::Output, { .92f, .92f, .92f, 1 } );

            if ( isDefault ) {
                ImGui::TextColored( { 0.76f, 0.65f, 0.46f, 1 }, MKT_LOC( "animation_graph_default" ).c_str() );
            }

            if ( active == state.mID ) {
                ImGui::TextColored( { 0.55f, 0.8f, 0.61f, 1 }, MKT_LOC( "animation_graph_playing" ).c_str() );
            }

            ed::EndNode();
            imgui::DrawBlueprintHeader( NodeID( state.mID ), headerStart, { 0.18f, 0.34f, 0.52f, 1 } );

            ed::PopStyleVar();
            ed::PopStyleColor( 2 );

            if ( mRestorePositions ) {
                ed::SetNodePosition( NodeID( state.mID ), { state.mX, state.mY } );
            }

            const auto position{ ed::GetNodePosition( NodeID( state.mID ) ) };

            if ( glm::abs( position.x - state.mX ) > 0.01f || glm::abs( position.y - state.mY ) > 0.01f ) {
                state.mX = position.x;
                state.mY = position.y;
                document.mDirty = true;
            }
        }

        mRestorePositions = false;

        if ( controller.FindState( controller.mDefaultState ) ) {
            ed::Link( 3, 2, NodeID( controller.mDefaultState ) + 1, { 0.49f, 0.72f, 0.48f, 1 }, 1.5f );
        }

        for ( const auto& transition: controller.mTransitions ) {
            ed::Link( LinkID( transition.mID ), NodeID( transition.mFrom ) + 2, NodeID( transition.mTo ) + 1,
                transition.mEnabled ? ImVec4{ .92f, .92f, .92f, 1 } : ImVec4{ 0.34f, 0.34f, 0.35f, 1 }, 1.5f );

            if ( animator && animator->GetControllerPlayback().GetLastTransition() == transition.mID ) {
                ed::Flow( LinkID( transition.mID ) );
            }
        }

        if ( ed::BeginCreate() ) {
            ed::PinId start{};
            ed::PinId end{};

            if ( ed::QueryNewLink( &start, &end ) && start && end ) {
                auto fromPin{ start.Get() };
                auto toPin{ end.Get() };

                for ( const auto& state: controller.mStates ) {
                    if ( fromPin == NodeID( state.mID ) + 1 ) {
                        eastl::swap( fromPin, toPin );
                    }
                }

                animation::ControllerID from{};
                animation::ControllerID to{};

                for ( const auto& state: controller.mStates ) {
                    if ( fromPin == NodeID( state.mID ) + 2 ) {
                        from = state.mID;
                    }

                    if ( toPin == NodeID( state.mID ) + 1 ) {
                        to = state.mID;
                    }
                }

                const bool entry{ fromPin == 2 };

                if ( to && ( entry || controller.CanConnect( from, to ) ) ) {
                    if ( ed::AcceptNewItem() ) {
                        if ( entry ) {
                            controller.mDefaultState = to;
                        }
                        else {
                            controller.Connect( from, to );
                        }

                        Changed( document, animator );
                    }
                }
                else {
                    ed::RejectNewItem( { 1, 0.3f, 0.3f, 1 }, 2 );
                }
            }
        }

        ed::EndCreate();

        if ( ed::BeginDelete() ) {
            ed::LinkId link{};

            while ( ed::QueryDeletedLink( &link ) ) {
                if ( link.Get() == 3 ) {
                    ed::RejectDeletedItem();
                    continue;
                }

                if ( ed::AcceptDeletedItem() ) {
                    eastl::erase_if( controller.mTransitions, [&]( const auto& t ) {
                        return LinkID( t.mID ) == link.Get();
                    } );
                    Changed( document, animator );
                }
            }

            ed::NodeId node{};

            while ( ed::QueryDeletedNode( &node ) ) {
                if ( node.Get() == 1 ) {
                    ed::RejectDeletedItem();
                    continue;
                }

                if ( ed::AcceptDeletedItem() ) {
                    const auto found{
                        eastl::find_if( controller.mStates.begin(),
                            controller.mStates.end(),
                            [&]( const auto& s ) {
                        return NodeID( s.mID ) == node.Get();
                    } )
                    };

                    if ( found != controller.mStates.end() ) {
                        controller.RemoveState( found->mID );
                    }

                    Changed( document, animator );
                }
            }
        }

        ed::EndDelete();

        if ( ed::HasSelectionChanged() ) {
            ed::NodeId node{};
            ed::LinkId link{};
            mSelectedState = mSelectedTransition = 0;

            if ( ed::GetSelectedNodes( &node, 1 ) ) {
                for ( const auto& state: controller.mStates ) {
                    if ( NodeID( state.mID ) == node.Get() ) {
                        mSelectedState = state.mID;
                    }
                }
            }

            if ( ed::GetSelectedLinks( &link, 1 ) ) {
                for ( const auto& transition: controller.mTransitions ) {
                    if ( LinkID( transition.mID ) == link.Get() ) {
                        mSelectedTransition = transition.mID;
                    }
                }
            }
        }

        ed::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor( 2 );
        ed::GetStyle() = previousStyle;

        if ( mFitRequested ) {
            if ( mFitDelay > 0 ) {
                --mFitDelay;
            }
            else {
                ed::NavigateToContent( 0 );
                mFitRequested = false;
            }
        }
    }

    auto AnimationGraphPanel::DrawInspector( animation::AnimationController& controller, DocumentView& document, animation::Animator* animator ) -> void {
        MKT_BEGIN_PROFILER();
        // Rigs are imported bindings, not independent playback targets. Show
        // them here so split meshes do not imply several competing controllers.
        if ( animator ) {
            if ( const auto model{ animator->GetModel() }; model && !model->GetRigs().empty() ) {
                if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "animation_graph_bound_skeletons", "Bound skeletons" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                    ImGui::TextWrapped( MKT_LOC( "animation_graph_this_character_s_graph_plays_and_blends_all_skeletons_together" ).c_str() );

                    for ( const auto& rig: model->GetRigs() ) {
                        ImGui::BulletText( "%s", rig.mName.c_str() );
                        ImGui::TextDisabled( MKT_LOC( "animation_graph_animation_clips" ).c_str(), rig.mAnimations.size() );
                    }
                }

                ImGui::Separator();
            }
        }

        if ( auto* state = controller.FindState( mSelectedState ) ) {
            ImGui::SeparatorText( MKT_LOC( "animation_graph_state" ).c_str() );
            ImGui::SetNextItemWidth( -1 );

            if ( EditText( "##StateName", state->mName ) ) {
                Changed( document, animator );
            }

            ImGui::TextUnformatted( MKT_LOC( "animation_graph_animation_clip" ).c_str() );
            ImGui::SetNextItemWidth( -1 );

            if ( animator ) {
                if ( ImGui::BeginCombo( "##Clip", state->mClip.empty() ? MKT_LOC( "animation_graph_choose_a_clip" ).c_str() : state->mClip.c_str() ) ) {
                    for ( const auto& clipEntry: animator->GetAnimationList() ) {
                        const auto& name{ clipEntry.first };

                        core::cstr clipName{ name.c_str() };

                        if ( ImGui::Selectable( clipName, state->mClip == clipName ) ) {
                            state->mClip = clipName;
                            Changed( document, animator );
                        }
                    }

                    ImGui::EndCombo();
                }

                const auto& clips{ animator->GetAnimationList() };

                if ( state->mClip.empty() || eastl::none_of( clips.begin(), clips.end(), [&]( const auto& item ) {
                    return state->mClip == item.first.c_str();
                } ) ) {
                    ImGui::TextWrapped( MKT_LOC( "animation_graph_choose_a_clip_available_on_this_model_before_playing" ).c_str() );
                }
            }
            else if ( EditText( "##Clip", state->mClip ) ) {
                document.mDirty = true;
            }

            if ( state->mID == controller.mDefaultState ) {
                ImGui::TextColored( { 1, 0.68f, 0.22f, 1 }, MKT_LOC( "animation_graph_default_state" ).c_str() );
            }
            else if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_set_as_default", "Set as default" ).c_str() ) ) {
                controller.mDefaultState = state->mID;
                Changed( document, animator );
            }

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_delete_state", "Delete state" ).c_str() ) ) {
                controller.RemoveState( state->mID );
                mSelectedState = 0;
                Changed( document, animator );
            }

            return;
        }

        const auto found{ eastl::find_if( controller.mTransitions.begin(), controller.mTransitions.end(), [this]( const auto& t ) {
            return t.mID == mSelectedTransition;
        } ) };

        if ( found == controller.mTransitions.end() ) {
            ImGui::TextWrapped( MKT_LOC( "animation_graph_select_a_state_or_transition_to_edit_it_green_marks" ).c_str() );
            return;
        }

        auto& transition{ *found };
        ImGui::SeparatorText( MKT_LOC( "animation_graph_transition" ).c_str() );
        ImGui::TextWrapped( "%s -> %s", controller.FindState( transition.mFrom )->mName.c_str(), controller.FindState( transition.mTo )->mName.c_str() );
        bool changed{ imgui::CheckBox( imgui::widget::LocalizedLabel( "animation_graph_enabled", "Enabled" ).c_str(), transition.mEnabled ) };
        changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "animation_graph_blend_duration", "Blend duration" ).c_str(), &transition.mBlendDuration, 0.01f, 0.0f, 30.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp );
        changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "animation_graph_has_exit_time", "Has exit time" ).c_str(), transition.mHasExitTime );

        if ( transition.mHasExitTime ) {
            changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "animation_graph_exit_time", "Exit time" ).c_str(), &transition.mExitTime, 0.01f, 0, 10, MKT_LOC( "animation_graph_loops" ).c_str(), ImGuiSliderFlags_AlwaysClamp );
            ImGui::TextWrapped( MKT_LOC( "animation_graph_1_0_waits_for_one_loop_conditions_must_also_pass" ).c_str() );
        }

        ImGui::SeparatorText( MKT_LOC( "animation_graph_conditions_all_required" ).c_str() );
        ImGui::BeginDisabled( controller.mParameters.empty() );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_condition", "+ Condition" ).c_str() ) ) {
            transition.mConditions.push_back( { controller.mParameters.front().mID } );
            changed = true;
        }

        ImGui::EndDisabled();
        core::i32 removed{ -1 };

        for ( core::i32 i{}; i < static_cast<core::i32>( transition.mConditions.size() ); ++i ) {
            auto& condition{ transition.mConditions[i] };
            ImGui::PushID( i );
            auto* parameter{ controller.FindParameter( condition.mParameter ) };
            ImGui::SetNextItemWidth( -30 );

            if ( ImGui::BeginCombo( "##Parameter", parameter ? parameter->mName.c_str() : MKT_LOC( "animation_graph_missing_parameter" ).c_str() ) ) {
                for ( const auto& p: controller.mParameters ) {
                    if ( ImGui::Selectable( p.mName.c_str(), p.mID == condition.mParameter ) ) {
                        condition.mParameter = p.mID;
                        condition.mThreshold = 0;
                        condition.mComparison = animation::ConditionOperator::eEqual;
                        changed = true;
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::SameLine();

            if ( ImGui::SmallButton( "x" ) ) {
                removed = i;
            }

            parameter = controller.FindParameter( condition.mParameter );

            if ( parameter && parameter->mType == animation::ParameterType::eTrigger ) {
                ImGui::TextDisabled( MKT_LOC( "animation_graph_consumes_trigger_when_taken" ).c_str() );
            }
            else if ( parameter && parameter->mType == animation::ParameterType::eBool ) {
                bool expected{ condition.mComparison == animation::ConditionOperator::eNotEqual ? condition.mThreshold == 0 : condition.mThreshold != 0 };

                if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "animation_graph_is_true", "Is true" ).c_str(), expected ) ) {
                    condition.mComparison = animation::ConditionOperator::eEqual;
                    condition.mThreshold = expected ? 1 : 0;
                    changed = true;
                }
            }
            else {
                core::i32 op{ static_cast<core::i32>( condition.mComparison ) };
                ImGui::SetNextItemWidth( 90 );

                if ( ImGui::Combo( "##Comparison", &op, "==\0!=\0>\0<\0>=\0<=\0" ) ) {
                    condition.mComparison = static_cast<animation::ConditionOperator>( op );
                    changed = true;
                }

                ImGui::SameLine();
                ImGui::SetNextItemWidth( -1 );
                changed |= ImGui::InputDouble( "##Threshold", &condition.mThreshold );
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if ( removed >= 0 ) {
            transition.mConditions.erase( transition.mConditions.begin() + removed );
            changed = true;
        }

        ImGui::TextWrapped( MKT_LOC( "animation_graph_transitions_are_checked_in_list_order_the_first_matching_outgoing" ).c_str() );
        const auto index{ static_cast<core::usize>( found - controller.mTransitions.begin() ) };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_raise_priority", "Raise priority" ).c_str() ) && index > 0 ) {
            eastl::swap( controller.mTransitions[index], controller.mTransitions[index - 1] );
            changed = true;
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animation_graph_delete", "Delete" ).c_str() ) ) {
            eastl::erase_if( controller.mTransitions, [this]( const auto& t ) {
                return t.mID == mSelectedTransition;
            } );
            mSelectedTransition = 0;
            changed = true;
        }

        if ( changed ) {
            Changed( document, animator );
        }
    }

    auto AnimationGraphPanel::OnRender( core::f32 ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 1250, 740 }, ImGuiCond_FirstUseEver );

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) ) {
            mPanelIsFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );
            mPanelIsHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_RootAndChildWindows );

            imgui::NodeEditorScope scope{ mContext };
            core::u64 animatorID{};
            auto* animator{ GetSelectedAnimator( animatorID ) };
            auto& controller{ animator ? animator->GetController() : mScratch };
            auto& document{ mDocuments[animatorID] };

            if ( mLastAnimatorID != animatorID ) {
                ed::ClearSelection();
                mSelectedState = mSelectedTransition = 0;
                mRestorePositions = mFitRequested = true;
                mLastAnimatorID = animatorID;
                mMessage.clear();
            }

            DrawToolbar( controller, document, animator );

            const core::f32 width{ ImGui::GetContentRegionAvail().x };
            const core::f32 left{ eastl::min( 220.0f, width * 0.22f ) };
            const core::f32 right{ eastl::min( 290.0f, width * 0.28f ) };
            ImGui::BeginChild( "AnimatorParameters", { left, 0 }, ImGuiChildFlags_Borders );

            DrawParameters( controller, document );

            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild( "AnimatorGraph", { eastl::max( 1.0f, ImGui::GetContentRegionAvail().x - right - ImGui::GetStyle().ItemSpacing.x ), 0 } );

            DrawGraph( controller, document, animator );

            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild( "AnimatorInspector", { 0, 0 }, ImGuiChildFlags_Borders );

            DrawInspector( controller, document, animator );

            ImGui::EndChild();
        }

        ImGui::End();
    }
}// namespace mikoto::editor
