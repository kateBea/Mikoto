/**
 * Texture2D.cc
 * Created by kate on 6/8/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Types.hh>
#include <Core/Logger.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Material/Texture2D.hh>
#include <Renderer/OpenGL/OpenGLTexture2D.hh>


namespace Mikoto {
    auto Texture2D::Create(const Path_T &path, Type type) -> std::shared_ptr<Mikoto::Texture2D> {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLTexture2D>(path, type);
            default:
                MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}