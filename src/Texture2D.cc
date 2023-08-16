/**
 * Texture2D.cc
 * Created by kate on 6/8/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Material/Texture.hh>
#include <Renderer/Material/Texture2D.hh>
#include <Renderer/OpenGL/OpenGLTexture2D.hh>


namespace Mikoto {

    auto Texture2D::CreateTexture(const Path_T &path) -> std::shared_ptr<Texture> {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLTexture2D>(path);
            default:
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }

    auto Texture2D::CreateTextureRawPtr(const Path_T &path) -> Texture* {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                return new OpenGLTexture2D(path);
            default:
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }

    auto Texture2D::LoadFromFile(const Path_T &path, Texture2D::Type type) -> std::shared_ptr<Mikoto::Texture> {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLTexture2D>(path);
            default:
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}