// C++ Standard Library
#include <vector>

// Third-Party Libraries
#include <GL/glew.h>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Renderer/OpenGL/OpenGLIndexBuffer.hh>

namespace Mikoto {
    OpenGLIndexBuffer::OpenGLIndexBuffer(const std::vector<UInt32_T>& indices, GLbitfield flags) {
        glCreateBuffers(1, &m_Id);
        m_ValidId = m_Id != 0;

        if (m_ValidId)
            Upload(indices, flags);
        else
            MKT_CORE_LOGGER_ERROR("Failed to create a valid OpenGL element buffer object");
    }

    auto OpenGLIndexBuffer::Upload(const std::vector<UInt32_T>& indices, GLbitfield flags) -> void {
        if (!m_ValidId) {
            glCreateBuffers(1, &m_Id);
            m_ValidId = m_Id != 0;
        }

        if (!indices.empty() && m_ValidId) {
            m_Count = indices.size();
            m_DataType = GL_UNSIGNED_INT; // same type as indices data (adjust in the future for more flexibility)
            glNamedBufferStorage(m_Id, static_cast<GLsizeiptr>(m_Count * sizeof(UInt32_T)), indices.data(), flags);
        }
        else
            MKT_CORE_LOGGER_ERROR("No valid element buffer object to upload data");
    }

    OpenGLIndexBuffer::OpenGLIndexBuffer(OpenGLIndexBuffer&& other) noexcept
        :   IndexBuffer{ other.GetCount() }, m_ValidId{ other.m_ValidId }
    {
        other.m_Id = 0;
        other.m_Count = 0;
        other.m_ValidId = false;
    }

    auto OpenGLIndexBuffer::operator=(OpenGLIndexBuffer&& other) noexcept -> OpenGLIndexBuffer& {
        m_Id = other.GetID();
        m_Count = other.GetCount();
        m_ValidId = other.m_ValidId;

        other.m_Id = 0;
        other.m_Count = 0;
        other.m_ValidId = false;
        return *this;
    }
}