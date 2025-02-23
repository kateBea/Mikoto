//
// Created by zanet on 2/16/2025.
//

// Project Headers
#include <Core/Logging/Logger.hh>
#include <Core/System/RenderSystem.hh>
#include <Renderer/Buffer/IndexBuffer.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>

namespace Mikoto {

    auto IndexBuffer::Create( const std::vector<UInt32_T>& data ) -> Scope_T<IndexBuffer> {
        auto& renderSystem{ Engine::GetSystem<RenderSystem>() };

        VulkanIndexBufferCreateInfo createInfo{
            .Indices{ data },
        };

        switch ( renderSystem.GetDefaultApi() ) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanIndexBuffer>( createInfo );
            default:
                MKT_CORE_LOGGER_CRITICAL( "VertexBuffer::Create - Unsupported renderer API" );
            return nullptr;
        }
    }
}// namespace Mikoto