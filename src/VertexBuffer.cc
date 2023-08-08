/**
 * VertexBuffer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library

// Project Headers
#include <Core/Logger.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>
#include <Renderer/OpenGL/OpenGLVertexBuffer.hh>

namespace kaTe {
    auto VertexBuffer::CreateBuffer(const std::vector<float>& data) -> std::shared_ptr<VertexBuffer> {
        VertexBufferCreateInfo createInfo{};

        createInfo.Data = data;
        createInfo.RetainData = false;
        createInfo.Layout = VertexBuffer::GetDefaultBufferLayout();

        switch(Renderer::GetActiveGraphicsAPI()) {
            case Renderer::GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLVertexBuffer>(createInfo);
            case Renderer::GraphicsAPI::VULKAN_API:
                return std::make_shared<VulkanVertexBuffer>(createInfo);
            default:
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}