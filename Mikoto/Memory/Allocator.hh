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

#ifndef MIKOTO_MEMORY_ALLOCATOR_HH
#define MIKOTO_MEMORY_ALLOCATOR_HH

#include <new>

#include <EASTL/vector.h>
#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#define MKT_BYTES( X ) ( X )

#define MKT_KIBIBYTES( X ) (( ( X ) * 1024 ))
#define MKT_KILOBYTES( X ) (( ( X ) * 1000 ))

#define MKT_MIBIBYTES( X ) (( ( X ) * 1024 * 1024 ))
#define MKT_MEGABYTES( X ) (( ( X ) * 1000 * 1000 ))

#define MKT_SIZEOF( X ) sizeof( X )
#define MKT_ADDRESSOF( OBJECT_REF ) std::addressof( OBJECT_REF )

#define MKT_VECTOR_SIZE_BYTES( vec ) vec.size() * sizeof(decltype(vec)::value_type)

#define MKT_NO_THROW_NEW new (std::nothrow)
#define MKT_NOTHROW_PLACEMENT_NEW( SIZE ) ::operator new( 1 * SIZE, std::nothrow )
#define MKT_NOTHROW_PLACEMENT_NEW_COUNT( SIZE, COUNT ) ::operator new( COUNT * SIZE, std::nothrow )
#define MKT_NOTHROW_PLACEMENT_DELETE( PTR ) ::operator delete(PTR)

template <class T>
    struct ankerl::unordered_dense::hash<eastl::unique_ptr<T>> {
    using is_avalanching = void;

    auto operator()(eastl::unique_ptr<T> const& ptr) const noexcept -> std::uint64_t {
        return hash<T*>{}( ptr.get() );
    }
};

namespace mikoto::memory {

    // Alignment must be >= 1 (power of two). 0 is invalid.
    MKT_NODISCARD auto AlignUp(core::usize value, core::usize alignment) -> core::usize;

    struct Allocation {
        core::usize mOffset{};
        core::usize mSize{};
    };

    class IAllocator {
    public:
        virtual ~IAllocator() = default;

        virtual auto Free(const Allocation& allocation) -> void = 0;
        MKT_NODISCARD virtual auto Allocate(core::usize size, core::usize alignment) -> eastl::optional<Allocation> = 0;

        virtual auto Reset() -> void; // optional override
    };
}

#endif// MIKOTO_MEMORY_ALLOCATOR_HH
