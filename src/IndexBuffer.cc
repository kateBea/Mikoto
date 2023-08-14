//
// Created by kate on 6/5/23.
//

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>
#include <Renderer/OpenGL/OpenGLIndexBuffer.hh>

namespace Mikoto {
    auto IndexBuffer::CreateBuffer(const std::vector<UInt32_T>& data) -> std::shared_ptr<IndexBuffer> {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case Renderer::GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLIndexBuffer>(data);
            case Renderer::GraphicsAPI::VULKAN_API:
                return std::make_shared<VulkanIndexBuffer>(data);
            default:
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}


