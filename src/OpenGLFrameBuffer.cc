/**
 * OpenGLFrameBuffer.cc
 * Created by kate on 6/23/23.
 * */

// Third-Party Libraries
#include <GL/glew.h>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <Renderer/OpenGL/OpenGLFrameBuffer.hh>

namespace kaTe {

    auto OpenGLFrameBuffer::Bind() -> void {
        glBindFramebuffer(GL_FRAMEBUFFER, m_Id);
        glViewport(0, 0, m_FrameBufferCreateInfo.width, m_FrameBufferCreateInfo.height);
    }

    auto OpenGLFrameBuffer::Unbind() -> void {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    auto OpenGLFrameBuffer::Recreate() -> void {
        if (m_Id != 0) {
            glDeleteFramebuffers(1, &m_Id);
            glDeleteTextures(1, &m_ColorAttachment);
            glDeleteTextures(1, &m_DepthAttachment);
        }

        glCreateFramebuffers(1, &m_Id);
        glBindFramebuffer(GL_FRAMEBUFFER, m_Id);


        glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_FrameBufferCreateInfo.width, m_FrameBufferCreateInfo.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTextureParameteri(m_ColorAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_ColorAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment);
        glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_FrameBufferCreateInfo.width, m_FrameBufferCreateInfo.height, 0,
                     GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

        // Enable Depth testing
        glEnable(GL_DEPTH_TEST);

        // Enable blending and setup blending function
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);  // Default blend equation
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // Default blending factors

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);

        bool result{ glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE };
        KT_ASSERT(result, "FrameBuffer is not complete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    OpenGLFrameBuffer::~OpenGLFrameBuffer() {
        glDeleteFramebuffers(1, &m_Id);
        glDeleteTextures(1, &m_ColorAttachment);
        glDeleteTextures(1, &m_DepthAttachment);
    }

    auto OpenGLFrameBuffer::Resize(UInt32_T width, UInt32_T height) -> void {
        m_FrameBufferCreateInfo.width = width;
        m_FrameBufferCreateInfo.height = height;
        Recreate();
    }

    auto OpenGLFrameBuffer::OnCreate(const FrameBufferCreateInfo& properties) -> void {
        m_FrameBufferCreateInfo = properties;

        Recreate();
    }
}