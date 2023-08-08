#include <Utility/Common.hh>

#include <Renderer/OpenGL/OpenGLVertexBuffer.hh>

namespace kaTe {
    OpenGLVertexBuffer::OpenGLVertexBuffer(const VertexBufferCreateInfo& createInfo, GLenum usage) noexcept {
        glCreateBuffers(1, &m_Id);
        m_ValidId = m_Id != 0;

        Upload(createInfo.Data, usage);
    }


    OpenGLVertexBuffer::OpenGLVertexBuffer(OpenGLVertexBuffer && other) noexcept
        :   m_Layout{ std::move(other.m_Layout) }, m_ValidId{ other.m_ValidId }
    {
        m_Id = other.GetID();

        other.m_Id = 0;
        other.m_Size = 0;
    }

    auto OpenGLVertexBuffer::operator=(OpenGLVertexBuffer && other) noexcept -> OpenGLVertexBuffer & {
        m_Id = other.GetID();
        m_Size = other.GetSize();

        other.m_Id = 0;
        other.m_Size = 0;
        return *this;
    }

    auto OpenGLVertexBuffer::Upload(const std::vector<float> &vertices, GLenum usage) -> void {
        if (!m_ValidId) {
            glCreateBuffers(1, &m_Id);
            m_ValidId = m_Id != 0;
        }

        if (!vertices.empty()) {
            Bind();
            m_Size = vertices.size() * sizeof(float);
            m_Count = m_Size / sizeof(float);  /* Assumes float for attribute components */
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_Size), vertices.data(), usage);
            Unbind();
        }
    }
}