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

#ifndef MIKOTOROOT_LINEAR_ALLOCATOR_HH
#define MIKOTOROOT_LINEAR_ALLOCATOR_HH

#include <EASTL/optional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

namespace mikoto::memory {

    // Linear allocator (a.k.a. bump allocator):
    // Allocates memory by advancing a single offset forward.
    // No per-allocation free; memory is released all at once via Reset().
    // Extremely fast and avoids fragmentation, ideal for transient data (e.g. per-frame uploads).

    // Linear allocator (bump allocator):
    // Allocates memory by linearly advancing an offset. Does not support individual frees;
    // memory is released all at once via Reset().
    // Allocation: O(1)
    // Free: O(1) (no-op)
    // Reset: O(1)
    // Best for transient allocations (e.g. per-frame data, upload buffers). No fragmentation.
    class LinearAllocator final : public IAllocator {
    public:
        explicit LinearAllocator(size_t sizeBytes);

        MKT_NODISCARD auto Allocate(size_t size, size_t alignment) -> eastl::optional<Allocation> override;
        auto Free(const Allocation&) -> void override; // no-op

        auto Reset() -> void override;

        MKT_NODISCARD auto GetSize() const -> size_t { return mSize; }
        MKT_NODISCARD auto GetOffset() const -> size_t { return mOffset; }

    private:
        size_t mSize{};
        size_t mOffset{};
    };
}

#endif//MIKOTOROOT_LINEAR_ALLOCATOR_HH
