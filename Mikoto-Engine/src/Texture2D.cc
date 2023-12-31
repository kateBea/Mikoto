/**
 * Texture2D.cc
 * Created by kate on 6/8/23.
 * */

// C++ Standard Library
#include <memory>
#include <stdexcept>

// Project Headers
#include <Common/Types.hh>
#include <Common/RenderingUtils.hh>

#include <Core/Logger.hh>

#include <Renderer/Renderer.hh>
#include <Renderer/Material/Texture2D.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>
#include <Renderer/OpenGL/OpenGLTexture2D.hh>


namespace Mikoto {
    auto Texture2D::Create(const Path_T& path, MapType type) -> std::shared_ptr<Texture2D> {
        try {
            switch(Renderer::GetActiveGraphicsAPI()) {
                case GraphicsAPI::OPENGL_API:
                    return std::make_shared<OpenGLTexture2D>(path, type);
                case GraphicsAPI::VULKAN_API:
                    return std::make_shared<VulkanTexture2D>(path, type);
                default:
                    MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                    break;
            }
        }
        catch ( const std::exception& except) {
            MKT_CORE_LOGGER_ERROR("Failed to create Texture2D. Exception: {}", except.what());
        }

        return nullptr;
    }
}