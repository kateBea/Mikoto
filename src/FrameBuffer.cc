/**
 * FrameBuffer.cc
 * Created by kate on 6/23/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Logger.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Buffers/FrameBuffer.hh>
#include <Renderer/OpenGL/OpenGLFrameBuffer.hh>

namespace Mikoto {

    auto FrameBuffer::CreatFrameBuffer(const FrameBufferCreateInfo &properties) -> std::shared_ptr<FrameBuffer> {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                return std::make_shared<OpenGLFrameBuffer>();
            default:
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }

    auto FrameBuffer::CreatFrameBufferRawPtr(const FrameBufferCreateInfo &properties) -> FrameBuffer* {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                return new OpenGLFrameBuffer();
            default:
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}