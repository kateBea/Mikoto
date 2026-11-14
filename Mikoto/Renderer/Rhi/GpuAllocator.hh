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

#ifndef MIKOTO_RENDERER_RHI_GPU_ALLOCATOR_HH
#define MIKOTO_RENDERER_RHI_GPU_ALLOCATOR_HH

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer::rhi {
    /**
     * Backend-independent interface for GPU-memory accounting and allocation.
     */
    class IGpuAllocator {
    public:
        /**
         * Initializes backend allocator resources.
         */
        virtual auto Init() -> void = 0;


        /**
         * Releases backend allocator resources.
         */
        virtual auto Shutdown() -> void = 0;


        /** Advances per-frame allocator accounting, including cached OS budgets. */
        virtual auto BeginFrame() -> void = 0;


        /**
         * Returns allocated GPU memory in bytes.
         *
         * @returns Allocated GPU memory in bytes.
         */
        MKT_NODISCARD virtual auto GetMemoryUsage() const -> core::usize = 0;


        /** Returns process-local and shared usage estimates, or unavailable when unsupported. */
        MKT_NODISCARD virtual auto GetProcessMemoryUsage() const -> ProcessGpuMemory = 0;


        /**
         * Returns total GPU memory in bytes.
         *
         * @returns Total GPU memory in bytes.
         */
        MKT_NODISCARD virtual auto GetMemoryTotal() const -> core::usize = 0;


        /**
         * Returns available GPU memory in bytes.
         *
         * @returns Available GPU memory in bytes.
         */
        MKT_NODISCARD virtual auto GetMemoryAvailable() const -> core::usize = 0;


        /**
         * Destroys the allocator.
         */
        virtual ~IGpuAllocator() = default;


        /**
         * Creates the allocator appropriate for @p device's graphics API.
         *
         * @param device Device that owns the allocator.
         * @returns The backend-specific allocator.
         */
        static auto Create( IGpuDevice* device ) -> eastl::unique_ptr<IGpuAllocator>;
    };
}

#endif// MIKOTO_RENDERER_RHI_GPU_ALLOCATOR_HH

