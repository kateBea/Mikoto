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

#include <Core/Platform.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/GpuAllocator.hh>
#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>
#endif

namespace mikoto::renderer::rhi {

    auto IGpuAllocator::Create(IGpuDevice* device) -> eastl::unique_ptr<IGpuAllocator> {
        switch ( device->GetGraphicsApi() ) {
            case GraphicsAPI::eVulkan:
                return eastl::make_unique<vulkan::GpuMemoryAllocator>(device);
#if defined( MIKOTO_PLATFORM_WINDOWS )
            case GraphicsAPI::eD3D12:
                return eastl::make_unique<d3d12::GpuMemoryAllocator>(device);
#endif
            default:;
        }

        return nullptr;
    }
}// namespace Mikoto