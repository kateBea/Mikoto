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

#include <EASTL/sort.h>
#include <EASTL/numeric.h>
#include <EASTL/algorithm.h>

#include <glm/gtx/compatibility.hpp>

#include <Core/Profiler.hh>

#include <Diagnostics/FrameHistory.hh>

namespace mikoto::editor {
    using namespace core;

    auto FrameHistory::Push( core::f32 seconds ) -> bool {
        MKT_BEGIN_PROFILER();
        const core::f32 milliseconds{ seconds * 1000.0f };

        if ( milliseconds <= 0.0f ) {
            return false;
        }

        mValues[mNext] = milliseconds;
        mNext = ( mNext + 1 ) % kCapacity;
        mCount = eastl::min( mCount + 1, kCapacity );
        return true;
    }

    auto FrameHistory::Clear() -> void {
        MKT_BEGIN_PROFILER();
        // Count bounds every read; new samples overwrite the retained storage
        // before it becomes visible again, so resetting need not zero the array.
        mNext = 0;
        mCount = 0;
    }

    auto FrameHistory::Count() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mCount;
    }

    auto FrameHistory::At( core::usize index ) const -> core::f32 {
        MKT_BEGIN_PROFILER();

        if ( index >= mCount ) {
            return 0.0f;
        }

        const core::usize oldest{ ( mNext + kCapacity - mCount ) % kCapacity };
        return mValues[( oldest + index ) % kCapacity];
    }

    auto FrameHistory::Summarize() const -> FrameSummary {
        MKT_BEGIN_PROFILER();
        FrameSummary result{};
        result.mCount = mCount;

        if ( mCount == 0 ) {
            return result;
        }

        // Before wrapping, valid slots form a prefix; after wrapping all slots
        // are valid. Sorting needs no chronological traversal or per-slot modulo.
        eastl::array<core::f32, kCapacity> sorted;
        eastl::copy_n( mValues.begin(), mCount, sorted.begin() );
        const auto end{ sorted.begin() + mCount };
        eastl::sort( sorted.begin(), end );

        result.mAverageMs = eastl::accumulate( sorted.begin(), end, 0.0 ) / mCount;
        result.mFps = 1000.0 / result.mAverageMs;
        const core::usize middle{ mCount / 2 };
        result.mMedianMs = mCount % 2 != 0
                               ? sorted[middle]
                               : ( static_cast<core::f64>( sorted[middle - 1] ) + sorted[middle] ) * 0.5;

        // Nearest-rank percentile: ceil(95% of the sample count), converted to
        // a zero-based index. Integer arithmetic keeps the rank exact.
        const core::usize percentileIndex{ ( 95 * mCount + 99 ) / 100 - 1 };
        result.mP95Ms = sorted[percentileIndex];
        result.mWorstMs = sorted[mCount - 1];
        return result;
    }
}// namespace mikoto::editor
