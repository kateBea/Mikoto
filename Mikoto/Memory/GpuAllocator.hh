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

#ifndef MIKOTO_GPU_ALLOCATOR_H
#define MIKOTO_GPU_ALLOCATOR_H

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::memory {

    class IGpuAllocator {
    public:
        explicit IGpuAllocator(renderer::rhi::IGpuDevice* device)
            : mDevice{ device }
        {}

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        MKT_NODISCARD virtual auto GetMemoryUsage() const -> size_t = 0;
        MKT_NODISCARD virtual auto GetMemoryTotal() const -> size_t = 0;
        MKT_NODISCARD virtual auto GetMemoryAvailable() const -> size_t = 0;

        virtual ~IGpuAllocator() = default;

        static auto Create(renderer::rhi::IGpuDevice* device) -> eastl::unique_ptr<IGpuAllocator>;

    protected:
        renderer::rhi::IGpuDevice* mDevice{};
    };
}



#endif //MIKOTO_GPU_ALLOCATOR_H
