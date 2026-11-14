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


#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>
#include <Memory/StackAllocator.hh>


namespace mikoto::memory {

    StackAllocator::StackAllocator( core::usize sizeBytes )
        : mSize{ sizeBytes }, mOffset{ 0 } {
    }

    auto StackAllocator::Allocate( core::usize size, core::usize alignment ) -> eastl::optional<Allocation> {
        MKT_BEGIN_PROFILER();
        const core::usize alignedOffset{ AlignUp( mOffset, alignment ) };
        const core::usize newOffset{ alignedOffset + size };

        if ( newOffset > mSize ) {
            return eastl::nullopt;
        }

        Allocation alloc{
            .mOffset = alignedOffset,
            .mSize = size
        };

        mOffset = newOffset;
        return alloc;
    }

    auto StackAllocator::Free( const Allocation& alloc ) -> void {
        MKT_BEGIN_PROFILER();
        // Only valid if freeing last allocation (LIFO)
        if ( alloc.mOffset + alloc.mSize == mOffset ) {
            mOffset = alloc.mOffset;
        }
        // else: ignore or assert in debug
    }

    auto StackAllocator::Reset() -> void {
        MKT_BEGIN_PROFILER();
        mOffset = 0;
    }
}// namespace mikoto::memory