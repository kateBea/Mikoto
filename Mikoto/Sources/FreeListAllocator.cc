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

#include <EASTL/algorithm.h>
#include <EASTL/optional.h>
#include <EASTL/sort.h>

#include <Memory/FreeListAllocator.hh>

namespace mikoto::memory {
    FreeListAllocator::FreeListAllocator( size_t sizeBytes )
        : mSize{ sizeBytes } {
        mFreeRanges.push_back( { 0, sizeBytes } );
    }

    auto FreeListAllocator::Reset() -> void {
        mFreeRanges.clear();
        mFreeRanges.push_back( { 0, mSize } );
    }

    auto FreeListAllocator::Free( const Allocation& alloc ) -> void {
        mFreeRanges.push_back( { alloc.mOffset, alloc.mSize } );

        // sort by offset
        eastl::sort( mFreeRanges.begin(), mFreeRanges.end(),
                     []( const FreeRange& a, const FreeRange& b ) {
                         return a.Offset < b.Offset;
                     } );

        // coalesce
        for ( size_t i = 0; i + 1 < mFreeRanges.size(); ) {
            auto& curr = mFreeRanges[i];
            auto& next = mFreeRanges[i + 1];

            if ( curr.Offset + curr.Size == next.Offset ) {
                curr.Size += next.Size;
                mFreeRanges.erase( mFreeRanges.begin() + i + 1 );
            } else {
                ++i;
            }
        }
    }

    auto FreeListAllocator::AllocateFromRange(
            size_t index,
            size_t size,
            size_t alignment ) -> Allocation {
        auto& range = mFreeRanges[index];

        size_t alignedOffset{ AlignUp( range.Offset, alignment ) };
        size_t padding{ alignedOffset - range.Offset };
        size_t totalSize{ padding + size };

        Allocation alloc{
            .mOffset = alignedOffset,
            .mSize = size
        };

        if ( totalSize == range.Size ) {
            // perfect fit
            mFreeRanges.erase( mFreeRanges.begin() + index );
        } else {
            // shrink range
            range.Offset += totalSize;
            range.Size -= totalSize;
        }

        return alloc;
    }

    FreeListFirstFitAllocator::FreeListFirstFitAllocator( size_t sizeBytes )
        : FreeListAllocator( sizeBytes ) {}

    auto FreeListFirstFitAllocator::Allocate( size_t size, size_t alignment ) -> eastl::optional<Allocation> {
        for ( size_t i{}; i < mFreeRanges.size(); ++i ) {
            const auto& range{ mFreeRanges[i] };

            size_t alignedOffset{ AlignUp( range.Offset, alignment ) };
            size_t padding{ alignedOffset - range.Offset };

            if ( range.Size >= padding + size ) {
                return AllocateFromRange( i, size, alignment );
            }
        }

        return eastl::nullopt;
    }

    FreeListBestFitAllocator::FreeListBestFitAllocator(size_t sizeBytes)
    : FreeListAllocator(sizeBytes) {}

    auto FreeListBestFitAllocator::Allocate(size_t size, size_t alignment) -> eastl::optional<Allocation> {
        size_t bestIndex = SIZE_MAX;
        size_t bestWaste = SIZE_MAX;

        for (size_t i{}; i < mFreeRanges.size(); ++i) {
            const auto& range = mFreeRanges[i];

            size_t alignedOffset{ AlignUp(range.Offset, alignment) };
            size_t padding{ alignedOffset - range.Offset };

            if (range.Size < padding + size)
                continue;

            size_t waste{ range.Size - (padding + size) };

            if (waste < bestWaste) {
                bestWaste = waste;
                bestIndex = i;
            }
        }

        if (bestIndex == SIZE_MAX)
            return eastl::nullopt;

        return AllocateFromRange(bestIndex, size, alignment);
    }
}// namespace mikoto::memory
