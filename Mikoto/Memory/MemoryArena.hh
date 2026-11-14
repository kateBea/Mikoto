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

#ifndef MIKOTO_MEMORY_MEMORY_ARENA_HH
#define MIKOTO_MEMORY_MEMORY_ARENA_HH

#include <EASTL/utility.h>
#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/ReferenceCounted.hh>

#include <Memory/Allocator.hh>

namespace mikoto::memory {

    // Memory arena:
    // Owns a memory buffer and suballocates it using a specific allocation strategy
    // (e.g. linear, stack, free-list). Provides a unified interface for clients to
    // request and release memory without managing the underlying storage.
    template<typename BufferType, typename AllocatorType>
    class MemoryArena final {
    public:
        explicit MemoryArena( core::Ref<BufferType> buffer, auto&&... args )
            : mBuffer{ buffer }, mAllocator{
                eastl::make_unique<AllocatorType>( eastl::forward<decltype( args )>( args )... ) }
        {}

        auto Allocate( core::usize size, core::usize alignment ) -> eastl::optional<Allocation> {
            MKT_BEGIN_PROFILER();
            return mAllocator->Allocate( size, alignment );
        }

        auto Free( const Allocation& alloc ) -> void {
            MKT_BEGIN_PROFILER();
            mAllocator->Free( alloc );
        }

        auto Reset() -> void {
            MKT_BEGIN_PROFILER();
            mAllocator->Reset();
        }

        MKT_NODISCARD auto GetBuffer() const -> core::Ref<BufferType> {
            MKT_BEGIN_PROFILER();
            return mBuffer;
        }

    private:
        core::Ref<BufferType> mBuffer{};
        eastl::unique_ptr<IAllocator> mAllocator{};
    };

}// namespace mikoto

#endif// MIKOTO_MEMORY_MEMORY_ARENA_HH
