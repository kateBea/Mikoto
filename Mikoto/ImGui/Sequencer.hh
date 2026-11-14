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

#ifndef MIKOTO_IMGUI_SEQUENCER_HH
#define MIKOTO_IMGUI_SEQUENCER_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Animation/KeyframeClip.hh>

namespace mikoto::imgui {
    struct SequencerResult {
        bool mScrubbed{};
        bool mEdited{};
        bool mInsertKey{};
    };

    // Timeline interaction/drawing only. The client owns the clip, previews the
    // object, and supplies values when a key is inserted.
    class Sequencer {
    public:
        auto Draw( animation::KeyframeClip& clip, core::f64& frame, core::usize visibleTracks ) -> SequencerResult;

        auto Reset() -> void;
        auto SelectKey( core::usize track, core::i32 frame ) -> void;

        MKT_NODISCARD auto GetSelectedTrack() const -> core::usize;
        MKT_NODISCARD auto GetSelectedFrame() const -> core::i32;

    private:
        core::usize mSelectedTrack{};
        core::i32 mSelectedFrame{ -1 };
        core::i32 mDragStartFrame{};
        core::f32 mDragStartX{};
        core::f32 mPixelsPerFrame{ 8.0f };
        core::f32 mFirstFrame{};
        bool mMovingKey{};
    };
}// namespace mikoto::imgui

#endif// MIKOTO_IMGUI_SEQUENCER_HH
