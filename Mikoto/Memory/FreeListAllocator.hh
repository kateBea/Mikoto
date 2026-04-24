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

#ifndef MIKOTOROOT_FREE_LIST_ALLOCATOR_HH
#define MIKOTOROOT_FREE_LIST_ALLOCATOR_HH

#include <EASTL/optional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

namespace mikoto::memory {

    // Free-list allocator (base):
    // Manages a list of free memory ranges and supports variable-size allocations.
    // Free blocks are tracked externally and merged (coalesced) on free to reduce fragmentation.
    // Allocation: depends on strategy (see derived classes)
    // Free: O(N log N) (due to sort + coalescing)
    // Reset: O(1)
    // General-purpose allocator for reusable memory with varying allocation sizes.
    class FreeListAllocator : public IAllocator {
    public:
        explicit FreeListAllocator(size_t sizeBytes);
        ~FreeListAllocator() override = default;

        auto Free(const Allocation& allocation) -> void override;
        auto Reset() -> void override;

    protected:
        struct FreeRange {
            size_t Offset{};
            size_t Size{};
        };

        MKT_NODISCARD auto AllocateFromRange( size_t index, size_t size, size_t alignment) -> Allocation;

    protected:
        size_t mSize{};
        eastl::vector<FreeRange> mFreeRanges{};
    };

    // Free-list allocator (first-fit):
    // Scans the free list and selects the first block large enough to satisfy the request.
    // Faster allocation but more prone to fragmentation over time.
    // Allocation: O(N)
    // Free: O(N log N) (coalescing)
    // Good trade-off between performance and simplicity.
    class FreeListFirstFitAllocator final : public FreeListAllocator {
    public:
        explicit FreeListFirstFitAllocator(size_t sizeBytes);

        auto Allocate(size_t size, size_t alignment) -> eastl::optional<Allocation> override;
    };

    // Free-list allocator (best-fit):
    // Scans the free list and selects the smallest block that fits the request,
    // minimizing wasted space but increasing search cost.
    // Allocation: O(N)
    // Free: O(N log N) (coalescing)
    // Reduces fragmentation compared to first-fit but can create many small unusable gaps.
    class FreeListBestFitAllocator final : public FreeListAllocator {
    public:
        explicit FreeListBestFitAllocator(size_t sizeBytes);

        auto Allocate(size_t size, size_t alignment) -> eastl::optional<Allocation> override;
    };
}

#endif//MIKOTOROOT_FREE_LIST_ALLOCATOR_HH
