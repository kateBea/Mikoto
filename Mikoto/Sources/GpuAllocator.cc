//
// Created by zanet on 10/6/2025.
//

#include <Memory/GpuAllocator.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace Mikoto {


    auto GpuAllocator::Create(GpuDevice* device) -> Unique<GpuAllocator> {
        switch ( RenderService::Get()->GetActiveGraphicsApi() ) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanMemoryAllocator>(device);
            default:;
        }

        return nullptr;
    }
}// namespace Mikoto