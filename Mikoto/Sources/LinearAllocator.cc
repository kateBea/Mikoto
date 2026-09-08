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

#include <Memory/LinearAllocator.hh>

namespace mikoto::memory {
    LinearAllocator::LinearAllocator( core::usize sizeBytes )
        : mSize{ sizeBytes }, mOffset{ 0 } {}

    auto LinearAllocator::Allocate( core::usize size, core::usize alignment ) -> eastl::optional<Allocation> {
        const core::usize alignedOffset{ AlignUp( mOffset, alignment ) };
        const core::usize newOffset{ alignedOffset + size };

        if ( newOffset > mSize )
            return eastl::nullopt;

        Allocation alloc{
            .mOffset = alignedOffset,
            .mSize = size
        };

        mOffset = newOffset;
        return alloc;
    }

    auto LinearAllocator::Free( const Allocation & ) -> void {
        // no-op
    }

    auto LinearAllocator::Reset() -> void {
        mOffset = 0;
    }
}// namespace mikoto::memory
