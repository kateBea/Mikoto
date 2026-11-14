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

#ifndef MIKOTO_DIAGNOSTICS_FRAME_HISTORY_HH
#define MIKOTO_DIAGNOSTICS_FRAME_HISTORY_HH

#include <EASTL/array.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::editor {
    /// Statistics over the retained samples; timings are in milliseconds.
    struct FrameSummary {
        core::f64 mFps{};
        core::f64 mAverageMs{};
        core::f64 mMedianMs{};
        core::f64 mP95Ms{};
        core::f64 mWorstMs{};
        core::usize mCount{};
    };

    /**
     * @brief Keeps a bounded rolling history of valid frame durations.
     * @details New samples replace the oldest once the buffer is full. Plotting
     * reads samples chronologically; summaries sort a local copy so calculating
     * percentiles cannot disturb the plotted history. Storage never allocates.
     */
    class FrameHistory {
    public:
        static constexpr core::usize kCapacity{ 240 };

        auto Clear() -> void;

        /// Append a duration in seconds. Reject nonpositive millisecond samples.
        MKT_NODISCARD auto Push( core::f32 seconds ) -> bool;

        MKT_NODISCARD auto Count() const -> core::usize;

        /// Read oldest-to-newest by index; return zero outside the retained range.
        MKT_NODISCARD auto At( core::usize index ) const -> core::f32;

        /// Return zeroed statistics when empty; FPS is 1000 / mean frame time.
        MKT_NODISCARD auto Summarize() const -> FrameSummary;

    private:
        core::usize mNext{};
        core::usize mCount{};

        eastl::array<core::f32, kCapacity> mValues{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_DIAGNOSTICS_FRAME_HISTORY_HH

