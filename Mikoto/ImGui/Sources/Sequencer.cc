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

#include <imgui.h>

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <ImGui/Sequencer.hh>

namespace mikoto::imgui {

    using namespace mikoto::core;
    using namespace mikoto::animation;

    auto Sequencer::Reset() -> void {
        mSelectedTrack = 0;
        mSelectedFrame = -1;
        mMovingKey = false;
        mFirstFrame = 0.0f;
    }

    auto Sequencer::SelectKey( core::usize track, core::i32 frame ) -> void {
        mSelectedTrack = glm::min( track, static_cast<core::usize>( animation::KeyframeChannel::eCount ) - 1 );
        mSelectedFrame = frame;
    }

    auto Sequencer::GetSelectedTrack() const -> core::usize {
        return mSelectedTrack;
    }

    auto Sequencer::GetSelectedFrame() const -> core::i32 {
        return mSelectedFrame;
    }

    auto Sequencer::Draw( animation::KeyframeClip& clip, core::f64& frame, core::usize visibleTracks ) -> SequencerResult {
        MKT_BEGIN_PROFILER();
        SequencerResult result{};
        visibleTracks = glm::clamp( visibleTracks, core::usize{ 1 }, clip.mTracks.size() );
        mSelectedTrack = glm::min( mSelectedTrack, visibleTracks - 1 );

        const core::f32 scale{ ImGui::GetFontSize() / 14.0f };
        const core::f32 rowHeight{ 34.0f * scale };
        const core::f32 rulerHeight{ 30.0f * scale };

        const ImVec2 origin{ ImGui::GetCursorScreenPos() };

        const core::f32 width{ glm::max( ImGui::GetContentRegionAvail().x, 1.0f ) };
        const core::f32 labelWidth{ glm::min( 175.0f * scale, width * 0.38f ) };
        const core::f32 height{ rulerHeight + rowHeight * static_cast<core::f32>( visibleTracks ) + 12.0f * scale };
        const core::f32 timelineX{ origin.x + labelWidth };

        const ImVec2 maximum{ origin.x + width, origin.y + height };

        ImGui::InvisibleButton( "##KeyframeTimeline", { width, height },
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle );

        const bool hovered{ ImGui::IsItemHovered() };
        const bool active{ ImGui::IsItemActive() };
        const auto& io{ ImGui::GetIO() };
        const ImVec2 mouse{ io.MousePos };

        // Zoom around the frame under the pointer, so editing does not jump to a
        // different part of the clip. Middle-drag pans without changing the pose.
        if ( hovered && mouse.x >= timelineX && io.MouseWheel != 0.0f ) {
            const core::f32 anchor{ mFirstFrame + ( mouse.x - timelineX ) / mPixelsPerFrame };
            mPixelsPerFrame = glm::clamp( mPixelsPerFrame * glm::pow( 1.2f, io.MouseWheel ), 0.15f, 64.0f );
            mFirstFrame = anchor - ( mouse.x - timelineX ) / mPixelsPerFrame;
        }

        if ( active && ImGui::IsMouseDragging( ImGuiMouseButton_Middle, 0.0f ) ) {
            mFirstFrame -= io.MouseDelta.x / mPixelsPerFrame;
        }

        const core::f32 visibleFrames{ ( width - labelWidth ) / mPixelsPerFrame };
        mFirstFrame = glm::clamp( mFirstFrame, 0.0f, glm::max( 0.0f, static_cast<core::f32>( clip.mLastFrame ) - visibleFrames ) );

        auto frameAtMouse{ [&]() -> core::i32 {
            const core::f32 value{ mFirstFrame + ( mouse.x - timelineX ) / mPixelsPerFrame };
            return glm::clamp( static_cast<core::i32>( glm::round( value ) ), 0, clip.mLastFrame );
        } };

        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) {
            mMovingKey = false;
            const core::i32 row{ static_cast<core::i32>( glm::floor( ( mouse.y - origin.y - rulerHeight ) / rowHeight ) ) };

            if ( row >= 0 && row < static_cast<core::i32>( visibleTracks ) ) {
                mSelectedTrack = static_cast<core::usize>( row );
            }

            if ( mouse.x >= timelineX ) {
                mSelectedFrame = -1;
                core::i32 selected{ frameAtMouse() };

                if ( row >= 0 && row < static_cast<core::i32>( visibleTracks ) ) {
                    // Search by time, then inspect nearby keys for the closest
                    // diamond. This stays cheap even with long, densely keyed clips.
                    const auto keys{ clip.mTracks[mSelectedTrack].GetKeys() };

                    if ( !keys.empty() ) {
                        const auto nearby{ eastl::lower_bound( keys.data(), keys.data() + keys.size(), selected,
                            []( const animation::Keyframe& key, core::i32 value ) {
                            return key.mFrame < value;
                        } ) };
                        core::f32 distance{ 8.0f * scale };
                        auto consider{ [&]( const animation::Keyframe& key ) {
                            const core::f32 pixels{ glm::abs( timelineX + ( key.mFrame - mFirstFrame ) * mPixelsPerFrame - mouse.x ) };

                            if ( pixels <= distance ) {
                                distance = pixels;
                                mSelectedFrame = key.mFrame;
                            }
                        } };

                        if ( nearby != keys.data() + keys.size() ) {
                            consider( *nearby );
                        }

                        if ( nearby != keys.data() ) {
                            consider( *( nearby - 1 ) );
                        }
                    }

                    if ( mSelectedFrame >= 0 ) {
                        selected = mSelectedFrame;
                        mMovingKey = true;
                        mDragStartFrame = selected;
                        mDragStartX = mouse.x;
                    }
                    else if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                        result.mInsertKey = true;
                    }
                }

                frame = selected;
                result.mScrubbed = true;
            }
        }

        if ( active && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) && mouse.x >= timelineX ) {
            if ( mMovingKey ) {
                const core::i32 destination{ glm::clamp( mDragStartFrame + static_cast<core::i32>( glm::round( ( mouse.x - mDragStartX ) / mPixelsPerFrame ) ), 0, clip.mLastFrame ) };

                if ( destination != mSelectedFrame && clip.mTracks[mSelectedTrack].MoveKey( mSelectedFrame, destination ) ) {
                    mSelectedFrame = destination;
                    frame = destination;
                    result.mEdited = true;
                    result.mScrubbed = true;
                }
            }
            else {
                frame = frameAtMouse();
                result.mScrubbed = true;
            }
        }

        if ( !active ) {
            mMovingKey = false;
        }

        auto* draw{ ImGui::GetWindowDrawList() };
        draw->PushClipRect( origin, maximum, true );
        draw->AddRectFilled( origin, maximum, IM_COL32( 23, 24, 27, 255 ), 4.0f * scale );
        draw->AddRectFilled( origin, { maximum.x, origin.y + rulerHeight }, IM_COL32( 36, 38, 42, 255 ) );

        for ( core::usize track{}; track < visibleTracks; ++track ) {
            const core::f32 top{ origin.y + rulerHeight + static_cast<core::f32>( track ) * rowHeight };

            if ( track == mSelectedTrack ) {
                draw->AddRectFilled( { origin.x, top }, { maximum.x, top + rowHeight }, IM_COL32( 41, 49, 57, 255 ) );
            }

            draw->AddText( { origin.x + 10.0f * scale, top + ( rowHeight - ImGui::GetFontSize() ) * 0.5f },
                IM_COL32( 205, 208, 214, 255 ), animation::KeyframeClip::ChannelName( static_cast<animation::KeyframeChannel>( track ) ) );
            draw->AddLine( { origin.x, top + rowHeight }, { maximum.x, top + rowHeight }, IM_COL32( 47, 49, 54, 255 ) );
        }

        draw->AddLine( { timelineX, origin.y }, { timelineX, maximum.y }, IM_COL32( 66, 69, 75, 255 ) );
        draw->AddText( { origin.x + 10.0f * scale, origin.y + 6.0f * scale }, IM_COL32( 157, 162, 173, 255 ), "Property / frame" );
        draw->PushClipRect( { timelineX, origin.y }, maximum, true );
        core::i32 tickStep{ 1 };

        while ( tickStep * mPixelsPerFrame < 55.0f * scale ) {
            tickStep *= 2;
        }

        const core::i32 firstTick{ static_cast<core::i32>( mFirstFrame ) / tickStep * tickStep };

        for ( core::i32 tick{ firstTick }; tick <= clip.mLastFrame; tick += tickStep ) {
            const core::f32 x{ timelineX + ( tick - mFirstFrame ) * mPixelsPerFrame };

            if ( x > maximum.x ) {
                break;
            }

            draw->AddLine( { x, origin.y + rulerHeight - 6.0f * scale }, { x, maximum.y }, IM_COL32( 52, 55, 60, 255 ) );
            const auto text{ string::Format( "{}", tick ) };
            draw->AddText( { x + 4.0f * scale, origin.y + 6.0f * scale }, IM_COL32( 165, 173, 184, 255 ), text.c_str() );
        }

        const core::f32 diamond{ 5.5f * scale };

        for ( core::usize track{}; track < visibleTracks; ++track ) {
            const auto keys{ clip.mTracks[track].GetKeys() };

            if ( keys.empty() ) {
                continue;
            }

            const auto first{ eastl::lower_bound( keys.data(), keys.data() + keys.size(), mFirstFrame - diamond / mPixelsPerFrame,
                []( const animation::Keyframe& key, core::f32 value ) {
                return key.mFrame < value;
            } ) };

            for ( auto key{ first }; key != keys.data() + keys.size(); ++key ) {
                const core::f32 x{ timelineX + ( key->mFrame - mFirstFrame ) * mPixelsPerFrame };

                if ( x > maximum.x + diamond ) {
                    break;
                }

                const core::f32 y{ origin.y + rulerHeight + ( static_cast<core::f32>( track ) + 0.5f ) * rowHeight };
                const bool selected{ track == mSelectedTrack && key->mFrame == mSelectedFrame };
                draw->AddQuadFilled( { x, y - diamond }, { x + diamond, y }, { x, y + diamond }, { x - diamond, y },
                    selected ? IM_COL32( 255, 192, 92, 255 ) : IM_COL32( 104, 175, 206, 255 ) );
            }
        }

        const core::f32 playhead{ timelineX + static_cast<core::f32>( frame - mFirstFrame ) * mPixelsPerFrame };
        draw->AddLine( { playhead, origin.y }, { playhead, maximum.y }, IM_COL32( 246, 111, 94, 255 ), 2.0f * scale );
        draw->AddTriangleFilled( { playhead - 5.0f * scale, origin.y }, { playhead + 5.0f * scale, origin.y },
            { playhead, origin.y + 8.0f * scale }, IM_COL32( 246, 111, 94, 255 ) );
        draw->PopClipRect();
        draw->PopClipRect();
        return result;
    }
}// namespace mikoto::imgui