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

#ifndef MIKOTOROOT_STACK_ALLOCATOR_HH
#define MIKOTOROOT_STACK_ALLOCATOR_HH

#include <EASTL/optional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

namespace mikoto::memory {

    // Stack allocator (LIFO):
    // Allocates memory linearly like a linear allocator, but supports freeing only in
    // last-in-first-out order. Free rewinds the offset if the last allocation is released.
    // Allocation: O(1)
    // Free: O(1) (only if freeing most recent allocation)
    // Reset: O(1)
    // Useful for scoped allocations and temporary data with strict lifetimes.
    class StackAllocator final : public IAllocator {
    public:
        explicit StackAllocator(size_t sizeBytes);

        MKT_NODISCARD auto Allocate(size_t size, size_t alignment) -> eastl::optional<Allocation> override;
        auto Free(const Allocation& allocation) -> void override;

        auto Reset() -> void override;

    private:
        struct Marker {
            size_t Offset{};
        };

        size_t mSize{};
        size_t mOffset{};
    };

}// namespace mikoto

#endif//MIKOTOROOT_STACK_ALLOCATOR_HH
