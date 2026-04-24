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

#ifndef MIKOTOROOT_POOL_ALLOCATOR_HH
#define MIKOTOROOT_POOL_ALLOCATOR_HH

#include <EASTL/optional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

namespace mikoto::memory {

    // Pool allocator:
    // Manages memory as a set of fixed-size blocks. Allocations return one block at a time.
    // Uses an intrusive free list where free blocks store the next pointer inside their own memory.
    // Allocation: O(1)
    // Free: O(1)
    // Reset: O(N)
    // No fragmentation, but only supports fixed-size allocations.
    // Ideal for frequently created/destroyed objects of uniform size.
    class PoolAllocator final : public IAllocator {
    public:
        PoolAllocator(size_t elementSize, size_t elementCount, size_t alignment);

        MKT_NODISCARD auto Allocate(size_t size, size_t alignment) -> eastl::optional<Allocation> override;
        auto Free(const Allocation& allocation) -> void override;

        auto Reset() -> void override;

        MKT_NODISCARD auto GetElementSize() const -> size_t { return mElementSize; }
        MKT_NODISCARD auto GetCapacity() const -> size_t { return mElementCount; }

    private:
        size_t mElementSize{};
        size_t mElementCount{};
        size_t mAlignment{};

        void* mFreeList{}; // intrusive free list
    };
}


#endif//MIKOTOROOT_POOL_ALLOCATOR_HH
