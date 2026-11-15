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

#include <cmath>

#include <imgui.h>

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/AnimatorPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace mikoto::scene;
    using namespace mikoto::animation;

    AnimatorPanel::AnimatorPanel( const AnimatorPanelCreateInfo& data )
        : Panel{ "Animator", "Panel", data.mGroup, data.mIsToggleable },
          mEditorState{ data.mState } {
        MKT_BEGIN_PROFILER();
        const eastl::string icon{ imgui::GetStringFromUnicode( 59677 ) };
        mPanelHeaderName = imgui::widget::MakeIconTitle( icon.c_str(), mPanelName );

        // This one is not visible on startup
        mPanelIsVisible = false;
    }

    auto AnimatorPanel::StopPreview() -> void {
        // Scene switches can destroy the old entity. Resolve its ID only while
        // the bound scene is still the active, live scene.
        if ( mBoundScene && mBoundScene == mEditorState->mActiveScene ) {
            if ( auto* entity{ mBoundScene->FindByID( mBoundEntity ) }; entity && entity->HasComponent<scene::KeyframeAnimationComponent>() ) {
                auto& animation{ entity->GetComponent<scene::KeyframeAnimationComponent>() };

                if ( animation.IsPreviewing() ) {
                    animation.Stop( *entity );
                }
            }
        }

        mBoundEntity = 0;
        mBoundScene = nullptr;
        mSequencer.Reset();
    }

    auto AnimatorPanel::OnRender( core::f32 ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            StopPreview();
            return;
        }

        auto* scene{ mEditorState->mActiveScene };
        const core::u64 selected{ mEditorState->mSelectedEntityID };

        if ( scene != mBoundScene || selected != mBoundEntity ) {
            StopPreview();
            mBoundScene = scene;
            mBoundEntity = selected;
        }

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            auto* entity{ scene ? scene->FindByID( selected ) : nullptr };

            if ( !entity ) {
                ImGui::TextDisabled( "%s", MKT_LOC( "animator_select_an_entity_to_create_or_edit_keyframe_animation" ).c_str() );
            }
            else {
                ImGui::TextUnformatted( entity->GetComponent<scene::TagComponent>().GetTag().c_str() );
                ImGui::Separator();
                ImGui::BeginDisabled( mEditorState->mRuntimeActive );

                if ( !entity->HasComponent<scene::KeyframeAnimationComponent>() ) {
                    ImGui::TextWrapped( "%s", MKT_LOC( "animator_animate_local_position_rotation_scale_and_light_properties_keys_are" ).c_str() );

                    if ( ImGui::Button( imgui::widget::LocalizedLabel( "animator_create_keyframe_animation", "Create keyframe animation" ).c_str() ) ) {
                        entity->AddComponent<scene::KeyframeAnimationComponent>();
                    }
                }

                if ( entity->HasComponent<scene::KeyframeAnimationComponent>() ) {
                    DrawAnimation( *entity, entity->GetComponent<scene::KeyframeAnimationComponent>() );
                }

                ImGui::EndDisabled();
            }
        }

        ImGui::End();

        if ( !mPanelIsVisible ) {
            StopPreview();
        }
    }

    auto AnimatorPanel::DrawAnimation( scene::Entity& entity, scene::KeyframeAnimationComponent& animation ) -> void {
        auto& clip{ animation.mClip };
        eastl::array<char, 256> name{};
        const core::usize length{ glm::min( clip.mName.size(), name.size() - 1 ) };
        eastl::copy_n( clip.mName.data(), length, name.data() );
        ImGui::SetNextItemWidth( 220.0f );

        if ( ImGui::InputText( imgui::widget::LocalizedLabel( "animator_clip", "Clip" ).c_str(), name.data(), name.size() ) ) {
            clip.mName = name.data();
        }

        ImGui::SetNextItemWidth( 100.0f );
        ImGui::DragInt( "FPS", &clip.mFrameRate, 1.0f, 1, 240, "%d", ImGuiSliderFlags_AlwaysClamp );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 120.0f );

        if ( ImGui::DragInt( imgui::widget::LocalizedLabel( "animator_last_frame", "Last frame" ).c_str(), &clip.mLastFrame, 1.0f, 1, animation::KeyframeClip::mMaxFrame, "%d", ImGuiSliderFlags_AlwaysClamp ) ) {
            if ( animation.IsPreviewing() ) {
                animation.Seek( entity, animation.GetFrame() );
            }
        }

        clip.Sanitize();
        imgui::CheckBox( imgui::widget::LocalizedLabel( "animator_loop", "Loop" ).c_str(), clip.mLoop );
        ImGui::SameLine();
        imgui::CheckBox( imgui::widget::LocalizedLabel( "animator_play_with_scene", "Play with scene" ).c_str(), clip.mPlayOnStart );

        if ( ImGui::Button( animation.IsPlaying() ? MKT_LOC( "animator_pause" ).c_str() : MKT_LOC( "animator_preview" ).c_str() ) ) {
            if ( animation.IsPlaying() ) {
                animation.Pause();
            }
            else {
                animation.Play( entity, true );
            }
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animator_stop_restore", "Stop / restore" ).c_str() ) ) {
            animation.Stop( entity );
        }

        ImGui::SameLine();
        core::i32 current{ static_cast<core::i32>( glm::round( animation.GetFrame() ) ) };
        ImGui::SetNextItemWidth( 100.0f );

        if ( ImGui::InputInt( imgui::widget::LocalizedLabel( "animator_frame", "Frame" ).c_str(), &current ) ) {
            animation.Pause();
            animation.Seek( entity, current );
            mSequencer.SelectKey( mSequencer.GetSelectedTrack(), -1 );
        }

        core::f64 cursor{ animation.GetFrame() };
        const core::usize trackCount{ entity.HasComponent<scene::LightComponent>() ? 5u : 3u };
        const auto interaction{ mSequencer.Draw( clip, cursor, trackCount ) };
        const core::usize trackIndex{ mSequencer.GetSelectedTrack() };
        auto& track{ clip.mTracks[trackIndex] };
        const auto channel{ static_cast<animation::KeyframeChannel>( trackIndex ) };

        if ( interaction.mInsertKey ) {
            const core::i32 frame{ static_cast<core::i32>( glm::round( cursor ) ) };
            track.SetKey( { .mFrame = frame, .mValue = scene::KeyframeAnimationComponent::ReadProperty( entity, channel ) } );
            mSequencer.SelectKey( trackIndex, frame );
        }

        if ( interaction.mScrubbed || interaction.mEdited ) {
            animation.Pause();
            animation.Seek( entity, cursor );
        }

        const core::i32 frame{ static_cast<core::i32>( glm::round( animation.GetFrame() ) ) };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animator_key_selected_property", "Key selected property" ).c_str() ) ) {
            track.SetKey( { .mFrame = frame, .mValue = scene::KeyframeAnimationComponent::ReadProperty( entity, channel ) } );
            mSequencer.SelectKey( trackIndex, frame );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animator_key_transform", "Key transform" ).c_str() ) ) {
            for ( core::usize index{}; index < 3; ++index ) {
                clip.mTracks[index].SetKey( { .mFrame = frame, .mValue = scene::KeyframeAnimationComponent::ReadProperty( entity, static_cast<animation::KeyframeChannel>( index ) ) } );
            }

            mSequencer.SelectKey( glm::min( trackIndex, core::usize{ 2 } ), frame );
        }

        ImGui::SameLine();
        const core::i32 selectedFrame{ mSequencer.GetSelectedFrame() };
        ImGui::BeginDisabled( !track.FindKey( selectedFrame ) );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animator_delete_key", "Delete key" ).c_str() ) ) {
            track.RemoveKey( selectedFrame );
            mSequencer.SelectKey( trackIndex, -1 );
            // Deleting the last key must not strand the object at a preview value.
            const core::f64 previous{ animation.GetFrame() };
            animation.Stop( entity );
            animation.Seek( entity, previous );
        }

        ImGui::EndDisabled();

        ImGui::SeparatorText( animation::KeyframeClip::ChannelName( channel ) );
        const core::i32 editFrame{ mSequencer.GetSelectedFrame() >= 0 ? mSequencer.GetSelectedFrame() : frame };
        const auto* existing{ track.FindKey( editFrame ) };
        animation::Keyframe edited{ existing ? *existing : animation::Keyframe{ .mFrame = editFrame, .mValue = scene::KeyframeAnimationComponent::ReadProperty( entity, channel ) } };
        bool changed{};

        if ( channel == animation::KeyframeChannel::eLightColor ) {
            changed = ImGui::ColorEdit3( imgui::widget::LocalizedLabel( "animator_value", "Value" ).c_str(), glm::value_ptr( edited.mValue ), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR );
        }
        else if ( channel == animation::KeyframeChannel::eLightIntensity ) {
            changed = ImGui::DragFloat( imgui::widget::LocalizedLabel( "animator_value", "Value" ).c_str(), &edited.mValue.x, 0.1f, 0.0f, 100000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp );
        }
        else {
            changed = ImGui::DragFloat3( imgui::widget::LocalizedLabel( "animator_value", "Value" ).c_str(), glm::value_ptr( edited.mValue ), 0.1f );
        }

        core::i32 interpolation{ static_cast<core::i32>( edited.mInterpolation ) };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "animator_to_next_key", "To next key" ).c_str(), &interpolation, MKT_LOC( "animator_step_linear_smooth" ).c_str() ) ) {
            edited.mInterpolation = static_cast<animation::KeyframeInterpolation>( interpolation );
            changed = true;
        }

        if ( changed && track.SetKey( edited ) ) {
            mSequencer.SelectKey( trackIndex, edited.mFrame );
            animation.Pause();
            animation.Seek( entity, edited.mFrame );
        }

        ImGui::TextDisabled( "%s", MKT_LOC( "animator_frame_seconds" ).c_str(), frame, animation.GetFrame() / clip.mFrameRate );
        ImGui::TextWrapped( "%s", MKT_LOC( "animator_select_a_row_and_frame_then_edit_value_to_create" ).c_str() );

        if ( entity.HasComponent<scene::RigidBodyComponent>() ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "animator_for_animated_physics_objects_use_a_kinematic_body_so_simulation" ).c_str() );
        }

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animator_remove_keyframe_animation", "Remove keyframe animation" ).c_str() ) ) {
            animation.Stop( entity );
            entity.RemoveComponent<scene::KeyframeAnimationComponent>();
            mSequencer.Reset();
        }
    }
}// namespace mikoto::editor
