/**
 * RendererAPI.cc
 * Created by kate on 6/9/23.
 * */

// C++ Standard Library
#include <new>

// Project Headers
#include <Core/Logger.hh>
#include <Renderer/RendererAPI.hh>
#include <Renderer/OpenGL/OpenGLRenderer.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {

    auto RendererAPI::Create(GraphicsAPI backend) -> RendererAPI* {
        switch(backend) {
            case GraphicsAPI::OPENGL_API: return new (std::nothrow) OpenGLRenderer();
            case GraphicsAPI::VULKAN_API: return new (std::nothrow) VulkanRenderer();
            default:
                MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API!");
                break;
        }

        return nullptr;
    }
}