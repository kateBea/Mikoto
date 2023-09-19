/**
 * VertexBuffer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <vector>
#include <memory>

// Project Headers
#include <Core/Logger.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>
#include <Renderer/OpenGL/OpenGLVertexBuffer.hh>

namespace Mikoto {
    auto VertexBuffer::Create(const std::vector<float>& data, const BufferLayout& layout) -> std::shared_ptr<VertexBuffer> {
        VertexBufferCreateInfo createInfo{};

        createInfo.Data = data;
        createInfo.RetainData = false; // for future use
        createInfo.Layout = layout;

        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLVertexBuffer>(std::move(createInfo));
            case GraphicsAPI::VULKAN_API:
                return std::make_shared<VulkanVertexBuffer>(std::move(createInfo));
            default:
                MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}