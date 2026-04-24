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

#include <Memory/PoolAllocator.hh>

namespace mikoto::memory {
    PoolAllocator::PoolAllocator( size_t elementSize, size_t elementCount, size_t alignment )
        : mElementSize{ AlignUp( elementSize, alignment ) },
          mElementCount{ elementCount },
          mAlignment{ alignment } {
        // Build free list (offset-based)
        mFreeList = nullptr;

        for ( size_t i{}; i < mElementCount; ++i ) {
            size_t offset{ i * mElementSize };

            // store next pointer inside block (offset-as-pointer trick avoided here)
            auto* node{ new size_t( offset ) };
            *node = reinterpret_cast<size_t>( mFreeList );
            mFreeList = node;
        }
    }

    auto PoolAllocator::Allocate( size_t size, size_t alignment ) -> eastl::optional<Allocation> {
        if ( size > mElementSize || alignment > mAlignment || mFreeList == nullptr )
            return eastl::nullopt;

        auto* node{ reinterpret_cast<size_t*>( mFreeList ) };
        size_t offset{ *node };
        mFreeList = reinterpret_cast<void*>( offset );

        delete node;

        return Allocation{
            .mOffset = offset,
            .mSize = mElementSize
        };
    }

    auto PoolAllocator::Free( const Allocation& alloc ) -> void {
        auto* node = new size_t( alloc.mOffset );
        *node = reinterpret_cast<size_t>( mFreeList );
        mFreeList = node;
    }

    auto PoolAllocator::Reset() -> void {
        // rebuild list (simple version)
        mFreeList = nullptr;

        for ( size_t i = 0; i < mElementCount; ++i ) {
            size_t offset = i * mElementSize;

            auto* node = new size_t( offset );
            *node = reinterpret_cast<size_t>( mFreeList );
            mFreeList = node;
        }
    }
}// namespace mikoto::memory