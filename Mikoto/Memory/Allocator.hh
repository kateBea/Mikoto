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

#ifndef MIKOTO_ALLOCATOR_HH
#define MIKOTO_ALLOCATOR_HH

#include <new>
#include <cstdint>

#include <EASTL/vector.h>
#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

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

namespace ankerl::unordered_dense {

    // Taken as reference: unordered_dense line 314
    template <class T>
    struct hash<eastl::unique_ptr<T>> {
        using is_avalanching = void;

        auto operator()(eastl::unique_ptr<T> const& ptr) const noexcept -> std::uint64_t {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return detail::wyhash::hash(reinterpret_cast<std::uintptr_t>(ptr.get()));
        }
    };
}

namespace mikoto::memory {

    using namespace mikoto::core;

    auto MallocFree( void* p ) -> void;

    MKT_NODISCARD auto MallocAlloc( size_t size ) -> void*;
    MKT_NODISCARD auto MallocCalloc( size_t size ) -> void*;
    MKT_NODISCARD auto MallocRealloc( void* p, size_t size ) -> void*;

    // Alignment must be >= 1 (power of two). 0 is invalid.
    MKT_NODISCARD auto AlignUp(size_t value, size_t alignment) -> size_t;

    struct Allocation {
        size_t mOffset{};
        size_t mSize{};
    };

    class IAllocator {
    public:
        virtual ~IAllocator() = default;

        MKT_NODISCARD virtual auto Allocate(size_t size, size_t alignment) -> eastl::optional<Allocation> = 0;
        virtual auto Free(const Allocation& allocation) -> void = 0;

        virtual auto Reset() -> void {} // optional override
    };
}

#endif//MIKOTO_ALLOCATOR_HH
