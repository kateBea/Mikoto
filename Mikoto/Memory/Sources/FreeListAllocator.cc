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


#include <EASTL/sort.h>
#include <EASTL/numeric.h>
#include <EASTL/optional.h>
#include <EASTL/algorithm.h>
#include <EASTL/numeric_limits.h>

#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>
#include <Memory/FreeListAllocator.hh>


namespace mikoto::memory {

    FreeListAllocator::FreeListAllocator( core::usize sizeBytes )
        : mSize{ sizeBytes } {
        MKT_BEGIN_PROFILER();
        mFreeRanges.push_back( { 0, sizeBytes } );
    }

    auto FreeListAllocator::Reset() -> void {
        MKT_BEGIN_PROFILER();
        mFreeRanges.clear();
        mFreeRanges.push_back( { 0, mSize } );
    }

    auto FreeListAllocator::Free( const Allocation& alloc ) -> void {
        MKT_BEGIN_PROFILER();
        mFreeRanges.push_back( { alloc.mOffset, alloc.mSize } );

        // Keep neighbours adjacent so reclaimed allocations can form larger free ranges.
        eastl::sort( mFreeRanges.begin(), mFreeRanges.end(),
            []( const FreeRange& a, const FreeRange& b ) {
            return a.Offset < b.Offset;
        } );

        // Revisit a merged range before advancing; it may also touch the next allocation.
        for ( core::usize i = 0; i + 1 < mFreeRanges.size(); ) {
            auto& curr = mFreeRanges[i];
            auto& next = mFreeRanges[i + 1];

            if ( curr.Offset + curr.Size == next.Offset ) {
                curr.Size += next.Size;
                mFreeRanges.erase( mFreeRanges.begin() + i + 1 );
            }
            else {
                ++i;
            }
        }
    }

    auto FreeListAllocator::AllocateFromRange(
        core::usize index,
        core::usize size,
        core::usize alignment ) -> Allocation {
        MKT_BEGIN_PROFILER();
        auto& range = mFreeRanges[index];

        core::usize alignedOffset{ AlignUp( range.Offset, alignment ) };
        core::usize padding{ alignedOffset - range.Offset };
        core::usize totalSize{ padding + size };

        Allocation alloc{
            .mOffset = alignedOffset,
            .mSize = size
        };

        if ( totalSize == range.Size ) {
            // Remove exhausted ranges so later searches only visit usable space.
            mFreeRanges.erase( mFreeRanges.begin() + index );
        }
        else {
            // Consume alignment padding together with the requested allocation.
            range.Offset += totalSize;
            range.Size -= totalSize;
        }

        return alloc;
    }

    FreeListFirstFitAllocator::FreeListFirstFitAllocator( core::usize sizeBytes )
        : FreeListAllocator( sizeBytes ) {
    }

    auto FreeListFirstFitAllocator::Allocate( core::usize size, core::usize alignment ) -> eastl::optional<Allocation> {
        MKT_BEGIN_PROFILER();

        for ( core::usize i{}; i < mFreeRanges.size(); ++i ) {
            const auto& range{ mFreeRanges[i] };

            core::usize alignedOffset{ AlignUp( range.Offset, alignment ) };
            core::usize padding{ alignedOffset - range.Offset };

            if ( range.Size >= padding + size ) {
                return AllocateFromRange( i, size, alignment );
            }
        }

        return eastl::nullopt;
    }

    FreeListBestFitAllocator::FreeListBestFitAllocator( core::usize sizeBytes )
        : FreeListAllocator( sizeBytes ) {
    }

    auto FreeListBestFitAllocator::Allocate( core::usize size, core::usize alignment ) -> eastl::optional<Allocation> {
        MKT_BEGIN_PROFILER();
        core::usize bestIndex = SIZE_MAX;
        core::usize bestWaste = SIZE_MAX;

        for ( core::usize i{}; i < mFreeRanges.size(); ++i ) {
            const auto& range = mFreeRanges[i];

            core::usize alignedOffset{ AlignUp( range.Offset, alignment ) };
            core::usize padding{ alignedOffset - range.Offset };

            if ( range.Size < padding + size ) {
                continue;
            }

            core::usize waste{ range.Size - ( padding + size ) };

            if ( waste < bestWaste ) {
                bestWaste = waste;
                bestIndex = i;
            }
        }

        if ( bestIndex == eastl::numeric_limits<core::usize>::max() ) {
            return eastl::nullopt;
        }

        return AllocateFromRange( bestIndex, size, alignment );
    }
}// namespace mikoto::memory
