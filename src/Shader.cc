//
// Created by kate on 6/16/23.
//

#include <Core/Logger.hh>

#include <Renderer/Material/Shader.hh>
#include <Renderer/OpenGL/OpenGLShader.hh>
#include <Renderer/Renderer.hh>

namespace Mikoto {

    auto Shader::CreateShader(const Path_T& vertStage, const Path_T& pixelStage) -> std::shared_ptr<Shader> {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case Renderer::GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLShader>(vertStage, pixelStage);
            default:
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}