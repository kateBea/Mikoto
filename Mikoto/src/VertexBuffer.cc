/**
* VertexBuffer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <vector>
#include <memory>

// Project Headers
#include <Core/Logging/Logger.hh>
#include <Core/System/RenderSystem.hh>
#include <Renderer/Buffer/VertexBuffer.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>

namespace Mikoto {
    auto VertexBuffer::Create(const std::vector<float>& data, const BufferLayout& layout) -> Scope_T<VertexBuffer> {
        auto& renderSystem{ Engine::GetSystem<RenderSystem>() };

        VertexBufferCreateInfo createInfo{
            .Data{ data },
            .Layout{ layout },
            .RetainData{ false },
        };

        switch(renderSystem.GetDefaultApi()) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanVertexBuffer>( createInfo );
            default:
                MKT_CORE_LOGGER_CRITICAL("VertexBuffer::Create - Unsupported renderer API");
            return nullptr;
        }
    }
}