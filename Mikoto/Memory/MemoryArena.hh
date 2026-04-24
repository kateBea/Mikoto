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

#ifndef MIKOTOROOT_MEMORY_ARENA_HH
#define MIKOTOROOT_MEMORY_ARENA_HH

#include <EASTL/optional.h>
#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

#include <Memory/Allocator.hh>

namespace mikoto::memory {

    // Memory arena:
    // Owns a memory buffer and suballocates it using a specific allocation strategy
    // (e.g. linear, stack, free-list). Provides a unified interface for clients to
    // request and release memory without managing the underlying storage.
    template<typename BufferT, typename AllocatorT>
    class MemoryArena final {
    public:
        explicit MemoryArena( Ref<BufferT> buffer, auto&&... args )
            : mBuffer{ buffer }, mAllocator{ eastl::make_unique<AllocatorT>( eastl::forward<decltype( args )>( args )... ) } {}

        auto Allocate( size_t size, size_t alignment )
                -> eastl::optional<Allocation> {
            return mAllocator->Allocate( size, alignment );
        }

        auto Free( const Allocation& alloc ) -> void {
            mAllocator->Free( alloc );
        }

        auto Reset() -> void {
            mAllocator->Reset();
        }

        MKT_NODISCARD auto GetBuffer() const -> Ref<BufferT> {
            return mBuffer;
        }

    private:
        Ref<BufferT> mBuffer{};
        eastl::unique_ptr<IAllocator> mAllocator{};
    };

}// namespace mikoto

#endif//MIKOTOROOT_MEMORY_ARENA_HH
