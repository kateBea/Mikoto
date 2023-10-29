/**
 * Shader.cc
 * Created by kate on 6/16/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include "Core/Logger.hh"
#include "Renderer/Material/Shader.hh"
#include "Renderer/OpenGL/OpenGLShader.hh"
#include "Renderer/Renderer.hh"

namespace Mikoto {

    auto Shader::Create(const Path_T& vertStage, const Path_T& pixelStage) -> std::shared_ptr<Shader> {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLShader>(vertStage, pixelStage);
            default:
                MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }

    auto Shader::Create(const Path_T& src, ShaderStage stage) -> std::shared_ptr<Shader> {
        switch(Renderer::GetActiveGraphicsAPI()) {
        case GraphicsAPI::OPENGL_API:

        case GraphicsAPI::VULKAN_API:

        default:
            MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API");
            return nullptr;
        }
    }
}