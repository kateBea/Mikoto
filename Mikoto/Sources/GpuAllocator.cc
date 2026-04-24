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
//

#include <EASTL/unique_ptr.h>

#include <Memory/GpuAllocator.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <Memory/GpuAllocator.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::memory {

    using namespace mikoto::renderer;
    using namespace mikoto::renderer::vulkan;

    auto IGpuAllocator::Create(GpuDevice* device) -> eastl::unique_ptr<IGpuAllocator> {
        switch ( device->GetGraphicsApi() ) {
            case GraphicsAPI::eVulkan:
                return eastl::make_unique<GpuMemoryAllocator>(device);
            default:;
        }

        return nullptr;
    }
}// namespace Mikoto